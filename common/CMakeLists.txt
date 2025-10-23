add_definitions(-DTRANSLATION_DOMAIN="kdisplaypresets_common")

add_library(kdisplaypresets_common OBJECT presets.cpp utils.cpp)

ecm_qt_declare_logging_category(kdisplaypresets_common
    HEADER kdisplaypresets_common_debug.h
    IDENTIFIER KDISPLAYPRESETS_COMMON
    CATEGORY_NAME kdisplaypresets.common
    DESCRIPTION "kdisplaypresets common library (kdisplaypresets)"
    EXPORT KDISPLAYPRESETS
)

set_property(TARGET kdisplaypresets_common PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(kdisplaypresets_common
    PRIVATE
        Qt::Core
        KF6::Screen
        KF6::CoreAddons
        KF6::I18n
)

target_include_directories(kdisplaypresets_common
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
)
