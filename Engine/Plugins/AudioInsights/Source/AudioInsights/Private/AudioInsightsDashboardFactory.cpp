// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioInsightsDashboardFactory.h"

#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "AudioInsightsModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Framework/Docking/TabManager.h"
#include "Internationalization/Text.h"
#include "IPropertyTypeCustomization.h"
#include "Kismet2/DebuggerCommands.h"
#include "Templates/SharedPointer.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "AudioInsights"


namespace UE::Audio::Insights
{
	namespace DashboardFactoryPrivate
	{
		static const FText ToolName = LOCTEXT("AudioDashboard_ToolName", "Audio Dashboard");

		static const FLazyName MainToolbarName = "MainToolbar";
		static const FText MainToolbarDisplayName = LOCTEXT("AudioDashboard_MainToolbarDisplayName", "Dashboard Transport");

		static const FText PreviewDeviceDisplayName = LOCTEXT("AudioDashboard_PreviewDevice", "[Preview Audio]");
		static const FText DashboardWorldSelectDescription = LOCTEXT("AudioDashboard_SelectWorldDescription", "Select world(s) to monitor (worlds may share audio output).");

		FText GetDebugNameFromDeviceId(::Audio::FDeviceId InDeviceId)
		{
			FString WorldName;
			if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
			{
				TArray<UWorld*> DeviceWorlds = DeviceManager->GetWorldsUsingAudioDevice(InDeviceId);
				for (const UWorld* World : DeviceWorlds)
				{
					if (!WorldName.IsEmpty())
					{
						WorldName += TEXT(", ");
					}
					WorldName += World->GetDebugDisplayName();
				}
			}

			if (WorldName.IsEmpty())
			{
				return PreviewDeviceDisplayName;
			}

			return FText::FromString(WorldName);
		}
	} // namespace DashboardFactoryPrivate

	FDashboardFactory::FDashboardFactory()
	{
	}

	void FDashboardFactory::OnWorldRegisteredToAudioDevice(const UWorld* InWorld, ::Audio::FDeviceId InDeviceId)
	{
		if (InDeviceId != INDEX_NONE)
		{
			if (bStartWithPIE)
			{
				const FTraceModule& TraceModule = FAudioInsightsModule::GetChecked().GetTraceModule();
				TraceModule.StartTraceAnalysis();
				ActiveDeviceId = InDeviceId;
			}
		}

		RefreshDeviceSelector();
	}

	void FDashboardFactory::OnPIEStarted(bool bSimulating)
	{
		if (bStartWithPIE)
		{
			const FTraceModule& TraceModule = FAudioInsightsModule::GetChecked().GetTraceModule();
			TraceModule.StartTraceAnalysis();
		}
	}

	void FDashboardFactory::OnPostPIEStarted(bool bSimulating)
	{
		OnActiveAudioDeviceChanged.Broadcast();
	}

	void FDashboardFactory::OnPIEStopped(bool bSimulating)
	{
		if (bStopWithPIE)
		{
			const FTraceModule& TraceModule = FAudioInsightsModule::GetChecked().GetTraceModule();
			TraceModule.StopTraceAnalysis();

		}

		RefreshDeviceSelector();
	}

	void FDashboardFactory::OnWorldUnregisteredFromAudioDevice(const UWorld* InWorld, ::Audio::FDeviceId InDeviceId)
	{
		RefreshDeviceSelector();
	}

	void FDashboardFactory::OnDeviceDestroyed(::Audio::FDeviceId InDeviceId)
	{
		if (ActiveDeviceId == InDeviceId)
		{
			if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
			{
				ActiveDeviceId = DeviceManager->GetMainAudioDeviceID();
			}
		}

		AudioDeviceIds.RemoveAll([InDeviceId](const TSharedPtr<::Audio::FDeviceId>& DeviceIdPtr)
		{
			return *DeviceIdPtr.Get() == InDeviceId;
		});

		if (AudioDeviceComboBox.IsValid())
		{
			AudioDeviceComboBox->RefreshOptions();
		}

		OnActiveAudioDeviceChanged.Broadcast();
	}

	void FDashboardFactory::RefreshDeviceSelector()
	{
		if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
		{
			if (!DeviceManager->IsValidAudioDevice(ActiveDeviceId))
			{
				ActiveDeviceId = DeviceManager->GetMainAudioDeviceID();
			}
		}

		AudioDeviceIds.Empty();
		if (const FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
		{
			DeviceManager->IterateOverAllDevices([this, &DeviceManager](::Audio::FDeviceId DeviceId, const FAudioDevice* AudioDevice)
			{
				AudioDeviceIds.Add(MakeShared<::Audio::FDeviceId>(DeviceId));
			});
		}

		if (AudioDeviceComboBox.IsValid())
		{
			AudioDeviceComboBox->RefreshOptions();
		}
	}

	void FDashboardFactory::ResetDelegates()
	{
		if (OnWorldRegisteredToAudioDeviceHandle.IsValid())
		{
			FAudioDeviceWorldDelegates::OnWorldRegisteredToAudioDevice.Remove(OnWorldRegisteredToAudioDeviceHandle);
			OnWorldRegisteredToAudioDeviceHandle.Reset();
		}

		if (OnWorldUnregisteredFromAudioDeviceHandle.IsValid())
		{
			FAudioDeviceWorldDelegates::OnWorldUnregisteredWithAudioDevice.Remove(OnWorldUnregisteredFromAudioDeviceHandle);
			OnWorldUnregisteredFromAudioDeviceHandle.Reset();
		}

		if (OnDeviceDestroyedHandle.IsValid())
		{
			FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.Remove(OnDeviceDestroyedHandle);
			OnDeviceDestroyedHandle.Reset();
		}

		if (OnPIEStartedHandle.IsValid())
		{
			FEditorDelegates::PreBeginPIE.Remove(OnPIEStartedHandle);
			OnPIEStartedHandle.Reset();
		}

		if (OnPostPIEStartedHandle.IsValid())
		{
			FEditorDelegates::PostPIEStarted.Remove(OnPostPIEStartedHandle);
			OnPostPIEStartedHandle.Reset();
		}

		if (OnPIEStoppedHandle.IsValid())
		{
			FEditorDelegates::EndPIE.Remove(OnPIEStoppedHandle);
			OnPIEStoppedHandle.Reset();
		}
	}

	::Audio::FDeviceId FDashboardFactory::GetDeviceId() const
	{
		return ActiveDeviceId;
	}

	TSharedRef<SDockTab> FDashboardFactory::MakeDockTabWidget(const FSpawnTabArgs& Args)
	{
		UnregisterTabSpawners();

		InitDelegates();

		InitTabLayout();

		const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
			.Label(DashboardFactoryPrivate::ToolName)
			.Clipping(EWidgetClipping::ClipToBounds)
			.TabRole(ETabRole::NomadTab);

		TSharedPtr<SWindow> Window = Args.GetOwnerWindow();

		DashboardTabManager = FGlobalTabmanager::Get()->NewTabManager(DockTab);

		RegisterTabSpawners();
		RefreshDeviceSelector();

		TSharedPtr<SWidget> Content = DashboardTabManager->RestoreFrom(TabLayout->AsShared(), Window);
		DockTab->SetContent(Content->AsShared());
		DockTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([this](TSharedRef<SDockTab> TabClosed)
		{
			ResetDelegates();
		}));
		return DockTab;
	}

	TSharedRef<SWidget> FDashboardFactory::MakeMainToolbarWidget()
	{
		static const FName PlayWorldToolBarName = "Kismet.DebuggingViewToolBar";
		if (!UToolMenus::Get()->IsMenuRegistered(PlayWorldToolBarName))
		{
			UToolMenu* ToolBar = UToolMenus::Get()->RegisterMenu(PlayWorldToolBarName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);
			FToolMenuSection& Section = ToolBar->AddSection("Debug");
			FPlayWorldCommands::BuildToolbar(Section);
		}

		return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("NoBorder"))
			[
				UToolMenus::Get()->GenerateWidget(PlayWorldToolBarName, { FPlayWorldCommands::GlobalPlayWorldActions })
			]
		]

		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("StartOnPIE_DisplayName", "Start with PIE:"))
			.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this]() { return bStartWithPIE ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { bStartWithPIE = NewState == ECheckBoxState::Checked; })
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("StopOnPIE_DisplayName", "Stop with PIE:"))
			.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this]() { return bStopWithPIE ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { bStopWithPIE = NewState == ECheckBoxState::Checked; })
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(2.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SelectDashboardWorld_DisplayName", "World Filter:"))
			.ToolTipText(DashboardFactoryPrivate::DashboardWorldSelectDescription)
			.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SAssignNew(AudioDeviceComboBox, SComboBox<TSharedPtr<::Audio::FDeviceId>>)
			.ToolTipText(DashboardFactoryPrivate::DashboardWorldSelectDescription)
			.OptionsSource(&AudioDeviceIds)
			.OnGenerateWidget_Lambda([](const TSharedPtr<::Audio::FDeviceId>& WidgetDeviceId)
			{
				FText NameText = DashboardFactoryPrivate::GetDebugNameFromDeviceId(*WidgetDeviceId);
				return SNew(STextBlock)
					.Text(NameText)
					.Font(IPropertyTypeCustomizationUtils::GetRegularFont());
			})
			.OnSelectionChanged_Lambda([this](TSharedPtr<::Audio::FDeviceId> NewDeviceId, ESelectInfo::Type)
			{
				if (NewDeviceId.IsValid())
				{
					ActiveDeviceId = *NewDeviceId;
					RefreshDeviceSelector();

					OnActiveAudioDeviceChanged.Broadcast();
				}
			})
			[
				SNew(STextBlock)
				.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
				.Text_Lambda([this]()
				{
					return DashboardFactoryPrivate::GetDebugNameFromDeviceId(ActiveDeviceId);
				})
			]
		];
	}

	void FDashboardFactory::InitDelegates()
	{
		if (!OnWorldRegisteredToAudioDeviceHandle.IsValid())
		{
			OnWorldRegisteredToAudioDeviceHandle = FAudioDeviceWorldDelegates::OnWorldRegisteredToAudioDevice.AddSP(this, &FDashboardFactory::OnWorldRegisteredToAudioDevice);
		}

		if (!OnWorldUnregisteredFromAudioDeviceHandle.IsValid())
		{
			OnWorldUnregisteredFromAudioDeviceHandle = FAudioDeviceWorldDelegates::OnWorldUnregisteredWithAudioDevice.AddSP(this, &FDashboardFactory::OnWorldUnregisteredFromAudioDevice);
		}

		if (!OnDeviceDestroyedHandle.IsValid())
		{
			OnDeviceDestroyedHandle = FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.AddSP(this, &FDashboardFactory::OnDeviceDestroyed);
		}

		if (!OnPIEStartedHandle.IsValid())
		{
			OnPIEStartedHandle = FEditorDelegates::PreBeginPIE.AddSP(this, &FDashboardFactory::OnPIEStarted);
		}

		if (!OnPostPIEStartedHandle.IsValid())
		{
			OnPostPIEStartedHandle = FEditorDelegates::PostPIEStarted.AddSP(this, &FDashboardFactory::OnPostPIEStarted);
		}

		if (!OnPIEStoppedHandle.IsValid())
		{
			OnPIEStoppedHandle = FEditorDelegates::EndPIE.AddSP(this, &FDashboardFactory::OnPIEStopped);
		}
	}

	void FDashboardFactory::InitTabLayout()
	{
		using namespace DashboardFactoryPrivate;

		TabLayout.Reset();

		TSharedRef<FTabManager::FStack> MainMenuTabStack = FTabManager::NewStack();
		TSharedRef<FTabManager::FStack> ViewportTabStack = FTabManager::NewStack();
		TSharedRef<FTabManager::FStack> LogTabStack = FTabManager::NewStack();
		TSharedRef<FTabManager::FStack> AnalysisTabStack = FTabManager::NewStack();
		TSharedRef<FTabManager::FStack> AudioMetersTabStack = FTabManager::NewStack();
		TSharedRef<FTabManager::FStack> AudioMeterTabStack = FTabManager::NewStack();
		TSharedRef<FTabManager::FStack> OscilloscopeTabStack = FTabManager::NewStack();

		MainMenuTabStack->AddTab(MainToolbarName, ETabState::OpenedTab);

		for (const TPair<FName, TSharedPtr<IDashboardViewFactory>>& Factory : DashboardViewFactories)
		{
			EDefaultDashboardTabStack DefaultTabStack = Factory.Value->GetDefaultTabStack();
			switch (DefaultTabStack)
			{
				case EDefaultDashboardTabStack::Viewport:
				{
					ViewportTabStack->AddTab(Factory.Key, ETabState::OpenedTab);
				}
				break;

				case EDefaultDashboardTabStack::Log:
				{
					LogTabStack->AddTab(Factory.Key, ETabState::OpenedTab);
				}
				break;

				case EDefaultDashboardTabStack::Analysis:
				{
					AnalysisTabStack->AddTab(Factory.Key, ETabState::OpenedTab);
				}
				break;

				case EDefaultDashboardTabStack::AudioMeters:
				{
					AudioMetersTabStack->AddTab(Factory.Key, ETabState::OpenedTab);
				}
				break;

				case EDefaultDashboardTabStack::AudioMeter:
				{
					AudioMeterTabStack->AddTab(Factory.Key, ETabState::OpenedTab);
				}
				break;

				case EDefaultDashboardTabStack::Oscilloscope:
				{
					OscilloscopeTabStack->AddTab(Factory.Key, ETabState::OpenedTab);
				}
				break;

				default:
					break;
			}
		}

		AnalysisTabStack->SetForegroundTab(FName("MixerSources"));

		TabLayout = FTabManager::NewLayout("AudioDashboard_Layout_v1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				MainMenuTabStack
				->SetSizeCoefficient(0.065)
				->SetHideTabWell(true)
			)
			->Split
			(
				// Left column
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					// Top
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.25f) // Column width
					->Split
					(
						ViewportTabStack
						->SetSizeCoefficient(0.5f)
					)
					// Bottom
					->Split
					(
						LogTabStack
						->SetSizeCoefficient(0.5f)
					)
				)
				
				// Middle column
				->Split
				(
					// Top
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.6f) // Column width
					->Split
					(
						FTabManager::NewSplitter()
						->SetOrientation(Orient_Horizontal)
						->Split
						(
							AnalysisTabStack
							->SetSizeCoefficient(0.58f)
						)
					)
					// Bottom
					->Split
					(
						AudioMetersTabStack
						->SetSizeCoefficient(0.42f)
					)
				)
				// Right column
				->Split
				(
					// Top
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.15f) // Column width
					->Split
					(
						AudioMeterTabStack
						->SetSizeCoefficient(0.7f)
						->SetHideTabWell(true)
					)
					// Bottom
					->Split
					(
						OscilloscopeTabStack
						->SetSizeCoefficient(0.3f)
						->SetHideTabWell(true)
					)
				)
			)
		);
	}

	void FDashboardFactory::RegisterTabSpawners()
	{
		DashboardWorkspace = DashboardTabManager->AddLocalWorkspaceMenuCategory(DashboardFactoryPrivate::ToolName);
		DashboardTabManager->RegisterTabSpawner(DashboardFactoryPrivate::MainToolbarName, FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs& Args)
		{
			return SNew(SDockTab)
				.Clipping(EWidgetClipping::ClipToBounds)
				.Label(DashboardFactoryPrivate::MainToolbarDisplayName)
				[
					MakeMainToolbarWidget()
				];
		}))
		.SetDisplayName(DashboardFactoryPrivate::MainToolbarDisplayName)
		.SetGroup(DashboardWorkspace->AsShared());

		for (const TPair<FName, TSharedPtr<IDashboardViewFactory>>& KVP : DashboardViewFactories)
		{
			const FName FactoryName = KVP.Value->GetName();
			DashboardTabManager->RegisterTabSpawner(FactoryName, FOnSpawnTab::CreateLambda([this, Factory = KVP.Value](const FSpawnTabArgs& Args)
			{
				TSharedPtr<SWidget> DashboardView = Factory->MakeWidget();
				return SNew(SDockTab)
					.Clipping(EWidgetClipping::ClipToBounds)
					.Label(Factory->GetDisplayName())
					[
						DashboardView->AsShared()
					];
			}))
			.SetDisplayName(KVP.Value->GetDisplayName())
			.SetGroup(DashboardWorkspace->AsShared())
			.SetIcon(KVP.Value->GetIcon());
		}
	}

	void FDashboardFactory::RegisterViewFactory(TSharedRef<IDashboardViewFactory> InFactory)
	{
		const FName Name = InFactory->GetName();
		if (ensureAlwaysMsgf(!DashboardViewFactories.Contains(Name), TEXT("Failed to register Audio Dashboard '%s': Dashboard with name already registered"), *Name.ToString()))
		{
			DashboardViewFactories.Add(Name, InFactory);
		}
	}

	void FDashboardFactory::UnregisterViewFactory(FName InName)
	{
		DashboardViewFactories.Remove(InName);
	}

	void FDashboardFactory::UnregisterTabSpawners()
	{
		using namespace DashboardFactoryPrivate;

		if (DashboardTabManager.IsValid())
		{
			for (const TPair<FName, TSharedPtr<IDashboardViewFactory>>& Factory : DashboardViewFactories)
			{
				DashboardTabManager->UnregisterTabSpawner(Factory.Value->GetName());
			}

			DashboardTabManager.Reset();
		}

		DashboardWorkspace.Reset();
	}
} // namespace UE::Audio::Insights
#undef LOCTEXT_NAMESPACE
