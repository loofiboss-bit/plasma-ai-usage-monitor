#ifndef CLIPBOARDHELPER_H
#define CLIPBOARDHELPER_H

#include <QObject>
#include <QClipboard>
#include <QGuiApplication>
#include <QtQmlIntegration/qqmlintegration.h>

/**
 * Helper class to provide clipboard access from QML.
 * Replaces the fragile hidden TextArea hack with a proper C++ implementation.
 */
class ClipboardHelper : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit ClipboardHelper(QObject *parent = nullptr) : QObject(parent) {}

    /// Copy text to the system clipboard
    Q_INVOKABLE void setText(const QString &text) const
    {
        QClipboard *clipboard = QGuiApplication::clipboard();
        if (clipboard != nullptr) {
            clipboard->setText(text);
        }
    }

    /// Get text from the system clipboard
    Q_INVOKABLE QString text() const
    {
        QClipboard *clipboard = QGuiApplication::clipboard();
        return (clipboard != nullptr) ? clipboard->text() : QString();
    }
};

#endif // CLIPBOARDHELPER_H
