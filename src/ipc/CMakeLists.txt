find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Network
)

add_library(libinputactions-standalone-ipc STATIC
    libinputactions-standalone-ipc/JsonSerializer.cpp
    libinputactions-standalone-ipc/MessageHandler.cpp
    libinputactions-standalone-ipc/messages.cpp
    libinputactions-standalone-ipc/MessageSocketConnection.cpp
)
target_link_libraries(libinputactions-standalone-ipc PUBLIC
    libinputactions
    Qt6::Network
)
target_include_directories(libinputactions-standalone-ipc PUBLIC libinputactions-standalone-ipc)
set_target_properties(libinputactions-standalone-ipc PROPERTIES PREFIX "")
