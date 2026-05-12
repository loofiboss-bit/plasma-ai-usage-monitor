#include "secretsmanager.h"
#include <QDebug>

SecretsManager::SecretsManager(QObject *parent)
    : QObject(parent)
{
    openWallet();
}

SecretsManager::~SecretsManager()
{
    delete m_wallet;
}

bool SecretsManager::isWalletOpen() const
{
    return m_walletOpen;
}

void SecretsManager::retryOpenWallet()
{
    delete m_wallet;
    m_wallet = nullptr;
    if (m_walletOpen) {
        m_walletOpen = false;
        Q_EMIT walletOpenChanged();
    }
    openWallet();
}

void SecretsManager::openWallet()
{
    // Open KWallet asynchronously
    m_wallet = KWallet::Wallet::openWallet(
        KWallet::Wallet::LocalWallet(),
        0, // window id
        KWallet::Wallet::Asynchronous
    );

    if (m_wallet != nullptr) {
        connect(m_wallet, &KWallet::Wallet::walletOpened,
                this, &SecretsManager::onWalletOpened);
    } else {
        qWarning() << "AI Usage Monitor: Failed to create KWallet instance";
        Q_EMIT error(QStringLiteral("Failed to access KDE Wallet"));
    }
}

void SecretsManager::onWalletOpened(bool success)
{
    m_walletOpen = success;
    Q_EMIT walletOpenChanged();

    if (!success) {
        qWarning() << "AI Usage Monitor: KWallet failed to open";
        Q_EMIT error(QStringLiteral("Failed to open KDE Wallet"));
        return;
    }

    ensureFolder();

    // Process any pending operations
    for (const auto &op : std::as_const(m_pendingOps)) {
        switch (op.type) {
        case PendingOp::Store:
            storeKey(op.provider, op.key);
            break;
        case PendingOp::Remove:
            removeKey(op.provider);
            break;
        }
    }
    m_pendingOps.clear();
}

bool SecretsManager::ensureFolder()
{
    if (m_wallet == nullptr || !m_walletOpen) return false;

    if (!m_wallet->hasFolder(m_folderName)) {
        if (!m_wallet->createFolder(m_folderName)) {
            qWarning() << "AI Usage Monitor: Failed to create wallet folder";
            Q_EMIT error(QStringLiteral("Failed to create wallet folder"));
            return false;
        }
    }
    m_wallet->setFolder(m_folderName);
    return true;
}

void SecretsManager::storeKey(const QString &provider, const QString &key)
{
    if (!m_walletOpen) {
        m_pendingOps.append({PendingOp::Store, provider, key});
        return;
    }

    if (!ensureFolder()) return;

    int result = m_wallet->writePassword(provider, key);
    if (result == 0) {
        Q_EMIT keyStored(provider);
    } else {
        qWarning() << "AI Usage Monitor: Failed to store key for" << provider;
        Q_EMIT error(QStringLiteral("Failed to store key for %1").arg(provider));
    }
}

QString SecretsManager::getKey(const QString &provider)
{
    if (!m_walletOpen || !ensureFolder()) {
        return QString();
    }

    QString password;
    int result = m_wallet->readPassword(provider, password);
    if (result != 0) {
        qWarning() << "AI Usage Monitor: Failed to read key for" << provider;
        return QString();
    }
    return password;
}

void SecretsManager::removeKey(const QString &provider)
{
    if (!m_walletOpen) {
        m_pendingOps.append({PendingOp::Remove, provider, QString()});
        return;
    }

    if (!ensureFolder()) return;

    int result = m_wallet->removeEntry(provider);
    if (result == 0) {
        Q_EMIT keyRemoved(provider);
    } else {
        qWarning() << "AI Usage Monitor: Failed to remove key for" << provider;
        Q_EMIT error(QStringLiteral("Failed to remove key for %1").arg(provider));
    }
}

bool SecretsManager::hasKey(const QString &provider)
{
    if (!m_walletOpen || !ensureFolder()) {
        return false;
    }
    return m_wallet->hasEntry(provider);
}
