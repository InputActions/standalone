/*
    Input Actions - Input handler that executes user-defined actions
    Copyright (C) 2024-2026 Marcin Woźniak

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <QString>
#include <QUuid>

namespace InputActions
{

class MessageSocketConnection;

/**
 * The first letter is the message's origin (client/server).
 */
enum class MessageType : int
{
    CInitializeRequest,
    SInitializeResponse,

    SActivateRequest,
    CActivateResponse,

    SDeactivateRequest,
    CDeactivateResponse,
};

class Message : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int type MEMBER m_type)

public:
    Message(MessageType type)
        : m_type(static_cast<int>(type))
    {
    }

    virtual ~Message() = default;

    MessageType type() const { return static_cast<MessageType>(m_type); }

    MessageSocketConnection *sender() const { return m_sender; }
    void setSender(MessageSocketConnection *value) { m_sender = value; }

protected:
    MessageSocketConnection *m_sender;

private:
    int m_type;
};

class ResponseMessage : public Message
{
    Q_OBJECT
    Q_PROPERTY(QString requestId MEMBER m_requestId)

public:
    ResponseMessage(MessageType type)
        : Message(type)
    {
    }

    const QString &requestId() const { return m_requestId; }
    void setRequestId(const QString &value) { m_requestId = value; }

private:
    QString m_requestId;

    bool m_success = true;
    QString m_error;
};

class RequestMessageBase : public Message
{
    Q_OBJECT
    Q_PROPERTY(QString requestId MEMBER m_requestId)

public:
    RequestMessageBase(MessageType type)
        : Message(type)
    {
    }

    const QString &requestId() const { return m_requestId; }

protected:
    void sendResponse(const ResponseMessage &response) const;

private:
    QString m_requestId = QUuid::createUuid().toString();
};

template<typename TResponse>
class RequestMessage : public RequestMessageBase
{
public:
    using RequestMessageBase::RequestMessageBase;

    TResponse makeResponse() const { return {}; }

    void reply() const
    {
        TResponse response;
        reply(response);
    }

    void reply(TResponse &response) const
    {
        response.setRequestId(requestId());
        sendResponse(response);
    }
};

class SInitializeResponseMessage : public ResponseMessage
{
    Q_OBJECT
    Q_PROPERTY(bool success MEMBER m_success)
    Q_PROPERTY(QString error MEMBER m_error)

public:
    SInitializeResponseMessage()
        : ResponseMessage(MessageType::SInitializeResponse)
    {
    }

    bool success() const { return m_success; }
    void setSuccess(bool value) { m_success = value; }

    const QString &error() const { return m_error; }
    void setError(QString value) { m_error = std::move(value); }

private:
    bool m_success{};
    QString m_error;
};

class CInitializeRequestMessage : public RequestMessage<SInitializeResponseMessage>
{
    Q_OBJECT
    Q_PROPERTY(QString tty MEMBER m_tty)
    Q_PROPERTY(int64_t mainThreadId MEMBER m_mainThreadId)

public:
    CInitializeRequestMessage()
        : RequestMessage(MessageType::CInitializeRequest)
    {
    }

    const QString &tty() const { return m_tty; }
    void setTty(const QString &value) { m_tty = value; }

    int64_t mainThreadId() const { return m_mainThreadId; }
    void setMainThreadId(int64_t value) { m_mainThreadId = value; }

private:
    QString m_tty;
    int64_t m_mainThreadId;
};

class CActivateResponseMessage : public ResponseMessage
{
    Q_OBJECT

public:
    CActivateResponseMessage()
        : ResponseMessage(MessageType::CActivateResponse)
    {
    }
};

class SActivateRequestMessage : public RequestMessage<CActivateResponseMessage>
{
    Q_OBJECT

public:
    SActivateRequestMessage()
        : RequestMessage(MessageType::SActivateRequest)
    {
    }
};

class CDeactivateResponseMessage : public ResponseMessage
{
    Q_OBJECT

public:
    CDeactivateResponseMessage()
        : ResponseMessage(MessageType::CDeactivateResponse)
    {
    }
};

class SDeactivateRequestMessage : public RequestMessage<CDeactivateResponseMessage>
{
    Q_OBJECT

public:
    SDeactivateRequestMessage()
        : RequestMessage(MessageType::SDeactivateRequest)
    {
    }
};

}