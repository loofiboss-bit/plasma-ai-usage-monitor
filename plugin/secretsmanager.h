#ifndef SECRETSMANAGER_H
#define SECRETSMANAGER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QHash>
#include <KWallet>
#include <QtQmlIntegration/qqmlintegration.h>

/**
 * SecretsManager wraps KWallet to securely store and retrieve API keys.
 * Exposed to QML so configuration pages can store keys without plaintext.
 */
class SecretsManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool walletOpen READ isWalletOpen NOTIFY walletOpenChanged)
    Q_PROPERTY(int secretReadCount READ secretReadCount NOTIFY diagnosticsChanged)
    Q_PROPERTY(bool demoIsolated READ isDemoIsolated CONSTANT)

public:
    explicit SecretsManager(QObject *parent = nullptr);
    SecretsManager(QObject *parent, bool openWalletOnConstruction);
    ~SecretsManager() override;

    bool isWalletOpen() const;
    int secretReadCount() const;
    bool isDemoIsolated() const;

    Q_INVOKABLE bool storeKey(const QString &provider, const QString &key);
    Q_INVOKABLE QString getKey(const QString &provider);
    Q_INVOKABLE bool removeKey(const QString &provider);
    Q_INVOKABLE bool hasKey(const QString &provider);
    Q_INVOKABLE void retryOpenWallet();

Q_SIGNALS:
    void walletOpenChanged();
    void keyStored(const QString &provider);
    void keyRemoved(const QString &provider);
    void error(const QString &message);
    void secretAvailabilityChanged(const QString &provider, bool available);
    void diagnosticsChanged();

private Q_SLOTS:
    void onWalletOpened(bool success);

private:
    void openWallet();
    bool ensureFolder();
    void warmCache();

    KWallet::Wallet *m_wallet = nullptr;
    bool m_walletOpen = false;
    bool m_demoIsolated = false;
    QString m_folderName = QStringLiteral("ai-usage-monitor");
    QHash<QString, QString> m_secretCache;
    int m_secretReadCount = 0;
};

#endif // SECRETSMANAGER_H
