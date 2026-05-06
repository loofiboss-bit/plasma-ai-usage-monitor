set(aiusagemonitor_SRCS
    aiusageplugin.cpp
    appinfo.cpp
    catalogloader.cpp
    providerpricingcatalog.cpp
    subscriptionplancatalog.cpp
    secretsmanager.cpp
    providerbackend.cpp
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
    awssigv4signer.h
    bedrockprovider.h
    localmetricsserver.h
    webhooknotifier.h
)

add_library(aiusagemonitorplugin SHARED ${aiusagemonitor_SRCS} ${aiusagemonitor_HDRS})

target_compile_definitions(aiusagemonitorplugin
    PRIVATE AIUSAGE_MONITOR_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(aiusagemonitorplugin
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
    Qt6::Sql
    OpenSSL::Crypto
    KF6::Wallet
    KF6::Notifications
    KF6::I18n
)

install(TARGETS aiusagemonitorplugin DESTINATION ${KDE_INSTALL_QMLDIR}/com/github/loofi/aiusagemonitor)
install(FILES qmldir DESTINATION ${KDE_INSTALL_QMLDIR}/com/github/loofi/aiusagemonitor)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
