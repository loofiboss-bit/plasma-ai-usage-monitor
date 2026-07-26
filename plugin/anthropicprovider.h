#ifndef ANTHROPICPROVIDER_H
#define ANTHROPICPROVIDER_H

#include "anthropicadminparser.h"
#include "providerbackend.h"

class AnthropicProvider : public ProviderBackend {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
  Q_PROPERTY(
      bool adminApiKeyConfigured READ hasAdminApiKey NOTIFY credentialsChanged)

public:
  enum class AdminCapability { Usage, Cost };

  explicit AnthropicProvider(QObject *parent = nullptr);

  QString name() const override { return QStringLiteral("Anthropic"); }
  QString iconName() const override { return QStringLiteral("anthropic"); }

  QString model() const;
  void setModel(const QString &model);
  Q_INVOKABLE void setAdminApiKey(const QString &key);
  bool hasAdminApiKey() const;

  void refreshImpl() override;
  Q_INVOKABLE void countTokensDiagnostic();

Q_SIGNALS:
  void modelChanged();
  void credentialsChanged();

private:
  void fetchModels(int generation);
  void fetchRateLimits(int generation);
  void fetchAdminPage(AdminCapability capability, const QUrl &url,
                      int generation);
  void handleModelsReply(QNetworkReply *reply, int generation);
  void handleCountTokensReply(QNetworkReply *reply, int generation);
  void handleAdminReply(AdminCapability capability, QNetworkReply *reply,
                        int generation);
  void finishCapability(AdminCapability capability, bool complete,
                        const QString &diagnostic = QString(),
                        ProviderErrorKind errorKind = ProviderErrorKind::None,
                        int httpStatus = 0,
                        const QDateTime &retryAfter = QDateTime());
  void finalizeRefresh(int generation);
  void publishUsage(bool stale);
  void publishCost(bool stale);
  QUrl adminUrl(AdminCapability capability, int days) const;

  QString m_model = QStringLiteral("claude-sonnet-4-20250514");
  QString m_adminApiKey;
  QList<AnthropicAdminParser::UsageRow> m_pendingUsageRows;
  QList<AnthropicAdminParser::CostRow> m_pendingCostRows;
  QList<AnthropicAdminParser::UsageRow> m_lastUsageRows;
  QList<AnthropicAdminParser::CostRow> m_lastCostRows;
  bool m_connectivityPending = false;
  bool m_usagePending = false;
  bool m_costPending = false;
  bool m_connectivitySucceeded = false;
  bool m_usageSucceeded = false;
  bool m_costSucceeded = false;
  bool m_usagePartial = false;
  bool m_costPartial = false;
  int m_usagePages = 0;
  int m_costPages = 0;

  static constexpr int MAX_ADMIN_PAGES = 64;
  static constexpr const char *BASE_URL = "https://api.anthropic.com/v1";
  static constexpr const char *API_VERSION = "2023-06-01";
};

#endif // ANTHROPICPROVIDER_H
