add_definitions(-DTRANSLATION_DOMAIN=\"plasma_applet_org.kde.kdisplaypresets\")

ecm_qt_declare_logging_category(org.kde.kdisplaypresets
  HEADER kdisplaypresets_applet_debug.h
  IDENTIFIER KDISPLAYPRESETS_APPLET
  CATEGORY_NAME kdisplaypresets.applet
  DESCRIPTION "kdisplaypresets plasmoid (kdisplaypresets)"
  EXPORT KDISPLAYPRESETS
)

plasma_add_applet(org.kde.kdisplaypresets
  QML_SOURCES
    main.qml
    PresetList.qml
    PresetView.qml
    PresetOutput.qml
  CPP_SOURCES
    kdisplaypresets_applet.cpp
    kdisplaypresets_applet_debug.cpp
)

target_link_libraries(org.kde.kdisplaypresets PRIVATE
                      Qt::Qml
                      Qt::DBus
                      KF6::I18n
                      Plasma::Plasma
                      )
