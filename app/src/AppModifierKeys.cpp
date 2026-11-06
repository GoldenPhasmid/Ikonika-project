#include "AppModifierKeys.h"

#include <QCoreApplication>
#include <QKeyEvent>

AppModifierKeys& AppModifierKeys::Get()
{
    static AppModifierKeys Instance;

    return Instance;
}

AppModifierKeys::AppModifierKeys(QObject* Parent)
    : QObject(Parent)
{
    QCoreApplication::instance()->installEventFilter(this);
}

bool AppModifierKeys::IsControlHeld() const
{
    return bControlHeld;
}

bool AppModifierKeys::eventFilter(QObject* Watched, QEvent* Event)
{
    switch (Event->type())
    {
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    {
        const auto* Key = static_cast<const QKeyEvent*>(Event);

        if (Key->key() == Qt::Key_Control)
        {
            SetControlHeld(Event->type() == QEvent::KeyPress);
        }
        else
        {
            SetControlHeld(Key->modifiers().testFlag(Qt::ControlModifier));
        }

        break;
    }
    case QEvent::ApplicationDeactivate:
    case QEvent::WindowDeactivate:
        SetControlHeld(false);
        break;
    default:
        break;
    }

    return QObject::eventFilter(Watched, Event);
}

void AppModifierKeys::SetControlHeld(bool bValue)
{
    if (bControlHeld == bValue)
    {
        return;
    }

    bControlHeld = bValue;

    emit controlHeldChanged();
}
