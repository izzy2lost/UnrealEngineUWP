// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubEditorModule.h"

#include "HAL/FileManager.h"
#include "LiveLinkHubEditorSettings.h"
#include "Misc/App.h"
#include "Misc/AsyncTaskNotification.h"
#include "Runtime/Launch/Resources/Version.h"
#include "SLiveLinkHubEditorStatusBar.h"
#include "ToolMenus.h"

static TAutoConsoleVariable<int32> CVarLiveLinkHubEnableStatusBar(
	TEXT("LiveLinkHub.EnableStatusBar"), 1,
	TEXT("Whether to enable showing the livelink hub status bar in the editor. Must be set before launching the editor."),
	ECVF_RenderThreadSafe);

DECLARE_LOG_CATEGORY_CLASS(LogLiveLinkHubEditor, Log, Log)

#define LOCTEXT_NAMESPACE "LiveLinkHubEditor"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <winreg.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#if DETECT_LIVELINKHUB
static const FString LiveLinkHubRegistryPath = TEXT("Software\\Epic Games\\LiveLinkHub\\") VERSION_STRINGIFY(ENGINE_MAJOR_VERSION) TEXT(".") VERSION_STRINGIFY(ENGINE_MINOR_VERSION);
static const FString LiveLinkHubExecutablePath = TEXT("ExecutablePath");

namespace LiveLinkHubUtils
{
	/** Attempt to open the livelinkhub registry key that contains the path to the executable. */
	HKEY OpenLiveLinkHubKey()
	{
		HKEY HkcuRunKey = nullptr;

		const LSTATUS OpenResult = RegCreateKeyEx(HKEY_CURRENT_USER, *LiveLinkHubRegistryPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &HkcuRunKey, NULL);
		if (OpenResult != ERROR_SUCCESS)
		{
			UE_LOG(LogLiveLinkHubEditor, Log, TEXT("Error opening registry key %s (%08X)"), *LiveLinkHubRegistryPath, OpenResult);
			return nullptr;
		}
		return HkcuRunKey;
	}

	/** Set the livelinkhub registry key to the livelinkhub executable path. */
	bool SaveExecutablePathToRegistry()
	{
		HKEY HkcuRunKey = OpenLiveLinkHubKey();
		if (!HkcuRunKey)
		{
			return false;
		}

		ON_SCOPE_EXIT
		{
			RegCloseKey(HkcuRunKey);
		};

		TCHAR ExeFilePath[MAX_PATH + 1];
		int32 PathLength = ::GetModuleFileNameW(NULL, ExeFilePath, MAX_PATH + 1);
		if (PathLength > 0)
		{
			const LSTATUS SetResult = RegSetValueEx(HkcuRunKey, *LiveLinkHubExecutablePath, 0, REG_SZ, reinterpret_cast<const BYTE*>(ExeFilePath), (PathLength + 1) * sizeof(TCHAR));
			if (SetResult != ERROR_SUCCESS)
			{
				UE_LOG(LogLiveLinkHubEditor, Error, TEXT("Error setting registry value %s (%08X)"), *LiveLinkHubExecutablePath, SetResult);
				return false;
			}
		}
		else
		{
			UE_LOG(LogLiveLinkHubEditor, Error, TEXT("Error setting getting the executable path while setting the registry key for livelink hub."));
			return false;
		}

		return true;
	}

	/** Get the executable path from the registry. */
	bool GetExecutablePathFromRegistry(FString& OutExecutablePath)
	{
		HKEY HkcuRunKey = OpenLiveLinkHubKey();
		if (!HkcuRunKey)
		{
			return false;
		}

		ON_SCOPE_EXIT
		{
			RegCloseKey(HkcuRunKey);
		};

		wchar_t InstallPathStr[MAX_PATH];
		DWORD InstallPathSize = sizeof(InstallPathStr);

		LSTATUS GetResult = RegGetValue(HkcuRunKey, NULL, *LiveLinkHubExecutablePath, RRF_RT_REG_SZ, NULL, &InstallPathStr, &InstallPathSize);
		if (GetResult != ERROR_SUCCESS)
		{
			UE_LOG(LogLiveLinkHubEditor, Error, TEXT("Error getting registry value %s:\"%s\" (%08X)"), *LiveLinkHubRegistryPath, *LiveLinkHubExecutablePath, GetResult);
			return false;
		}

		OutExecutablePath = WCHAR_TO_TCHAR(InstallPathStr);

		return true;
	}
}
#endif

void FLiveLinkHubEditorModule::StartupModule()
{
	if (!IsRunningCommandlet() && CVarLiveLinkHubEnableStatusBar.GetValueOnAnyThread())
	{
		FCoreDelegates::OnPostEngineInit.AddRaw(this, &FLiveLinkHubEditorModule::OnPostEngineInit);
	}
}

void FLiveLinkHubEditorModule::ShutdownModule()
{
	UToolMenus::UnregisterOwner(this);

	if (!IsRunningCommandlet() && CVarLiveLinkHubEnableStatusBar.GetValueOnAnyThread())
	{
		FCoreDelegates::OnPostEngineInit.RemoveAll(this);
		UnregisterLiveLinkHubStatusBar();
	}
}

void FLiveLinkHubEditorModule::OnPostEngineInit()
{
	if (GEditor)
	{
		RegisterLiveLinkHubStatusBar();
		
#if DETECT_LIVELINKHUB
		if (GetDefault<ULiveLinkHubEditorSettings>()->bDetectLiveLinkHubExecutable)
		{
			LiveLinkHubUtils::GetExecutablePathFromRegistry(LiveLinkHubExecutablePath);
		}

		if (GetDefault<ULiveLinkHubEditorSettings>()->bWriteLiveLinkHubRegistryKey)
		{
			// Set the executable path registry key if it hasn't been set.
			LiveLinkHubUtils::SaveExecutablePathToRegistry();
		}
#endif

		FToolMenuOwnerScoped OwnerScoped(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		FToolMenuSection& Section = Menu->AddSection("VirtualProductionSection", LOCTEXT("VirtualProductionSection", "Virtual Production"));

		Section.AddMenuEntry("LiveLinkHub",
			LOCTEXT("LiveLinkHubLabel", "LiveLink Hub"),
			LOCTEXT("LiveLinkHubTooltip", "Launch the LiveLink Hub app."),
			FSlateIcon("LiveLinkStyle", "LiveLinkClient.Common.Icon.Small"),
			FUIAction(FExecuteAction::CreateRaw(this, &FLiveLinkHubEditorModule::OpenLiveLinkHub)));
	}
}

void FLiveLinkHubEditorModule::OpenLiveLinkHub() const
{
	FAsyncTaskNotificationConfig NotificationConfig;
	NotificationConfig.bKeepOpenOnFailure = true;
	NotificationConfig.TitleText = LOCTEXT("LaunchingLiveLinkHub", "Launching LiveLink Hub...");
	NotificationConfig.LogCategory = &LogLiveLinkHubEditor;

	FAsyncTaskNotification Notification(NotificationConfig);

	// Find livelink hub executable location for our build configuration
	FString LiveLinkHubPath = FPlatformProcess::GenerateApplicationPath(TEXT("LiveLinkHub"), FApp::GetBuildConfiguration());

	// Validate it exists and fall back to development if it doesn't.
	if (!IFileManager::Get().FileExists(*LiveLinkHubPath))
	{
		LiveLinkHubPath = FPlatformProcess::GenerateApplicationPath(TEXT("LiveLinkHub"), EBuildConfiguration::Development);

		// If it still doesn't exist, fall back to the shipping executable.
		if (!IFileManager::Get().FileExists(*LiveLinkHubPath))
		{
			LiveLinkHubPath = FPlatformProcess::GenerateApplicationPath(TEXT("LiveLinkHub"), EBuildConfiguration::Shipping);
		}
	}

	const FText LaunchLiveLinkHubErrorTitle = LOCTEXT("LaunchLiveLinkHubErrorTitle", "Failed to Launch LiveLinkhub.");
	if (!IFileManager::Get().FileExists(*LiveLinkHubPath))
	{
		Notification.SetComplete(
			LaunchLiveLinkHubErrorTitle,
			LOCTEXT("LaunchLiveLinkHubError_ExecutableMissing", "Could not find the executable. Have you compiled the LiveLink Hub app?"),
			false
		);

		return;
	}

	if (GetDefault<ULiveLinkHubEditorSettings>()->bDetectLiveLinkHubExecutable)
	{
		LiveLinkHubPath = LiveLinkHubExecutablePath;
	}

	// Validate we do not have it running locally
	const FString AppName = FPaths::GetCleanFilename(LiveLinkHubPath);
	if (FPlatformProcess::IsApplicationRunning(*AppName))
	{
		Notification.SetComplete(
			LaunchLiveLinkHubErrorTitle,
			LOCTEXT("LaunchLiveLinkHubError_AlreadyRunning", "A LiveLinkHub instance is already running."),
			false
		);
		return;
	}

	constexpr bool bLaunchDetached = true;
	constexpr bool bLaunchHidden = false;
	constexpr bool bLaunchReallyHidden = false;

	const FProcHandle ProcHandle = FPlatformProcess::CreateProc(*LiveLinkHubPath, TEXT(""), bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, nullptr, 0, nullptr, nullptr, nullptr);
	if (ProcHandle.IsValid())
	{
		Notification.SetComplete(
			LOCTEXT("LaunchedLiveLinkHub", "Launched LiveLink Hub"), FText(), true);

		return;
	}
	else // Very unlikely in practice, but possible in theory.
	{
		Notification.SetComplete(
			LaunchLiveLinkHubErrorTitle,
			LOCTEXT("LaunchLiveLinkHubError_InvalidHandle", "Failed to create the LiveLink Hub process."),
			false);
	}
}

void FLiveLinkHubEditorModule::RegisterLiveLinkHubStatusBar()
{
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.StatusBar.ToolBar"));

	FToolMenuSection& LiveLinkHubSection = Menu->AddSection(TEXT("LiveLinkHub"), FText::GetEmpty(), FToolMenuInsert(NAME_None, EToolMenuInsertType::First));

	LiveLinkHubSection.AddEntry(
		FToolMenuEntry::InitWidget(TEXT("LiveLinkHubStatusBar"), CreateLiveLinkHubWidget(), FText::GetEmpty(), true, false)
	);
}

void FLiveLinkHubEditorModule::UnregisterLiveLinkHubStatusBar()
{
	UToolMenus::UnregisterOwner(this);
}

TSharedRef<SWidget> FLiveLinkHubEditorModule::CreateLiveLinkHubWidget()
{
	return SNew(SLiveLinkHubEditorStatusBar);
}

IMPLEMENT_MODULE(FLiveLinkHubEditorModule, LiveLinkHubEditor);

#undef LOCTEXT_NAMESPACE /*LiveLinkHubEditor*/ 