// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXConflictMonitor.h"

#include "Algo/Transform.h"
#include "DMXConflictMonitorConflictModel.h"
#include "DMXEditorLog.h"
#include "Commands/DMXConflictMonitorCommands.h"
#include "DMXEditorSettings.h"
#include "DMXEditorStyle.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Application/SlateApplication.h"
#include "IO/DMXConflictMonitor.h"
#include "IO/DMXPortManager.h"
#include "SDMXConflictMonitorToolbar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/SRichTextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXConflictMonitor"

namespace UE::DMX
{
	const FName SDMXConflictMonitor::FColumnIds::Ports = "Ports";
	const FName SDMXConflictMonitor::FColumnIds::Universe = "Universe";
	const FName SDMXConflictMonitor::FColumnIds::Conflicts = "Conflicts";
	const FName SDMXConflictMonitor::FColumnIds::Channels = "Channels";
	
	SDMXConflictMonitor::SDMXConflictMonitor()
		: Status(EDMXConflictMonitorStatus::Idle)
	{}

	void SDMXConflictMonitor::Construct(const FArguments& InArgs)
	{
		SetupCommandList();
		SetCanTick(false);

		ChildSlot
		[
			SNew(SVerticalBox)

			// Toolbar
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.f)
			[
				SNew(SDMXConflictMonitorToolbar, CommandList.ToSharedRef())
				.Status(this, &SDMXConflictMonitor::GetStatus)
				.IsScanning(this, &SDMXConflictMonitor::IsScanning)
				.OnDepthChanged_Lambda([this]()
					{
						Refresh();
					})
			]

			// Log
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(16.f)
			[
				SNew(SScrollBox)
				.Orientation(EOrientation::Orient_Vertical)
					
				+ SScrollBox::Slot()
				.AutoSize()
				[
					SNew(SBorder)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
					[
						SAssignNew(TextBlock, SRichTextBlock)
						.Visibility(EVisibility::HitTestInvisible)
						.AutoWrapText(true)
						.TextStyle(FAppStyle::Get(), "MessageLog")
						.DecoratorStyleSet(&FDMXEditorStyle::Get())
					]
				]
			]
		];

		Refresh();

		const UDMXEditorSettings* EditorSettings = GetDefault<UDMXEditorSettings>();
		if (EditorSettings->ConflictMonitorSettings.bRunWhenOpened)
		{
			Play();
		}
	}

	void SDMXConflictMonitor::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		if (FSlateApplication::Get().AnyMenusVisible())
		{
			return;
		}

		const FDMXConflictMonitor* ConflictMonitor = FDMXConflictMonitor::Get();
		if (!ConflictMonitor)
		{
			return;
		}

		const TMap<FName, TArray<TSharedRef<FDMXMonitoredOutboundDMXData>>> NewOutboundConflicts = ConflictMonitor->GetOutboundConflictsSynchronous();

		if (!CachedOutboundConflicts.OrderIndependentCompareEqual(NewOutboundConflicts) &&
			!FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
		{
			CachedOutboundConflicts = NewOutboundConflicts;
			Refresh();
		}

		// Update Status
		if (Models.IsEmpty())
		{
			Status = EDMXConflictMonitorStatus::OK;
		}
		else
		{
			Status = EDMXConflictMonitorStatus::Conflict;
		}
	}

	void SDMXConflictMonitor::Refresh()
	{
		// Test new items for changes
		TArray<TSharedPtr<FDMXConflictMonitorConflictModel>> NewModels;
		Algo::Transform(CachedOutboundConflicts, NewModels, [](const TPair<FName, TArray<TSharedRef<FDMXMonitoredOutboundDMXData>>>& Conflicts)
			{
				return MakeShared<FDMXConflictMonitorConflictModel>(Conflicts.Value);
			});

		FString NewText;
		uint32 LineCount = 0;
		for (const TSharedPtr<FDMXConflictMonitorConflictModel>& Item : NewModels)
		{
			++LineCount;
			NewText.Append(Item->GetConflictAsString());
			NewText.Append(TEXT("\n"));
		}

		// Auto-pause even if the data hasn't changed
		const UDMXEditorSettings* EditorSettings = GetDefault<UDMXEditorSettings>();
		if (!NewModels.IsEmpty() && IsScanning() && EditorSettings->ConflictMonitorSettings.bAutoPause)
		{
			Pause();
		}

		// Skip if text did not change
		if (TextBlock->GetText().ToString() == NewText)
		{
			return;
		}

		Models = NewModels;
		TextBlock->SetText(FText::FromString(NewText));

		// Log conflicts
		if (bPrintToLog)
		{
			UE_LOG(LogDMXEditor, Log, TEXT("%s"), *NewText);
		}
	}

	void SDMXConflictMonitor::SetupCommandList()
	{
		CommandList = MakeShared<FUICommandList>();

		CommandList->MapAction(FDMXConflictMonitorCommands::Get().StartScan,
			FExecuteAction::CreateSP(this, &SDMXConflictMonitor::Play),
			FCanExecuteAction::CreateLambda([this]
				{
					return !GetCanTick() && !bIsPaused;
				}),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateLambda([this]
				{
					return !GetCanTick() && !bIsPaused;
				})
		);

		CommandList->MapAction(FDMXConflictMonitorCommands::Get().PauseScan,
			FExecuteAction::CreateSP(this, &SDMXConflictMonitor::Pause),
			FCanExecuteAction::CreateLambda([this]
				{
					return GetCanTick();
				}),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateLambda([this]
				{
					return GetCanTick();
				})
		);

		CommandList->MapAction(FDMXConflictMonitorCommands::Get().ResumeScan,
			FExecuteAction::CreateSP(this, &SDMXConflictMonitor::Play),
			FCanExecuteAction::CreateLambda([this]
				{
					return !GetCanTick() && bIsPaused;
				}),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateLambda([this]
				{
					return !GetCanTick() && bIsPaused;
				})
		);

		CommandList->MapAction(FDMXConflictMonitorCommands::Get().StopScan,
			FExecuteAction::CreateSP(this, &SDMXConflictMonitor::Stop),
			FCanExecuteAction::CreateLambda([this]
				{
					return GetCanTick() || bIsPaused;
				})
		);

		CommandList->MapAction(FDMXConflictMonitorCommands::Get().ToggleAutoPause,
			FExecuteAction::CreateSP(this, &SDMXConflictMonitor::ToggleAutoPause),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SDMXConflictMonitor::IsAutoPause)
		);

		CommandList->MapAction(FDMXConflictMonitorCommands::Get().TogglePrintToLog,
			FExecuteAction::CreateSP(this, &SDMXConflictMonitor::TogglePrintToLog),
			FCanExecuteAction::CreateLambda([this]
				{
					return IsAutoPause();
				}),
			FIsActionChecked::CreateSP(this, &SDMXConflictMonitor::IsPrintingToLog)
		);

		CommandList->MapAction(FDMXConflictMonitorCommands::Get().ToggleRunWhenOpened,
			FExecuteAction::CreateSP(this, &SDMXConflictMonitor::ToggleRunWhenOpened),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SDMXConflictMonitor::IsRunWhenOpened)		
		);
	}

	void SDMXConflictMonitor::Play()
	{
		UserSession = FDMXConflictMonitor::Join("SDMXConflictMonitor");

		bIsPaused = false;
		SetCanTick(true);

		// Note, status is updated on tick
	}

	void SDMXConflictMonitor::Pause()
	{
		UserSession.Reset();

		bIsPaused = true;
		SetCanTick(false);
		Status = EDMXConflictMonitorStatus::Idle;
	}

	void SDMXConflictMonitor::Stop()
	{
		UserSession.Reset();

		bIsPaused = false;
		SetCanTick(false);
		Status = EDMXConflictMonitorStatus::Idle;

		CachedOutboundConflicts.Reset();
		Models.Reset();
		Refresh();
	}

	void SDMXConflictMonitor::SetAutoPause(bool bEnabled)
	{
		UDMXEditorSettings* EditorSettings = GetMutableDefault<UDMXEditorSettings>();
		const bool bWasEnabled = EditorSettings->ConflictMonitorSettings.bAutoPause;
		EditorSettings->ConflictMonitorSettings.bAutoPause = bEnabled;

		EditorSettings->SaveConfig();
	}

	void SDMXConflictMonitor::ToggleAutoPause()
	{
		SetAutoPause(!IsAutoPause());
	}

	bool SDMXConflictMonitor::IsAutoPause() const
	{
		const UDMXEditorSettings* EditorSettings = GetDefault<UDMXEditorSettings>();
		return EditorSettings->ConflictMonitorSettings.bAutoPause;
	}

	void SDMXConflictMonitor::SetPrintToLog(bool bEnabled)
	{
		bPrintToLog = bEnabled;
	}

	void SDMXConflictMonitor::TogglePrintToLog()
	{
		bPrintToLog = !bPrintToLog;
	}

	bool SDMXConflictMonitor::IsPrintingToLog() const
	{
		// Only available in auto-pause mode
		return bPrintToLog && IsAutoPause();
	}

	void SDMXConflictMonitor::SetRunWhenOpened(bool bEnabled)
	{
		UDMXEditorSettings* EditorSettings = GetMutableDefault<UDMXEditorSettings>();
		if (EditorSettings->ConflictMonitorSettings.bRunWhenOpened != bEnabled)
		{
			EditorSettings->ConflictMonitorSettings.bRunWhenOpened = bEnabled;
			EditorSettings->SaveConfig();
		}
	}
	
	void SDMXConflictMonitor::ToggleRunWhenOpened()
	{
		SetRunWhenOpened(!IsRunWhenOpened());
	}
	
	bool SDMXConflictMonitor::IsRunWhenOpened() const
	{
		const UDMXEditorSettings* EditorSettings = GetDefault<UDMXEditorSettings>();
		return EditorSettings->ConflictMonitorSettings.bRunWhenOpened;
	}

	bool SDMXConflictMonitor::IsScanning() const
	{
		return GetCanTick() && !bIsPaused;
	}
}

#undef LOCTEXT_NAMESPACE
