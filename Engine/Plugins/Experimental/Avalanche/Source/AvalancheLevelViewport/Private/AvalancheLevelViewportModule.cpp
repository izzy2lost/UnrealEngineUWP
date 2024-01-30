// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvalancheLevelViewportModule.h"

#include "AvaLevelViewportCommands.h"
#include "AvaLevelViewportStyle.h"
#include "AvaViewportCameraHistory.h"
#include "AvaViewportUtils.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "ViewportClient/AvaLevelViewportClient.h"

DEFINE_LOG_CATEGORY(AvaLevelViewportLog);

namespace UE::AvalancheLevelViewport::Private
{
	TSharedPtr<IAvaViewportClient> GetAsAvaLevelViewportClient(FEditorViewportClient* InViewportClient)
	{
		if (FAvaLevelViewportClient::IsAvaLevelViewportClient(InViewportClient))
		{
			return static_cast<FAvaLevelViewportClient*>(InViewportClient)->AsShared();
		}

		return nullptr;
	}
}

void FAvalancheLevelViewportModule::StartupModule()
{
	FAvaLevelViewportStyle::Initialize();
	FAvaLevelViewportCommands::Register();

	ViewportCameraHistory = MakeShared<FAvaViewportCameraHistory>();

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAvalancheLevelViewportModule::RegisterMenus));

	AvaLevelViewportClientCasterDelegateHandle = FAvaViewportUtils::RegisterViewportClientCaster(
		&UE::AvalancheLevelViewport::Private::GetAsAvaLevelViewportClient
	);
}

void FAvalancheLevelViewportModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	ViewportCameraHistory.Reset();

	FAvaLevelViewportStyle::Shutdown();
	FAvaLevelViewportCommands::Unregister();

	if (AvaLevelViewportClientCasterDelegateHandle.IsValid())
	{
		FAvaViewportUtils::UnregisterViewportClientCaster(AvaLevelViewportClientCasterDelegateHandle);
		AvaLevelViewportClientCasterDelegateHandle.Reset();
	}
}

void FAvalancheLevelViewportModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);
	
	// Extension point for SAvaLevelViewportStatusBar
	{
		static FName StatusBarMenuName = UE::AvaLevelViewport::Internal::StatusBarMenuName;
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(StatusBarMenuName, NAME_None, EMultiBoxType::ToolBar, false);
		Menu->SetStyleSet(&FAvaLevelViewportStyle::Get());
		Menu->StyleName = "StatusBar";
	}
}

IMPLEMENT_MODULE(FAvalancheLevelViewportModule, AvalancheLevelViewport)
