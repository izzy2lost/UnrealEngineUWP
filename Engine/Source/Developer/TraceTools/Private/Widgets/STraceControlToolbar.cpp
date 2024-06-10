// Copyright Epic Games, Inc. All Rights Reserved.

#include "STraceControlToolbar.h"

#include "ITraceController.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SlateOptMacros.h"
#include "SocketSubsystem.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"

//TraceTools
#include "Models/TraceControlCommands.h"

#define LOCTEXT_NAMESPACE "STraceControlToolbar"

namespace UE::TraceTools
{

STraceControlToolbar::STraceControlToolbar()
{
}

STraceControlToolbar::~STraceControlToolbar()
{
	TraceController->OnSelectedSessionStatusReceived().Remove(OnStatusReceivedDelegate);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void STraceControlToolbar::Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& CommandList, TSharedPtr<ITraceController> InTraceController)
{
	InitializeSettings();

	FTraceControlCommands::Register();
	TraceController = InTraceController;

	OnStatusReceivedDelegate = TraceController->OnStatusReceived().AddSP(this, &STraceControlToolbar::OnTraceStatusUpdated);

	BindCommands(CommandList);

	// create the toolbar
	FToolBarBuilder Toolbar(CommandList, FMultiBoxCustomization::None);
	{
		Toolbar.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateSP(this, &STraceControlToolbar::BuildTraceTargetMenu, CommandList),
			TAttribute<FText>::CreateSP(this, &STraceControlToolbar::GetTraceTargetLabelText),
			TAttribute<FText>::CreateSP(this, &STraceControlToolbar::GetTraceTargetTooltipText),
			TAttribute<FSlateIcon>::CreateSP(this, &STraceControlToolbar::GetTraceTargetIcon),
			false);

		Toolbar.AddSeparator();

		Toolbar.AddToolBarButton(FTraceControlCommands::Get().StartTrace);
		Toolbar.AddToolBarButton(FTraceControlCommands::Get().StopTrace);
		Toolbar.AddToolBarButton(FTraceControlCommands::Get().TraceSnapshot);

		Toolbar.AddSeparator();

		Toolbar.AddToolBarButton(FTraceControlCommands::Get().PauseTrace);
		Toolbar.AddToolBarButton(FTraceControlCommands::Get().ResumeTrace);

		Toolbar.AddSeparator();

		Toolbar.AddToolBarButton(FTraceControlCommands::Get().TraceBookmark);
		Toolbar.AddToolBarButton(FTraceControlCommands::Get().TraceScreenshot);

		Toolbar.AddSeparator();

		Toolbar.AddToolBarButton(FTraceControlCommands::Get().ToggleStatNamedEvents);
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(0.0f)
		[
			Toolbar.MakeWidget()
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> STraceControlToolbar::BuildTraceTargetMenu(const TSharedRef<FUICommandList> CommandList)
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, CommandList);

	MenuBuilder.SetSearchable(false);

	MenuBuilder.AddMenuEntry(FTraceControlCommands::Get().SetTraceTargetServer);
	MenuBuilder.AddMenuEntry(FTraceControlCommands::Get().SetTraceTargetFile);

	return MenuBuilder.MakeWidget();
}

void STraceControlToolbar::BindCommands(const TSharedRef<FUICommandList>& CommandList)
{
	CommandList->MapAction(FTraceControlCommands::Get().SetTraceTargetServer, 
						   FExecuteAction::CreateSP(this, &STraceControlToolbar::SetTraceTarget_Execute, ETraceTarget::Server),
						   FCanExecuteAction::CreateSP(this, &STraceControlToolbar::SetTraceTarget_CanExecute));

	CommandList->MapAction(FTraceControlCommands::Get().SetTraceTargetFile, 
						   FExecuteAction::CreateSP(this, &STraceControlToolbar::SetTraceTarget_Execute, ETraceTarget::File),
						   FCanExecuteAction::CreateSP(this, &STraceControlToolbar::SetTraceTarget_CanExecute));

	CommandList->MapAction(FTraceControlCommands::Get().StartTrace, 
					       FExecuteAction::CreateSP(this, &STraceControlToolbar::StartTrace_Execute),
					       FCanExecuteAction::CreateSP(this, &STraceControlToolbar::StartTrace_CanExecute));

	CommandList->MapAction(FTraceControlCommands::Get().StopTrace, 
						   FExecuteAction::CreateSP(this, &STraceControlToolbar::StopTrace_Execute),
						   FCanExecuteAction::CreateSP(this, &STraceControlToolbar::StopTrace_CanExecute));

	CommandList->MapAction(FTraceControlCommands::Get().TraceSnapshot, 
						   FExecuteAction::CreateSP(this, &STraceControlToolbar::TraceSnapshot_Execute),
						   FCanExecuteAction::CreateSP(this, &STraceControlToolbar::TraceSnapshot_CanExecute));

	CommandList->MapAction(FTraceControlCommands::Get().PauseTrace, 
						   FExecuteAction::CreateSP(this, &STraceControlToolbar::PauseTrace_Execute),
						   FCanExecuteAction::CreateSP(this, &STraceControlToolbar::PauseTrace_CanExecute));

	CommandList->MapAction(FTraceControlCommands::Get().ResumeTrace, 
						   FExecuteAction::CreateSP(this, &STraceControlToolbar::ResumeTrace_Execute),
						   FCanExecuteAction::CreateSP(this, &STraceControlToolbar::ResumeTrace_CanExecute));

	CommandList->MapAction(FTraceControlCommands::Get().TraceBookmark, 
						   FExecuteAction::CreateSP(this, &STraceControlToolbar::TraceBookmark_Execute),
						   FCanExecuteAction::CreateSP(this, &STraceControlToolbar::TraceBookmark_CanExecute));

	CommandList->MapAction(FTraceControlCommands::Get().TraceScreenshot, 
						   FExecuteAction::CreateSP(this, &STraceControlToolbar::TraceScreenshot_Execute),
						   FCanExecuteAction::CreateSP(this, &STraceControlToolbar::TraceScreenshot_CanExecute));

	CommandList->MapAction(FTraceControlCommands::Get().ToggleStatNamedEvents, 
						   FExecuteAction::CreateSP(this, &STraceControlToolbar::ToggleStatNamedEvents_Execute),
						   FCanExecuteAction::CreateSP(this, &STraceControlToolbar::ToggleStatNamedEvents_CanExecute),
						   FIsActionChecked::CreateSP(this, &STraceControlToolbar::ToggleStatNamedEvents_IsChecked));
}

void STraceControlToolbar::InitializeSettings()
{
	TSharedPtr<FInternetAddr> RecorderAddr;
	if (ISocketSubsystem* Sockets = ISocketSubsystem::Get())
	{
		bool bCanBindAll = false;
		RecorderAddr = Sockets->GetLocalHostAddr(*GLog, bCanBindAll);
	}

	if (RecorderAddr.IsValid())
	{
		TraceHostAddr = RecorderAddr->ToString(false);
	}
	else
	{
		TraceHostAddr = TEXT("127.0.0.1");
	}
}

bool STraceControlToolbar::SetTraceTarget_CanExecute() const
{
	return TraceController->HasAvailableSelectedInstance() && !bIsTracing;
}

void STraceControlToolbar::SetTraceTarget_Execute(ETraceTarget InTraceTarget)
{
	TraceTarget = InTraceTarget;
}

bool STraceControlToolbar::StartTrace_CanExecute() const
{
	return TraceController->HasAvailableSelectedInstance() && bIsTracing == false;
}

void STraceControlToolbar::StartTrace_Execute()
{
	TraceController->WithSelectedInstances([&](const FTraceStatus& Status, ITraceControllerCommands& Commands)
	{
		if (TraceTarget == ETraceTarget::Server)
		{
			Commands.Send(TraceHostAddr, TEXT(""));
		}
		else if (TraceTarget == ETraceTarget::File)
		{
			Commands.File(TEXT(""), TEXT(""));
		}
	});
	bIsTracing = true;
}

bool STraceControlToolbar::StopTrace_CanExecute() const
{
	return TraceController->HasAvailableSelectedInstance() && bIsTracing == true;
}

void STraceControlToolbar::StopTrace_Execute()
{
	TraceController->WithSelectedInstances([&](const FTraceStatus& Status, ITraceControllerCommands& Commands)
	{
		Commands.Stop();
	});
	bIsTracing = false;
}

bool STraceControlToolbar::TraceSnapshot_CanExecute() const
{
	return TraceController->HasAvailableSelectedInstance();
}

void STraceControlToolbar::TraceSnapshot_Execute()
{
	TraceController->WithSelectedInstances([&](const FTraceStatus& Status, ITraceControllerCommands& Commands)
	{
		if (TraceTarget == ETraceTarget::Server)
		{
			Commands.SnapshotSend(TraceHostAddr);
		}
		else if (TraceTarget == ETraceTarget::File)
		{
			Commands.SnapshotFile(TEXT(""));
		}
	});
}

bool STraceControlToolbar::PauseTrace_CanExecute() const
{
	return TraceController->HasAvailableSelectedInstance() && bIsTracing && bIsPaused == false;
}

void STraceControlToolbar::PauseTrace_Execute()
{
	TraceController->WithSelectedInstances([&](const FTraceStatus& Status, ITraceControllerCommands& Commands)
	{
		Commands.Pause();
	});
	bIsPaused = true;
}

bool STraceControlToolbar::ResumeTrace_CanExecute() const
{
	return TraceController->HasAvailableSelectedInstance() && bIsTracing && bIsPaused == true;
}

void STraceControlToolbar::ResumeTrace_Execute()
{
	TraceController->WithSelectedInstances([&](const FTraceStatus& Status, ITraceControllerCommands& Commands)
	{
		Commands.Resume();
	});
	bIsPaused = false;
}

bool STraceControlToolbar::TraceBookmark_CanExecute() const
{
	return TraceController->HasAvailableSelectedInstance() && bIsTracing && !bIsPaused;
}

void STraceControlToolbar::TraceBookmark_Execute()
{
	const FString BookmarkName = FDateTime::Now().ToString(TEXT("Bookmark_%Y%m%d_%H%M%S"));
	TraceController->WithSelectedInstances([&](const FTraceStatus& Status, ITraceControllerCommands& Commands)
	{
		Commands.Bookmark(BookmarkName);
	});
}

bool STraceControlToolbar::TraceScreenshot_CanExecute() const
{
	return TraceController->HasAvailableSelectedInstance() && bIsTracing && !bIsPaused;
}

void STraceControlToolbar::TraceScreenshot_Execute()
{
	TraceController->WithSelectedInstances([&](const FTraceStatus& Status, ITraceControllerCommands& Commands)
	{
		Commands.Screenshot(TEXT(""), false);
	});
}

bool STraceControlToolbar::ToggleStatNamedEvents_CanExecute() const
{
	return TraceController->HasAvailableSelectedInstance() && bIsTracing;
}

bool STraceControlToolbar::ToggleStatNamedEvents_IsChecked() const
{
	return bAreStatNamedEventsEnabled;
}

void STraceControlToolbar::ToggleStatNamedEvents_Execute()
{
	bAreStatNamedEventsEnabled = !bAreStatNamedEventsEnabled;
	TraceController->WithSelectedInstances([&](const FTraceStatus& Status, ITraceControllerCommands& Commands)
	{
		Commands.SetStatNamedEventsEnabled(bAreStatNamedEventsEnabled);
	});
}

FText STraceControlToolbar::GetTraceTargetLabelText() const
{
	if (TraceTarget == ETraceTarget::Server)
	{
		return LOCTEXT("TraceTargetServerLabel", "Server");
	}

	return LOCTEXT("TraceTargetFileLabel", "File");
}

FText STraceControlToolbar::GetTraceTargetTooltipText() const
{
	if (TraceTarget == ETraceTarget::Server)
	{
		return LOCTEXT("TraceTargetServerTooltip", "Set the Unreal Trace Server as the trace target.");
	}

	return LOCTEXT("TraceTargetFileTooltip", "Set File as the trace target.");
}

FSlateIcon STraceControlToolbar::GetTraceTargetIcon() const
{
	if (TraceTarget == ETraceTarget::Server)
	{
		return FSlateIcon(FTraceToolsStyle::GetStyleSetName(), "TraceControl.SetTraceTargetServer");
	}

	return FSlateIcon(FTraceToolsStyle::GetStyleSetName(), "TraceControl.SetTraceTargetFile");
}

void STraceControlToolbar::OnTraceStatusUpdated(const FTraceStatus& InStatus, FTraceStatus::EUpdateType InUpdateType, ITraceControllerCommands& Commands)
{
	bIsTracing = InStatus.bIsTracing;
	bIsPaused = InStatus.bIsPaused;
	bAreStatNamedEventsEnabled = InStatus.bAreStatNamedEventsEnabled;
}

} // namespace UE::TraceTools

#undef LOCTEXT_NAMESPACE