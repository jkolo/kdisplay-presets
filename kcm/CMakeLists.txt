add_definitions(-DTRANSLATION_DOMAIN=\"kcm_displaypresets\")

kcmutils_add_qml_kcm(kcm_displaypresets)

ecm_qt_declare_logging_category(kcm_displaypresets
    HEADER kdisplaypresets_kcm_debug.h
    IDENTIFIER KDISPLAYPRESETS_KCM
    CATEGORY_NAME kdisplaypresets.kcm
    DESCRIPTION "kdisplaypresets kcm (kdisplaypresets)"
    EXPORT KDISPLAYPRESETS
)

target_sources(kcm_displaypresets PRIVATE
    kcm_displaypresets.cpp
    preset_manager.cpp
)

target_link_libraries(kcm_displaypresets PRIVATE
    kdisplaypresets_common
    Qt::Core
    Qt::Quick
    Qt::DBus
    KF6::I18n
    KF6::Screen
    KF6::CoreAddons
    KF6::KCMUtils
    KF6::KCMUtilsQuick
)

# No settings configuration needed for this KCM

# Install QML files
install(FILES
    ui/main.qml
    ui/PresetView.qml
    ui/PresetOutput.qml
    DESTINATION ${KDE_INSTALL_DATADIR}/kcm_displaypresets/qml
)
