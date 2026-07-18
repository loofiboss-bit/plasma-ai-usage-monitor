set(aiusagemonitor_SRCS
    aiusageplugin.cpp
    appinfo.cpp
    catalogloader.cpp
    providerpricingcatalog.cpp
    subscriptionplancatalog.cpp
    secretsmanager.cpp
    providerbackend.cpp
    descriptorprovider.cpp
    providermanager.cpp
    sourcereadinessmodel.cpp
    refreshschedulermodel.cpp
    openaicompatibleprovider.cpp
    openaiprovider.cpp
    azureopenaiprovider.cpp
    anthropicprovider.cpp
    googleprovider.cpp
    mistralprovider.cpp
    deepseekprovider.cpp
    groqprovider.cpp
    xaiprovider.cpp
    ollamacloudprovider.cpp
    openrouterprovider.cpp
    togetherprovider.cpp
    cohereprovider.cpp
    googleveoprovider.cpp
    usagedatabase.cpp
    updatechecker.cpp
    subscriptiontoolbackend.cpp
    localactivitymonitorbase.cpp
    claudecodemonitor.cpp
    codexclimonitor.cpp
    copilotmonitor.cpp
    cursormonitor.cpp
    windsurfmonitor.cpp
    jetbrainsaimonitor.cpp
    browsercookieextractor.cpp
    browsersyncservice.cpp
    awssigv4signer.cpp
    bedrockprovider.cpp
    localmetricsserver.cpp
    webhooknotifier.cpp
)

set(aiusagemonitor_HDRS
    aiusageplugin.h
    appinfo.h
    catalogloader.h
    providerpricingcatalog.h
    subscriptionplancatalog.h
    secretsmanager.h
    providerbackend.h
    descriptorprovider.h
    providermanager.h
    sourcereadinessmodel.h
    refreshschedulermodel.h
    openaicompatibleprovider.h
    openaiprovider.h
    azureopenaiprovider.h
    anthropicprovider.h
    googleprovider.h
    mistralprovider.h
    deepseekprovider.h
    groqprovider.h
    xaiprovider.h
    ollamacloudprovider.h
    openrouterprovider.h
    togetherprovider.h
    cohereprovider.h
    googleveoprovider.h
    usagedatabase.h
    clipboardhelper.h
    updatechecker.h
    subscriptiontoolbackend.h
    localactivitymonitorbase.h
    claudecodemonitor.h
    codexclimonitor.h
    copilotmonitor.h
    cursormonitor.h
    windsurfmonitor.h
    jetbrainsaimonitor.h
    browsercookieextractor.h
    browsersyncservice.h
    awssigv4signer.h
    bedrockprovider.h
    localmetricsserver.h
    webhooknotifier.h
)

qt_add_qml_module(aiusagemonitorplugin
    URI com.github.loofi.aiusagemonitor
    VERSION 1.0
    CLASS_NAME AiUsagePlugin
    PLUGIN_TARGET aiusagemonitorplugin
    NO_GENERATE_PLUGIN_SOURCE
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/com/github/loofi/aiusagemonitor
    SOURCES ${aiusagemonitor_SRCS} ${aiusagemonitor_HDRS}
)

# QQmlExtensionPlugin resolves native plugins as lib<name>.so on Linux.  Keep
# the conventional prefix explicit because qt_add_qml_module() otherwise
# produced aiusagemonitorplugin.so in Fedora packages, which the runtime could
# not discover from qmldir.
set_target_properties(aiusagemonitorplugin PROPERTIES PREFIX "lib")

target_compile_definitions(aiusagemonitorplugin
    PRIVATE AIUSAGE_MONITOR_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(aiusagemonitorplugin
    PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    OpenSSL::Crypto
    KF6::Wallet
    KF6::Notifications
    KF6::I18n
    ${CMAKE_DL_LIBS}
)

install(TARGETS aiusagemonitorplugin DESTINATION ${KDE_INSTALL_QMLDIR}/com/github/loofi/aiusagemonitor)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/com/github/loofi/aiusagemonitor/qmldir
              ${CMAKE_CURRENT_BINARY_DIR}/com/github/loofi/aiusagemonitor/aiusagemonitorplugin.qmltypes
        DESTINATION ${KDE_INSTALL_QMLDIR}/com/github/loofi/aiusagemonitor)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
