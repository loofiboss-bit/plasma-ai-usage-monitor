import QtQuick
import com.github.loofi.aiusagemonitor 1.0

QtObject {
    id: catalog

    readonly property int schemaVersion: ProviderPricingCatalog.schemaVersion
    readonly property string catalogVersion: ProviderPricingCatalog.catalogVersion
    readonly property string lastReviewed: ProviderPricingCatalog.lastReviewed
    readonly property bool runtimeScraping: ProviderPricingCatalog.runtimeScraping

    readonly property var providers: [
        {
            name: "OpenAI",
            label: i18n("OpenAI"),
            dbName: "OpenAI",
            configKey: "openai",
            color: "#10A37F",
            requiresApiKey: true,
            supportsBudget: true,
            refreshConfigKey: "openaiRefreshInterval",
            notificationsConfigKey: "openaiNotificationsEnabled",
            dailyBudgetConfigKey: "openaiDailyBudget",
            monthlyBudgetConfigKey: "openaiMonthlyBudget"
        },
        {
            name: "Anthropic",
            label: i18n("Anthropic"),
            dbName: "Anthropic",
            configKey: "anthropic",
            color: "#D4A574",
            requiresApiKey: true,
            supportsBudget: true,
            refreshConfigKey: "anthropicRefreshInterval",
            notificationsConfigKey: "anthropicNotificationsEnabled",
            dailyBudgetConfigKey: "anthropicDailyBudget",
            monthlyBudgetConfigKey: "anthropicMonthlyBudget"
        },
        {
            name: "Google Gemini",
            label: i18n("Google Gemini"),
            dbName: "Google",
            configKey: "google",
            color: "#4285F4",
            requiresApiKey: true,
            supportsBudget: true,
            refreshConfigKey: "googleRefreshInterval",
            notificationsConfigKey: "googleNotificationsEnabled",
            dailyBudgetConfigKey: "googleDailyBudget",
            monthlyBudgetConfigKey: "googleMonthlyBudget"
        },
        {
            name: "Mistral AI",
            label: i18n("Mistral AI"),
            dbName: "Mistral",
            configKey: "mistral",
            color: "#FF7000",
            requiresApiKey: true,
            supportsBudget: true,
            refreshConfigKey: "mistralRefreshInterval",
            notificationsConfigKey: "mistralNotificationsEnabled",
            dailyBudgetConfigKey: "mistralDailyBudget",
            monthlyBudgetConfigKey: "mistralMonthlyBudget"
        },
        {
            name: "DeepSeek",
            label: i18n("DeepSeek"),
            dbName: "DeepSeek",
            configKey: "deepseek",
            color: "#5B6EE1",
            requiresApiKey: true,
            supportsBudget: true,
            refreshConfigKey: "deepseekRefreshInterval",
            notificationsConfigKey: "deepseekNotificationsEnabled",
            dailyBudgetConfigKey: "deepseekDailyBudget",
            monthlyBudgetConfigKey: "deepseekMonthlyBudget"
        },
        {
            name: "Groq",
            label: i18n("Groq"),
            dbName: "Groq",
            configKey: "groq",
            color: "#F55036",
            requiresApiKey: true,
            supportsBudget: true,
            refreshConfigKey: "groqRefreshInterval",
            notificationsConfigKey: "groqNotificationsEnabled",
            dailyBudgetConfigKey: "groqDailyBudget",
            monthlyBudgetConfigKey: "groqMonthlyBudget"
        },
        {
            name: "xAI / Grok",
            label: i18n("xAI / Grok"),
            dbName: "xAI",
            configKey: "xai",
            color: "#1DA1F2",
            requiresApiKey: true,
            supportsBudget: true,
            refreshConfigKey: "xaiRefreshInterval",
            notificationsConfigKey: "xaiNotificationsEnabled",
            dailyBudgetConfigKey: "xaiDailyBudget",
            monthlyBudgetConfigKey: "xaiMonthlyBudget"
        },
        {
            name: "Ollama Cloud",
            label: i18n("Ollama Cloud"),
            dbName: "OllamaCloud",
            configKey: "ollama",
            color: "#111827",
            requiresApiKey: true,
            supportsBudget: true,
            refreshConfigKey: "ollamaRefreshInterval",
            notificationsConfigKey: "ollamaNotificationsEnabled",
            dailyBudgetConfigKey: "ollamaDailyBudget",
            monthlyBudgetConfigKey: "ollamaMonthlyBudget"
        },
        {
            name: "OpenRouter",
            label: i18n("OpenRouter"),
            dbName: "OpenRouter",
            configKey: "openrouter",
            color: "#6366F1",
            requiresApiKey: true,
            supportsBudget: true,
            refreshConfigKey: "openrouterRefreshInterval",
            notificationsConfigKey: "openrouterNotificationsEnabled",
            dailyBudgetConfigKey: "openrouterDailyBudget",
            monthlyBudgetConfigKey: "openrouterMonthlyBudget"
        },
        {
            name: "Together AI",
            label: i18n("Together AI"),
            dbName: "Together",
            configKey: "together",
            color: "#0EA5E9",
            requiresApiKey: true,
            supportsBudget: true,
            refreshConfigKey: "togetherRefreshInterval",
            notificationsConfigKey: "togetherNotificationsEnabled",
            dailyBudgetConfigKey: "togetherDailyBudget",
            monthlyBudgetConfigKey: "togetherMonthlyBudget"
        },
        {
            name: "Cohere",
            label: i18n("Cohere"),
            dbName: "Cohere",
            configKey: "cohere",
            color: "#39D353",
            requiresApiKey: true,
            supportsBudget: true,
            refreshConfigKey: "cohereRefreshInterval",
            notificationsConfigKey: "cohereNotificationsEnabled",
            dailyBudgetConfigKey: "cohereDailyBudget",
            monthlyBudgetConfigKey: "cohereMonthlyBudget"
        },
        {
            name: "Google Veo",
            label: i18n("Google Veo"),
            dbName: "GoogleVeo",
            configKey: "googleveo",
            color: "#EA4335",
            requiresApiKey: true,
            supportsBudget: true,
            refreshConfigKey: "googleveoRefreshInterval",
            notificationsConfigKey: "googleveoNotificationsEnabled",
            dailyBudgetConfigKey: "googleveoDailyBudget",
            monthlyBudgetConfigKey: "googleveoMonthlyBudget"
        },
        {
            name: "Azure OpenAI",
            label: i18n("Azure OpenAI"),
            dbName: "AzureOpenAI",
            configKey: "azure",
            color: "#0078D4",
            requiresApiKey: true,
            supportsBudget: true,
            refreshConfigKey: "azureRefreshInterval",
            notificationsConfigKey: "azureNotificationsEnabled",
            dailyBudgetConfigKey: "azureDailyBudget",
            monthlyBudgetConfigKey: "azureMonthlyBudget"
        },
        {
            name: "AWS Bedrock",
            label: i18n("AWS Bedrock"),
            dbName: "AWS Bedrock",
            configKey: "bedrock",
            color: "#ff9900",
            requiresApiKey: true,
            supportsBudget: true,
            refreshConfigKey: "bedrockRefreshInterval",
            notificationsConfigKey: "bedrockNotificationsEnabled",
            dailyBudgetConfigKey: "bedrockDailyBudget",
            monthlyBudgetConfigKey: "bedrockMonthlyBudget"
        }
    ]

    readonly property var budgetProviders: providers.filter(function(provider) {
        return provider.supportsBudget;
    })
}
