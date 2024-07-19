// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPresetComboButton.h"

#include "Assets/MultiUserReplicationSessionPreset.h"
#include "Replication/Preset/PresetManager.h"

#include "Containers/Ticker.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "SSimpleComboButton.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SPresetComboButton"

namespace UE::MultiUserClient
{
	namespace Private
	{
		static FText MakeTitle(const FReplaceSessionContentResult& Result, const FText& PresetText)
		{
			const FText Format = Result.IsSuccess()
				? LOCTEXT("ApplyPreset.Title.SuccessFmt", "Applied {0} preset")
				: LOCTEXT("ApplyPreset.Title.FailFmt", "Failed to apply {0} preset");
			return FText::Format(Format, PresetText);
		}

		static FText MakeSubText(const FReplaceSessionContentResult& Result)
		{
			switch (Result.ErrorCode)
			{
			case EReplaceSessionContentErrorCode::Success: return FText::GetEmpty();
			case EReplaceSessionContentErrorCode::Cancelled: return LOCTEXT("ApplyPreset.SubText.Success", "Disconnected from session.");
			case EReplaceSessionContentErrorCode::InProgress: return LOCTEXT("ApplyPreset.SubText.InProgress", "Another operation is already in progress.");
			case EReplaceSessionContentErrorCode::Timeout: return LOCTEXT("ApplyPreset.SubText.Timeout", "Request timed out.");
			case EReplaceSessionContentErrorCode::FeatureDisabled: return LOCTEXT("ApplyPreset.SubText.FeatureDisabled", "This session does not support presets.");
			case EReplaceSessionContentErrorCode::Rejected: return LOCTEXT("ApplyPreset.SubText.Rejected", "Rejected by server.");
			default: checkNoEntry(); return FText::GetEmpty();
			}
		}
	}
	
	void SPresetComboButton::Construct(const FArguments& InArgs, FPresetManager& InPresetManager)
	{
		PresetManager = &InPresetManager;
		ChildSlot
		[
			SNew(SSimpleComboButton)
			.Icon(FAppStyle::Get().GetBrush("AssetEditor.SaveAsset"))
			.Text(LOCTEXT("Presets", "Presets"))
			.OnGetMenuContent(this, &SPresetComboButton::CreateMenuContent)
			.HasDownArrow(true)
		];
	}

	TSharedRef<SWidget> SPresetComboButton::CreateMenuContent()
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		CreateSaveMenuContent(MenuBuilder);

		MenuBuilder.BeginSection(NAME_None, LOCTEXT("Section.Import", "Import preset"));
		CreateLoadMenuContent(MenuBuilder);
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	void SPresetComboButton::CreateSaveMenuContent(FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SavePresetAs.Label", "Save Preset as..."),
			LOCTEXT("SavePresetAs.ToolTip", "Saves what each client was replicating as a preset."),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "AssetEditor.SaveAssetAs"),
			FUIAction(FExecuteAction::CreateSP(this, &SPresetComboButton::SavePresetAs))	
		);
	}

	void SPresetComboButton::CreateLoadMenuContent(FMenuBuilder& MenuBuilder)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ImportPreset.ClearOtherClients.Label", "Clear clients not in preset"),
			LOCTEXT("ImportPreset.ClearOtherClients.ToolTip", "If checked, clients that were not in the session when the preset was created will get their content reset, too."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this](){ Options.bResetAllOtherClients = !Options.bResetAllOtherClients; }),
				FCanExecuteAction::CreateLambda([](){ return true; }),
				FIsActionChecked::CreateLambda([this]{ return Options.bResetAllOtherClients; })
				),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
			);
			
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		FAssetPickerConfig AssetPickerConfig;
		AssetPickerConfig.Filter.ClassPaths.Add(UMultiUserReplicationSessionPreset::StaticClass()->GetClassPathName());
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.Filter.bRecursiveClasses = false;
		AssetPickerConfig.OnAssetSelected.BindSP(this, &SPresetComboButton::LoadPreset);
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.bAllowDragging = false;
		
		TSharedRef<SWidget> PresetPicker = SNew(SBox)
			.HeightOverride(400)
			.WidthOverride(300)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Menu.Background"))
				[
					ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
				]
			];
		MenuBuilder.AddWidget(PresetPicker, FText(), true, false);
	}

	void SPresetComboButton::SavePresetAs()
	{
		PresetManager->ExportToPresetAndSaveAs();
	}

	void SPresetComboButton::LoadPreset(const FAssetData& AssetData)
	{
		FSlateNotificationManager& NotificationManager = FSlateNotificationManager::Get();
		if (UMultiUserReplicationSessionPreset* Preset = Cast<UMultiUserReplicationSessionPreset>(AssetData.GetAsset()))
		{
			FText PresetText = FText::FromName(Preset->GetFName());
			FNotificationInfo Info(FText::Format(LOCTEXT("ApplyPreset.Title.InProgressFmt", "Applying preset {0}"), PresetText));
			Info.ExpireDuration = 4.f;
			TSharedPtr<SNotificationItem> Notification = NotificationManager.AddNotification(Info);
			Notification->SetCompletionState(SNotificationItem::CS_Pending);

			PresetManager->ReplaceSessionContentWithPreset(*Preset, BuildFlags())
				.Next([PresetText = MoveTemp(PresetText), Notification = MoveTemp(Notification)](const FReplaceSessionContentResult& Result) mutable
				{
					ExecuteOnGameThread(TEXT("SPresetComboButton"), [PresetText = MoveTemp(PresetText), Notification = MoveTemp(Notification), Result]()
					{
						Notification->SetText(Private::MakeTitle(Result, PresetText));
						Notification->SetSubText(Private::MakeSubText(Result));
						Notification->SetCompletionState(Result.IsSuccess() ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
					});
				});
		}
		else
		{
			FNotificationInfo Info(LOCTEXT("FailedToLoad", "Failed to load preset"));
			Info.ExpireDuration = 4.f;
			NotificationManager.AddNotification(Info)
				->SetCompletionState(SNotificationItem::CS_Fail);
		}
	}

	EApplyPresetFlags SPresetComboButton::BuildFlags() const
	{
		EApplyPresetFlags Flags = EApplyPresetFlags::None;

		if (Options.bResetAllOtherClients)
		{
			Flags |= EApplyPresetFlags::ClearUnreferencedClients;
		}

		return Flags;
	}
}

#undef LOCTEXT_NAMESPACE