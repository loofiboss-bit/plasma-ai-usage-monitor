#include <QtTest>

#include "catalogupdater.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include <openssl/evp.h>

namespace {

QByteArray signEd25519(const QByteArray &message)
{
    const QByteArray seed = QByteArray::fromHex(
        "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
    EVP_PKEY *key = EVP_PKEY_new_raw_private_key(
        EVP_PKEY_ED25519, nullptr,
        reinterpret_cast<const unsigned char *>(seed.constData()),
        static_cast<size_t>(seed.size()));
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    if (!key || !context) {
        EVP_MD_CTX_free(context);
        EVP_PKEY_free(key);
        return {};
    }
    if (EVP_DigestSignInit(context, nullptr, nullptr, nullptr, key) != 1) {
        EVP_MD_CTX_free(context);
        EVP_PKEY_free(key);
        return {};
    }
    size_t signatureSize = 0;
    if (EVP_DigestSign(context, nullptr, &signatureSize,
                       reinterpret_cast<const unsigned char *>(message.constData()),
                       static_cast<size_t>(message.size())) != 1) {
        EVP_MD_CTX_free(context);
        EVP_PKEY_free(key);
        return {};
    }
    QByteArray signature(static_cast<qsizetype>(signatureSize), Qt::Uninitialized);
    if (EVP_DigestSign(context,
                       reinterpret_cast<unsigned char *>(signature.data()),
                       &signatureSize,
                       reinterpret_cast<const unsigned char *>(message.constData()),
                       static_cast<size_t>(message.size())) != 1) {
        EVP_MD_CTX_free(context);
        EVP_PKEY_free(key);
        return {};
    }
    signature.resize(static_cast<qsizetype>(signatureSize));
    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);
    return signature;
}

QByteArray envelopeForSequence(qint64 sequence)
{
    const QJsonObject payload{
        {QStringLiteral("schemaVersion"), 7},
        {QStringLiteral("catalogVersion"), QStringLiteral("2099.01.01")},
        {QStringLiteral("lastReviewed"), QStringLiteral("2099-01-01")},
        {QStringLiteral("runtimeScraping"), false},
        {QStringLiteral("sequence"), sequence},
        {QStringLiteral("hardExpiresAt"), QStringLiteral("2099-02-01T00:00:00Z")},
        {QStringLiteral("providers"), QJsonArray{}},
    };
    const QByteArray payloadBytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    const QJsonObject envelope{
        {QStringLiteral("sequence"), sequence},
        {QStringLiteral("expiresAt"), QStringLiteral("2099-01-15T00:00:00Z")},
        {QStringLiteral("keyId"), QStringLiteral("catalog-ed25519-v1")},
        {QStringLiteral("payload"), payload},
        {QStringLiteral("sha256"), QString::fromLatin1(
             QCryptographicHash::hash(payloadBytes, QCryptographicHash::Sha256).toHex())},
        {QStringLiteral("signature"), QString::fromLatin1(signEd25519(payloadBytes).toBase64())},
    };
    return QJsonDocument(envelope).toJson(QJsonDocument::Compact);
}

} // namespace

class CatalogUpdaterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void verifiesRfc8032Vector();
    void installsAndRejectsRollback();
    void rejectsTamperAndCorruptCache();
};

void CatalogUpdaterTest::verifiesRfc8032Vector()
{
    const QByteArray signature = QByteArray::fromHex(
        "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
        "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
    QVERIFY(CatalogUpdateManager::verifyEd25519({}, signature));
    QVERIFY(!CatalogUpdateManager::verifyEd25519("tampered", signature));
}

void CatalogUpdaterTest::installsAndRejectsRollback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QByteArray oldCache = qgetenv("AIUSAGE_MONITOR_CATALOG_CACHE_DIR");
    const bool hadCache = !oldCache.isNull();
    qputenv("AIUSAGE_MONITOR_CATALOG_CACHE_DIR", directory.path().toUtf8());

    CatalogUpdateManager manager;
    QString diagnostic;
    QVERIFY2(manager.installSignedFeed(envelopeForSequence(1), &diagnostic), qPrintable(diagnostic));
    QVERIFY(QFile::exists(manager.cachePath()));
    QVERIFY(QFile::exists(manager.cachePath() + QStringLiteral(".meta.json")));
    QVERIFY2(manager.installSignedFeed(envelopeForSequence(2), &diagnostic), qPrintable(diagnostic));
    QVERIFY(!manager.installSignedFeed(envelopeForSequence(1), &diagnostic));
    QVERIFY(diagnostic.contains(QStringLiteral("rollback")));

    if (hadCache) qputenv("AIUSAGE_MONITOR_CATALOG_CACHE_DIR", oldCache);
    else qunsetenv("AIUSAGE_MONITOR_CATALOG_CACHE_DIR");
}

void CatalogUpdaterTest::rejectsTamperAndCorruptCache()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QByteArray oldCache = qgetenv("AIUSAGE_MONITOR_CATALOG_CACHE_DIR");
    const bool hadCache = !oldCache.isNull();
    qputenv("AIUSAGE_MONITOR_CATALOG_CACHE_DIR", directory.path().toUtf8());

    CatalogUpdateManager manager;
    QString diagnostic;
    QJsonDocument document = QJsonDocument::fromJson(envelopeForSequence(1));
    QJsonObject tampered = document.object();
    QJsonObject payload = tampered.value(QStringLiteral("payload")).toObject();
    payload.insert(QStringLiteral("catalogVersion"), QStringLiteral("tampered"));
    tampered.insert(QStringLiteral("payload"), payload);
    QVERIFY(!manager.installSignedFeed(QJsonDocument(tampered).toJson(QJsonDocument::Compact), &diagnostic));
    QVERIFY(diagnostic.contains(QStringLiteral("signature")));

    QJsonObject unknownKey = document.object();
    unknownKey.insert(QStringLiteral("keyId"), QStringLiteral("unknown-key"));
    QVERIFY(!manager.installSignedFeed(QJsonDocument(unknownKey).toJson(QJsonDocument::Compact), &diagnostic));
    QVERIFY(diagnostic.contains(QStringLiteral("key id")));

    QVERIFY(manager.installSignedFeed(envelopeForSequence(1), &diagnostic));
    QFile cache(manager.cachePath());
    QVERIFY(cache.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(cache.write("tampered") > 0);
    cache.close();
    QVERIFY(!manager.installSignedFeed(envelopeForSequence(2), &diagnostic));
    QVERIFY(diagnostic.contains(QStringLiteral("integrity")));

    if (hadCache) qputenv("AIUSAGE_MONITOR_CATALOG_CACHE_DIR", oldCache);
    else qunsetenv("AIUSAGE_MONITOR_CATALOG_CACHE_DIR");
}

QTEST_MAIN(CatalogUpdaterTest)
#include "test_catalogupdater.moc"
