qt_add_executable(VoStudio
    WIN32
    MACOSX_BUNDLE
    main.cpp
    AudioController.cpp
    AudioController.hpp
)

qt_add_qml_module(VoStudio
    URI VoStudio
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

target_link_libraries(VoStudio PRIVATE
    VoStudio::Engine
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    Qt6::Multimedia
    Qt6::Concurrent
)

target_compile_features(VoStudio PRIVATE cxx_std_20)
set_target_properties(VoStudio PROPERTIES
    OUTPUT_NAME VoStudio
    MACOSX_BUNDLE_GUI_IDENTIFIER org.waruta.vostudio
    MACOSX_BUNDLE_BUNDLE_NAME VoStudio
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
)

if(ANDROID)
    # Keep the CMake target name "VoStudio" so Qt 6.8 uses it as the
    # generated Android launcher label without relying on QT_ANDROID_APP_NAME
    # (that target property was added in Qt 6.9).
    set_target_properties(VoStudio PROPERTIES
        QT_ANDROID_PACKAGE_NAME "org.waruta.vostudio"
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 35
        QT_ANDROID_VERSION_CODE 1
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_ABIS "arm64-v8a"
    )
elseif(APPLE AND CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # Device builds are intentionally unsigned in CI. A provisioning profile
    # and signing identity are required before installing on a physical device.
    set_target_properties(VoStudio PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
    )
else()
    include(GNUInstallDirs)
    install(TARGETS VoStudio
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    qt_generate_deploy_qml_app_script(
        TARGET VoStudio
        OUTPUT_SCRIPT vostudio_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${vostudio_deploy_script})
endif()
