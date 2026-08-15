#ifndef CATALOGLOADER_H
#define CATALOGLOADER_H

#include <QJsonObject>
#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>

class CatalogLoader : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool valid READ isValid NOTIFY statusChanged)
    Q_PROPERTY(bool stale READ stale NOTIFY statusChanged)
    Q_PROPERTY(int schemaVersion READ schemaVersion NOTIFY statusChanged)
    Q_PROPERTY(QString catalogVersion READ catalogVersion NOTIFY statusChanged)
    Q_PROPERTY(QString lastReviewed READ lastReviewed NOTIFY statusChanged)
    Q_PROPERTY(bool runtimeScraping READ runtimeScraping NOTIFY statusChanged)
    Q_PROPERTY(int manualReviewCount READ manualReviewCount NOTIFY statusChanged)
    Q_PROPERTY(int sourceConflictCount READ sourceConflictCount NOTIFY statusChanged)
    Q_PROPERTY(QVariantList reviewItems READ reviewItems NOTIFY statusChanged)
    Q_PROPERTY(QString catalogPath READ catalogPath NOTIFY statusChanged)
    Q_PROPERTY(QString sourceFingerprint READ sourceFingerprint NOTIFY statusChanged)
    Q_PROPERTY(QString verificationState READ verificationState NOTIFY statusChanged)
    Q_PROPERTY(qint64 sequence READ sequence NOTIFY statusChanged)
    Q_PROPERTY(QString hardExpiresAt READ hardExpiresAt NOTIFY statusChanged)
    Q_PROPERTY(bool estimatesAllowed READ estimatesAllowed NOTIFY statusChanged)
    Q_PROPERTY(int freshnessSloDays READ freshnessSloDays NOTIFY statusChanged)
    Q_PROPERTY(QStringList diagnostics READ diagnostics NOTIFY statusChanged)

public:
    explicit CatalogLoader(QString fileName, int expectedSchemaVersion, QObject *parent = nullptr);

    Q_INVOKABLE bool load();
    Q_INVOKABLE bool isStale(int maxAgeDays) const;

    bool isValid() const;
    bool stale() const;
    int schemaVersion() const;
    QString catalogVersion() const;
    QString lastReviewed() const;
    bool runtimeScraping() const;
    int manualReviewCount() const;
    int sourceConflictCount() const;
    QVariantList reviewItems() const;
    QString catalogPath() const;
    QString sourceFingerprint() const;
    QString verificationState() const;
    qint64 sequence() const;
    QString hardExpiresAt() const;
    bool estimatesAllowed() const;
    int freshnessSloDays() const;
    QStringList diagnostics() const;

Q_SIGNALS:
    void statusChanged();

protected:
    bool ensureLoaded() const;
    QJsonObject rootObject() const;
    static QString locateCatalogFile(const QString &fileName);

private:
    void countReviewFlags();

    QString m_fileName;
    int m_expectedSchemaVersion = 0;
    mutable bool m_loadAttempted = false;
    bool m_valid = false;
    int m_schemaVersion = 0;
    QString m_catalogVersion;
    QString m_lastReviewed;
    bool m_runtimeScraping = false;
    int m_manualReviewCount = 0;
    int m_sourceConflictCount = 0;
    QVariantList m_reviewItems;
    QString m_catalogPath;
    QString m_sourceFingerprint;
    QString m_verificationState;
    qint64 m_sequence = 0;
    QString m_hardExpiresAt;
    bool m_estimatesAllowed = true;
    int m_freshnessSloDays = 30;
    QStringList m_diagnostics;
    QJsonObject m_root;
};

#endif // CATALOGLOADER_H
