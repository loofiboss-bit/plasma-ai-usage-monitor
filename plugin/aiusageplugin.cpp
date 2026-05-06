#include "aiusageplugin.h"
#include "appinfo.h"
#include "providerpricingcatalog.h"
#include "subscriptionplancatalog.h"
#include "secretsmanager.h"
#include "providerbackend.h"
#include "openaiprovider.h"
#include "azureopenaiprovider.h"
#include "anthropicprovider.h"
#include "googleprovider.h"
#include "mistralprovider.h"
#include "deepseekprovider.h"
#include "groqprovider.h"
#include "xaiprovider.h"
#include "ollamacloudprovider.h"
#include "openrouterprovider.h"
#include "togetherprovider.h"
#include "cohereprovider.h"
#include "googleveoprovider.h"
#include "usagedatabase.h"
#include "clipboardhelper.h"
#include "updatechecker.h"
#include "subscriptiontoolbackend.h"
#include "claudecodemonitor.h"
#include "codexclimonitor.h"
#include "copilotmonitor.h"
#include "cursormonitor.h"
#include "windsurfmonitor.h"
#include "jetbrainsaimonitor.h"
#include "browsercookieextractor.h"
#include "bedrockprovider.h"
#include "localmetricsserver.h"
#include "webhooknotifier.h"

#include <QQmlEngine>
#include <QJSEngine>

void AiUsagePlugin::registerTypes(const char *uri)
{
    Q_ASSERT(QLatin1String(uri) == QLatin1String("com.github.loofi.aiusagemonitor"));

    qmlRegisterSingletonType<AppInfo>(uri, 1, 0, "AppInfo",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new AppInfo();
        });
    qmlRegisterSingletonType<ProviderPricingCatalog>(uri, 1, 0, "ProviderPricingCatalog",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new ProviderPricingCatalog();
        });
    qmlRegisterSingletonType<SubscriptionPlanCatalog>(uri, 1, 0, "SubscriptionPlanCatalog",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new SubscriptionPlanCatalog();
        });

    // Register C++ types for use in QML
    qmlRegisterType<SecretsManager>(uri, 1, 0, "SecretsManager");
    qmlRegisterType<OpenAIProvider>(uri, 1, 0, "OpenAIProvider");
    qmlRegisterType<AzureOpenAIProvider>(uri, 1, 0, "AzureOpenAIProvider");
    qmlRegisterType<AnthropicProvider>(uri, 1, 0, "AnthropicProvider");
    qmlRegisterType<GoogleProvider>(uri, 1, 0, "GoogleProvider");
    qmlRegisterType<MistralProvider>(uri, 1, 0, "MistralProvider");
    qmlRegisterType<DeepSeekProvider>(uri, 1, 0, "DeepSeekProvider");
    qmlRegisterType<GroqProvider>(uri, 1, 0, "GroqProvider");
    qmlRegisterType<XAIProvider>(uri, 1, 0, "XAIProvider");
    qmlRegisterType<OllamaCloudProvider>(uri, 1, 0, "OllamaCloudProvider");
    qmlRegisterType<OpenRouterProvider>(uri, 1, 0, "OpenRouterProvider");
    qmlRegisterType<TogetherProvider>(uri, 1, 0, "TogetherProvider");
    qmlRegisterType<CohereProvider>(uri, 1, 0, "CohereProvider");
    qmlRegisterType<GoogleVeoProvider>(uri, 1, 0, "GoogleVeoProvider");
    qmlRegisterType<UsageDatabase>(uri, 1, 0, "UsageDatabase");
    qmlRegisterType<ClipboardHelper>(uri, 1, 0, "ClipboardHelper");
    qmlRegisterType<UpdateChecker>(uri, 1, 0, "UpdateChecker");

    // Subscription tool monitors
    qmlRegisterType<ClaudeCodeMonitor>(uri, 1, 0, "ClaudeCodeMonitor");
    qmlRegisterType<CodexCliMonitor>(uri, 1, 0, "CodexCliMonitor");
    qmlRegisterType<CopilotMonitor>(uri, 1, 0, "CopilotMonitor");
    qmlRegisterType<CursorMonitor>(uri, 1, 0, "CursorMonitor");
    qmlRegisterType<WindsurfMonitor>(uri, 1, 0, "WindsurfMonitor");
    qmlRegisterType<JetBrainsAiMonitor>(uri, 1, 0, "JetBrainsAiMonitor");

    // Browser cookie extraction for sync
    qmlRegisterType<BrowserCookieExtractor>(uri, 1, 0, "BrowserCookieExtractor");

    qmlRegisterType<BedrockProvider>(uri, 1, 0, "BedrockProvider");
    qmlRegisterType<LocalMetricsServer>(uri, 1, 0, "LocalMetricsServer");
    qmlRegisterType<WebhookNotifier>(uri, 1, 0, "WebhookNotifier");

    // Register abstract base classes as uncreatable (for type info in QML)
    qmlRegisterUncreatableType<ProviderBackend>(uri, 1, 0, "ProviderBackend",
        QStringLiteral("ProviderBackend is abstract; use a specific provider type."));
    qmlRegisterUncreatableType<SubscriptionToolBackend>(uri, 1, 0, "SubscriptionToolBackend",
        QStringLiteral("SubscriptionToolBackend is abstract; use a specific monitor type."));
}
