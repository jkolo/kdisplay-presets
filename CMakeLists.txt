cmake_minimum_required(VERSION 3.16)

project(kdisplaypresets)

set(PROJECT_VERSION "1.0.3")
set(PROJECT_VERSION_MAJOR 1)

set(QT_MIN_VERSION 6.8.0)
set(KF_MIN_VERSION 6.5.0)
set(KDE_COMPILERSETTINGS_LEVEL 5.82)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(ECMSetupVersion)
include(ECMGenerateHeaders)
include(ECMQtDeclareLoggingCategory)
include(ECMDeprecationSettings)
include(ECMQmlModule)
include(ECMGenerateDBusServiceFile)
include(FeatureSummary)

ecm_setup_version(${PROJECT_VERSION}
                  VARIABLE_PREFIX KDISPLAYPRESETS
                  VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kdisplaypresets_version.h"
                  PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KDisplayPresetsConfigVersion.cmake"
                  SOVERSION 1)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    Quick
    QuickControls2
    Gui
    DBus
    Test
)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    I18n
    CoreAddons
    Config
    DBusAddons
    XmlGui
    GlobalAccel
    KCMUtils
    Screen
)

find_package(Plasma ${KF_MIN_VERSION} REQUIRED)

find_package(KF6Screen CONFIG REQUIRED)

ecm_set_disabled_deprecation_versions(
    QT 6.7.2
    KF 6.14.0
)

add_subdirectory(common)
add_subdirectory(daemon)
add_subdirectory(kcm)
add_subdirectory(plasmoid)

# Install logging categories
ecm_qt_install_logging_categories(
    EXPORT KDISPLAYPRESETS
    FILE kdisplaypresets.categories
    DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
)

# Install translations
ki18n_install(po)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
