add_executable(kdisplaypresets_daemon
    main.cpp
    presetsservice.cpp
    presetsservice.h
)

ecm_qt_declare_logging_category(kdisplaypresets_daemon
    HEADER
        kdisplaypresets_daemon_debug.h
    IDENTIFIER
        KDISPLAYPRESETS_DAEMON
    CATEGORY_NAME
        kdisplaypresets.daemon
    DESCRIPTION "kdisplaypresets daemon (kdisplaypresets)"
    EXPORT KDISPLAYPRESETS
)

target_link_libraries(kdisplaypresets_daemon PRIVATE
    kdisplaypresets_common
    Qt::Core
    Qt::Gui
    Qt::DBus
    KF6::Screen
    KF6::I18n
    KF6::GlobalAccel
)

install(TARGETS kdisplaypresets_daemon DESTINATION ${KDE_INSTALL_LIBEXECDIR})

ecm_generate_dbus_service_file(
    NAME org.kde.kdisplaypresets
    EXECUTABLE ${KDE_INSTALL_FULL_LIBEXECDIR}/kdisplaypresets_daemon
    DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR}
    SYSTEMD_SERVICE plasma-kdisplaypresets.service
)

configure_file(plasma-kdisplaypresets.service plasma-kdisplaypresets.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/plasma-kdisplaypresets.service
    DESTINATION ${KDE_INSTALL_SYSTEMDUSERUNITDIR}
)
