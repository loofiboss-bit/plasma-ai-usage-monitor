#ifndef SECRETSMANAGER_H
#define SECRETSMANAGER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <KWallet>

/**
 * SecretsManager wraps KWallet to securely store and retrieve API keys.
 * Exposed to QML so configuration pages can store keys without plaintext.
 */
class SecretsManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool walletOpen READ isWalletOpen NOTIFY walletOpenChanged)

public:
    explicit SecretsManager(QObject *parent = nullptr);
    ~SecretsManager() override;

    bool isWalletOpen() const;

    Q_INVOKABLE void storeKey(const QString &provider, const QString &key);
    Q_INVOKABLE QString getKey(const QString &provider);
    Q_INVOKABLE void removeKey(const QString &provider);
    Q_INVOKABLE bool hasKey(const QString &provider);
    Q_INVOKABLE void retryOpenWallet();

Q_SIGNALS:
    void walletOpenChanged();
    void keyStored(const QString &provider);
    void keyRemoved(const QString &provider);
    void error(const QString &message);

private Q_SLOTS:
    void onWalletOpened(bool success);

private:
    void openWallet();
    bool ensureFolder();

    KWallet::Wallet *m_wallet = nullptr;
    bool m_walletOpen = false;
    QString m_folderName = QStringLiteral("ai-usage-monitor");

    // Pending operations queue
    struct PendingOp {
        enum Type { Store, Remove };
        Type type;
        QString provider;
        QString key;
    };
    QList<PendingOp> m_pendingOps;
};

#endif // SECRETSMANAGER_H
