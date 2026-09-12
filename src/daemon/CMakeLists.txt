add_executable(inputactionsd
    main.cpp
    Server.cpp
    ServerHandler.cpp
)
target_compile_options(inputactionsd PUBLIC -fexceptions)
target_link_libraries(inputactionsd PRIVATE
    libinputactions-standalone-common
)

install(
    TARGETS inputactionsd
    COMPONENT standalone
)

if(INPUTACTIONS_SYSTEMD)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/inputactionsd.service.in ${CMAKE_CURRENT_BINARY_DIR}/inputactionsd.service @ONLY)
    pkg_get_variable(SYSTEMD_SERVICE_DIR systemd systemdsystemunitdir)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/inputactionsd.service DESTINATION ${SYSTEMD_SERVICE_DIR})
endif()