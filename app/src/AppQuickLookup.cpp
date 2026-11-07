#include "AppQuickLookup.h"

#include "core/FTLog.h"

#include <QClipboard>
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QThread>
#include <QWindow>
#include <QtCore/qt_windows.h>
#include <atomic>

struct AppQuickLookup::FKeyboardCapture : QObject
{
    explicit FKeyboardCapture(AppQuickLookup& InOwner)
        : Owner(InOwner)
    {
        moveToThread(&Thread);
        Thread.start();
    }

    ~FKeyboardCapture() override
    {
        QMetaObject::invokeMethod(this, [this]()
        {
            UpdateHook(false);
            moveToThread(Owner.thread());
        }, Qt::BlockingQueuedConnection);
        Thread.quit();
        Thread.wait();
    }

    bool UpdateHook(bool bEnable)
    {
        if (bEnable)
        {
            Hook = SetWindowsHookExW(WH_KEYBOARD_LL, OnKeyboardEvent, GetModuleHandleW(nullptr), 0);
            if (Hook == nullptr)
            {
                qCWarning(LogApp) << "Could not enable Quick Add:" << GetLastError();
                return false;
            }

            Active = this;
            return true;
        }

        if (Hook != nullptr)
        {
            UnhookWindowsHookEx(Hook);
            Hook = nullptr;
            Active = nullptr;
        }

        return false;
    }

    static LRESULT CALLBACK OnKeyboardEvent(int Code, WPARAM Message, LPARAM Data);
    void OnCopyKey(WPARAM Message, const KBDLLHOOKSTRUCT& Event);

    static thread_local FKeyboardCapture* Active;
    AppQuickLookup& Owner;
    QThread Thread;
    HHOOK Hook = nullptr;
    HWND SourceWindow = nullptr;
    DWORD FirstTap = 0;
    DWORD ClipboardSequence = 0;
    bool bCopyHeld = false;
    bool bFirstTap = false;
    std::atomic_uint Revision{0};
    unsigned int PendingRevision = 0;
    HWND PendingWindow = nullptr;
    DWORD PendingSequence = 0;
    HWND ReturnWindow = nullptr;
};

thread_local AppQuickLookup::FKeyboardCapture* AppQuickLookup::FKeyboardCapture::Active = nullptr;

AppQuickLookup::AppQuickLookup(QObject* Parent)
    : QObject(Parent), Capture(std::make_unique<FKeyboardCapture>(*this))
{
    ClipboardTimer.setInterval(40);
    connect(&ClipboardTimer, &QTimer::timeout, this, &AppQuickLookup::ReadSelection);

    ApplyEnabled(QSettings{}.value(QStringLiteral("desktop/quickLookup"), true).toBool());
}

AppQuickLookup::~AppQuickLookup() = default;

bool AppQuickLookup::IsEnabled() const
{
    return bEnabled;
}

void AppQuickLookup::SetEnabled(bool bEnable)
{
    if (bEnable == IsEnabled())
    {
        return;
    }

    ApplyEnabled(bEnable);
    if (bEnable == IsEnabled())
    {
        QSettings{}.setValue(QStringLiteral("desktop/quickLookup"), bEnable);
    }
    emit enabledChanged();
}

void AppQuickLookup::ApplyEnabled(bool bEnable)
{
    bool bInstalled{false};
    QMetaObject::invokeMethod(Capture.get(), [this, bEnable, &bInstalled]()
    {
        bInstalled = Capture->UpdateHook(bEnable);
    }, Qt::BlockingQueuedConnection);
    bEnabled = bInstalled;

    if (!bEnabled)
    {
        Reset();
    }
}

void AppQuickLookup::Reset()
{
    ClipboardTimer.stop();
    Capture->ReturnWindow = nullptr;
    QMetaObject::invokeMethod(Capture.get(), [this]()
    {
        ++Capture->Revision;
        Capture->bFirstTap = false;
        Capture->bCopyHeld = false;
    }, Qt::BlockingQueuedConnection);
}

LRESULT CALLBACK AppQuickLookup::FKeyboardCapture::OnKeyboardEvent(int Code, WPARAM Message, LPARAM Data)
{
    if (Code == HC_ACTION && Active != nullptr)
    {
        Active->OnCopyKey(Message, *reinterpret_cast<const KBDLLHOOKSTRUCT*>(Data));
    }

    return CallNextHookEx(nullptr, Code, Message, Data);
}

void AppQuickLookup::FKeyboardCapture::OnCopyKey(WPARAM Message, const KBDLLHOOKSTRUCT& Event)
{
    if ((Event.flags & LLKHF_INJECTED) != 0)
    {
        return;
    }

    if (Event.vkCode != 'C')
    {
        if (Message == WM_KEYDOWN || Message == WM_SYSKEYDOWN
            || Event.vkCode == VK_LCONTROL || Event.vkCode == VK_RCONTROL)
        {
            bFirstTap = false;
        }

        return;
    }

    if (Message == WM_KEYUP || Message == WM_SYSKEYUP)
    {
        bCopyHeld = false;

        return;
    }

    if (bCopyHeld)
    {
        return;
    }

    bCopyHeld = true;

    if (Message != WM_KEYDOWN || (GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0
        || (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0
        || (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0)
    {
        bFirstTap = false;

        return;
    }

    const HWND Foreground{GetForegroundWindow()};

    if (Foreground == nullptr)
    {
        bFirstTap = false;

        return;
    }

    constexpr DWORD DoubleTapMilliseconds{450};

    if (bFirstTap && SourceWindow == Foreground && Event.time - FirstTap <= DoubleTapMilliseconds)
    {
        bFirstTap = false;
        QMetaObject::invokeMethod(&Owner, [this, Foreground, Sequence = ClipboardSequence, Gesture = Revision.load()]()
        {
            if (!Owner.IsEnabled() || Gesture != Revision.load())
            {
                return;
            }

            PendingWindow = Foreground;
            PendingSequence = Sequence;
            PendingRevision = Gesture;
            Owner.ClipboardAttempts = 9;
            Owner.ClipboardTimer.start();
        }, Qt::QueuedConnection);

        return;
    }

    ++Revision;
    SourceWindow = Foreground;
    FirstTap = Event.time;
    ClipboardSequence = GetClipboardSequenceNumber();
    bFirstTap = true;
}

void AppQuickLookup::ReadSelection()
{
    if (--ClipboardAttempts == 0 || Capture->PendingRevision != Capture->Revision.load()
        || Capture->PendingWindow != GetForegroundWindow())
    {
        ClipboardTimer.stop();

        return;
    }

    if (GetClipboardSequenceNumber() == Capture->PendingSequence)
    {
        return;
    }

    const QString Text{QGuiApplication::clipboard()->text().simplified()};

    if (Text.isEmpty())
    {
        return;
    }

    ClipboardTimer.stop();

    constexpr qsizetype MaximumSelectionLength{160};

    if (Text.size() <= MaximumSelectionLength && Capture->PendingWindow == GetForegroundWindow()
        && Capture->PendingRevision == Capture->Revision.load())
    {
        Capture->ReturnWindow = Capture->PendingWindow;
        emit lookupRequested(Text);
    }
}

void AppQuickLookup::present(QWindow* Window)
{
    const QPoint Position{QCursor::pos()};

    if (QScreen* Screen = QGuiApplication::screenAt(Position))
    {
        const QRect Area{Screen->availableGeometry()};
        Window->setScreen(Screen);
        Window->setFramePosition({
            qBound(Area.left(), Position.x() + 12, qMax(Area.left(), Area.right() - Window->width() - 16)),
            qBound(Area.top(), Position.y() + 16, qMax(Area.top(), Area.bottom() - Window->height() - 48))});
    }

    Window->show();
    Window->raise();
    Window->requestActivate();
}

void AppQuickLookup::dismiss(QWindow* Window, bool bRestoreFocus)
{
    const HWND PreviousWindow{Capture->ReturnWindow};
    Capture->ReturnWindow = nullptr;

    if (bRestoreFocus && GetForegroundWindow() == reinterpret_cast<HWND>(Window->winId())
        && PreviousWindow != nullptr && IsWindow(PreviousWindow))
    {
        SetForegroundWindow(PreviousWindow);
    }

    Window->hide();
}
