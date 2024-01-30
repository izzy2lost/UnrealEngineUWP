// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_AvalanchePlayback.h"

#include "AvaMediaEditorStyle.h"
#include "ContentBrowserMenuContexts.h"
#include "Playback/AvaPlaybackEditor.h"
#include "Playback/AvalanchePlayback.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_AvalanchePlaylist"

FText UAssetDefinition_AvalanchePlayback::GetAssetDisplayName() const
{
	return LOCTEXT("AvaPlaybackAction_Name", "Motion Design Playback");
}

TSoftClassPtr<UObject> UAssetDefinition_AvalanchePlayback::GetAssetClass() const
{
	return UAvalanchePlayback::StaticClass();
}

FLinearColor UAssetDefinition_AvalanchePlayback::GetAssetColor() const
{
	static const FName PlaybackAssetColorName(TEXT("AvalancheMediaEditor.AssetColors.Playback"));
	return FAvaMediaEditorStyle::Get().GetColor(PlaybackAssetColorName);
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_AvalanchePlayback::GetAssetCategories() const
{
	static const TArray<FAssetCategoryPath> Categories = {EAssetCategoryPaths::Blueprint};
	return Categories;
}

EAssetCommandResult UAssetDefinition_AvalanchePlayback::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	EAssetCommandResult CommandResult = EAssetCommandResult::Unhandled;
	if (OpenArgs.OpenMethod == EAssetOpenMethod::Edit)
	{
		for (UAvalanchePlayback* Playback : OpenArgs.LoadObjects<UAvalanchePlayback>())
		{
			if (Playback)
			{
				const TSharedRef<FAvaPlaybackEditor> PlaybackEditor = MakeShared<FAvaPlaybackEditor>();
				PlaybackEditor->InitPlaybackEditor(EToolkitMode::Standalone, OpenArgs.ToolkitHost, Playback);
				CommandResult = EAssetCommandResult::Handled;
			}
		}
	}
	return CommandResult;
}

namespace MenuExtension_AvalanchePlayback
{
	/** Count selected assets of the given type, loaded or not. */
	template<typename ExpectedAssetType>
	int32 GetNumSelectedAssets(const UContentBrowserAssetContextMenuContext* Context)
	{
		int32 NumSelectedAssets = 0;
		for (const FAssetData& Asset : Context->SelectedAssets)
		{
			if (Asset.IsInstanceOf(ExpectedAssetType::StaticClass()))
			{
				++NumSelectedAssets;
			}
		}
		return NumSelectedAssets;
	}

	/** Returns the count of playbacks that are loaded and playing. Will not load unloaded assets. */
	static int32 GetNumSelectedPlaybackPlaying(const UContentBrowserAssetContextMenuContext* Context)
	{
		int32 NumPlaying = 0;
		for (const FAssetData& Asset : Context->SelectedAssets)
		{
			if (Asset.IsInstanceOf(UAvalanchePlayback::StaticClass()))
			{
				if (const UAvalanchePlayback* Playback = Cast<UAvalanchePlayback>(Asset.FastGetAsset(false)))
				{
					if (Playback->IsPlaying())
					{
						++NumPlaying;
					}
				}
			}
		}
		return NumPlaying;
	}
	
	static bool CanExecutePlay(const FToolMenuContext& InContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
		return GetNumSelectedAssets<UAvalanchePlayback>(Context) > GetNumSelectedPlaybackPlaying(Context);
	}

	static void ExecutePlay(const FToolMenuContext& InContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);

		for (UAvalanchePlayback* Playback : Context->LoadSelectedObjects<UAvalanchePlayback>())
		{
			if (Playback && !Playback->IsPlaying())
			{
				Playback->Play();
			}
		}
	}

	static bool CanExecuteStop(const FToolMenuContext& InContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
		return GetNumSelectedPlaybackPlaying(Context) > 0;
	}

	static void ExecuteStop(const FToolMenuContext& InContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);

		for (UAvalanchePlayback* Playback : Context->LoadSelectedObjects<UAvalanchePlayback>())
		{
			if (Playback && Playback->IsPlaying())
			{
				Playback->Stop(EAvaPlaybackStopOptions::Default);
			}
		}
	}

	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []
	{
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
		{
			FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UAvalanchePlayback::StaticClass());

			FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
			Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				{
					const TAttribute<FText> Label = LOCTEXT("AvalanchePlayback_Play", "Play");
					const TAttribute<FText> ToolTip = LOCTEXT("AvalanchePlayback_PlayTooltip", "Play");
					const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericPlay");

					FToolUIAction UIAction;
					UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecutePlay);
					UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&CanExecutePlay);
					InSection.AddMenuEntry("AvalanchePlayback_Play", Label, ToolTip, Icon, UIAction);
				}
				{
					const TAttribute<FText> Label = LOCTEXT("AvalanchePlayback_Stop", "Stop");
					const TAttribute<FText> ToolTip = LOCTEXT("AvalanchePlayback_Stop", "Stop");
					const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericPause");

					FToolUIAction UIAction;
					UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteStop);
					UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&CanExecuteStop);
					InSection.AddMenuEntry("AvalanchePlayback_Stop", Label, ToolTip, Icon, UIAction);
				}
			}));
		}));
	});
}


#undef LOCTEXT_NAMESPACE
