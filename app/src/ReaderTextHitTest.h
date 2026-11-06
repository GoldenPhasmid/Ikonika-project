#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickTextDocument>
#include <QtQml/qqmlregistration.h>

#include <optional>

class QTextLayout;
class QTextLine;

class ReaderTextHitTest : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickTextDocument* textDocument READ GetTextDocument WRITE SetTextDocument NOTIFY textDocumentChanged)

public:
    explicit ReaderTextHitTest(QObject* Parent = nullptr);

    QQuickTextDocument* GetTextDocument() const;
    void SetTextDocument(QQuickTextDocument* Value);

    Q_INVOKABLE int characterAt(qreal X, qreal Y) const;

    Q_INVOKABLE int positionNear(qreal X, qreal Y) const;

signals:
    void textDocumentChanged();

private:
    struct FLineHit
    {
        int BlockPosition = 0;
        const QTextLayout* Layout = nullptr;
        int LineIndex = 0;
    };

    std::optional<FLineHit> FindLine(qreal Y, bool bClamp) const;

    QPointer<QQuickTextDocument> TextDocument;
};
