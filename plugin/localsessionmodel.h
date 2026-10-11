#ifndef LOCALSESSIONMODEL_H
#define LOCALSESSIONMODEL_H

#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>
#include <atomic>
#include <memory>

class LocalSessionModel : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(bool codexEnabled READ codexEnabled NOTIFY enabledChanged)
  Q_PROPERTY(
      bool claudeCodeEnabled READ claudeCodeEnabled NOTIFY enabledChanged)
  Q_PROPERTY(QString status READ status NOTIFY statusChanged)
  Q_PROPERTY(
      int pendingWorkerCount READ pendingWorkerCount NOTIFY statusChanged)
  Q_PROPERTY(int retentionDays READ retentionDays WRITE setRetentionDays NOTIFY
                 retentionDaysChanged)
public:
  explicit LocalSessionModel(QObject *parent = nullptr);
  ~LocalSessionModel() override;
  bool codexEnabled() const { return m_codexEnabled; }
  bool claudeCodeEnabled() const { return m_claudeEnabled; }
  QString status() const { return m_status; }
  int pendingWorkerCount() const { return m_pending; }
  int retentionDays() const { return m_retentionDays; }
  void setRetentionDays(int days);
  Q_INVOKABLE void setToolEnabled(const QString &toolId, bool enabled);
  Q_INVOKABLE void requestImport();
  Q_INVOKABLE void requestSessions(const QString &requestId,
                                   const QString &toolId,
                                   const QDateTime &fromUtc,
                                   const QDateTime &toUtc,
                                   const QString &projectId = {},
                                   int offset = 0, int limit = 100);
  Q_INVOKABLE void requestSummary(const QString &requestId,
                                  const QString &toolId,
                                  const QDateTime &fromUtc,
                                  const QDateTime &toUtc,
                                  const QString &projectId = {});
  Q_INVOKABLE void
  requestDaily(const QString &requestId, const QString &toolId,
               const QDateTime &fromUtc, const QDateTime &toUtc,
               const QString &projectId = {},
               const QString &timeZoneId = QStringLiteral("UTC"));
  Q_INVOKABLE void
  requestExport(const QString &requestId, const QString &toolId,
                const QDateTime &fromUtc, const QDateTime &toUtc,
                const QString &projectId, const QString &format,
                const QString &destinationPath);
  Q_INVOKABLE bool setProjectAlias(const QString &projectId,
                                   const QString &alias);
  Q_INVOKABLE QString projectAlias(const QString &projectId) const;
  Q_INVOKABLE void clearSessions();
  Q_INVOKABLE void clearSessions(const QString &requestId);
  Q_INVOKABLE void requestProjects(const QString &requestId);
signals:
  void enabledChanged();
  void statusChanged();
  void retentionDaysChanged();
  void sessionsReady(const QString &requestId, const QVariantList &rows);
  void summaryReady(const QString &requestId, const QVariantMap &summary);
  void dailyReady(const QString &requestId, const QVariantList &rows);
  void exportReady(const QString &requestId, const QString &errorKey);
  void projectsReady(const QString &requestId, const QVariantList &rows);
  void clearFinished(const QString &requestId, bool ok,
                     const QString &errorKey);
  void importFinished(const QString &toolId, int importedCount, bool partial,
                      const QString &errorKey);

private:
  QTimer *m_importTimer = nullptr;
  bool m_importAgain = false;
  bool m_codexEnabled = false;
  bool m_claudeEnabled = false;
  int m_pending = 0;
  QString m_status = QStringLiteral("disabled");
  int m_retentionDays = 30;
  quint64 m_generation = 0;
  std::shared_ptr<std::atomic<quint64>> m_generationState =
      std::make_shared<std::atomic<quint64>>(0);
};

#endif
