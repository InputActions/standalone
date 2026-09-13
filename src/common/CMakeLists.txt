find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Network
)

add_library(libinputactions-standalone-common STATIC
    libinputactions-standalone-common/helpers/Session.cpp
    libinputactions-standalone-common/ipc/JsonSerializer.cpp
    libinputactions-standalone-common/ipc/messages.cpp
    libinputactions-standalone-common/ipc/MessageSocketConnection.cpp
)
target_link_libraries(libinputactions-standalone-common PUBLIC
    libinputactions
    Qt6::Network
)
target_include_directories(libinputactions-standalone-common PUBLIC libinputactions-standalone-common)
set_target_properties(libinputactions-standalone-common PROPERTIES PREFIX "")
