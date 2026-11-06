#include "AppStartupTask.h"

#include "AppPaths.h"

#include "core/FTLog.h"

#include <QString>

#include <thread>

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.Foundation.h>

namespace
{
    constexpr const wchar_t* TaskId = L"IkonikaStartup";

    QString Describe(const winrt::hresult_error& Error)
    {
        return QString::fromStdWString(std::wstring{Error.message()});
    }
}

namespace AppStartupTask
{
    void Apply(bool bEnabled)
    {
        std::thread Worker{[bEnabled]()
        {
            using namespace winrt::Windows::ApplicationModel;

            try
            {
                winrt::init_apartment();

                const StartupTask Task = StartupTask::GetAsync(TaskId).get();

                if (!bEnabled)
                {
                    Task.Disable();

                    qCInfo(LogApp) << "Removed from Windows startup";

                    return;
                }

                const StartupTaskState State = Task.RequestEnableAsync().get();

                if (State == StartupTaskState::Enabled || State == StartupTaskState::EnabledByPolicy)
                {
                    qCInfo(LogApp) << "Added to Windows startup";

                    return;
                }

                qCWarning(LogApp) << "Windows refused to add the app to startup, state"
                                  << static_cast<int>(State);
            }
            catch (const winrt::hresult_error& Error)
            {
                qCWarning(LogApp) << "Changing Windows startup failed:" << Describe(Error);
            }
        }};

        Worker.detach();
    }

    bool WasLaunchedAtSignIn()
    {
        if (!AppPaths::IsStorePackage())
        {
            return false;
        }

        using namespace winrt::Windows::ApplicationModel;

        try
        {
            const Activation::IActivatedEventArgs Arguments = AppInstance::GetActivatedEventArgs();

            return Arguments != nullptr && Arguments.Kind() == Activation::ActivationKind::StartupTask;
        }
        catch (const winrt::hresult_error& Error)
        {
            qCWarning(LogApp) << "Reading how the app was launched failed:" << Describe(Error);

            return false;
        }
    }
}
