// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_AvalanchePlaylist.h"

#include "AvaMediaEditorStyle.h"
#include "ContentBrowserMenuContexts.h"
#include "Misc/MessageDialog.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/AvaPlaylistEditorUtils.h"
#include "Playlist/AvalanchePlaylist.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_AvalanchePlaylist"

FText UAssetDefinition_AvalanchePlaylist::GetAssetDisplayName() const
{
	return LOCTEXT("AvaPlaylistAction_Name", "Motion Design Rundown");
}

TSoftClassPtr<UObject> UAssetDefinition_AvalanchePlaylist::GetAssetClass() const
{
	return UAvalanchePlaylist::StaticClass();
}

FLinearColor UAssetDefinition_AvalanchePlaylist::GetAssetColor() const
{
	static const FName RundownAssetColorName(TEXT("AvalancheMediaEditor.AssetColors.Rundown"));
	return FAvaMediaEditorStyle::Get().GetColor(RundownAssetColorName);
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_AvalanchePlaylist::GetAssetCategories() const
{
	static const TArray<FAssetCategoryPath> Categories = {EAssetCategoryPaths::Blueprint};
	return Categories;
}

EAssetCommandResult UAssetDefinition_AvalanchePlaylist::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	EAssetCommandResult CommandResult = EAssetCommandResult::Unhandled;
	if (OpenArgs.OpenMethod == EAssetOpenMethod::Edit)
	{
		for (UAvalanchePlaylist* Playlist : OpenArgs.LoadObjects<UAvalanchePlaylist>())
		{
			if (Playlist)
			{
				const TSharedRef<FAvaPlaylistEditor> PlaylistEditor = MakeShared<FAvaPlaylistEditor>();
				PlaylistEditor->InitPlaylistEditor(EToolkitMode::Standalone, OpenArgs.ToolkitHost, Playlist);
				CommandResult = EAssetCommandResult::Handled;
			}
		}
	}
	return CommandResult;
}

namespace MenuExtension_AvalanchePlaylist
{
	static bool CanExportToJson(const FToolMenuContext& InContext)
	{
		return true;
	}
	
	static void ExportToJson(const FToolMenuContext& InContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);

		for (UAvalanchePlaylist* Playlist : Context->LoadSelectedObjects<UAvalanchePlaylist>())
		{
			if (Playlist)
			{
				if (Playlist->GetOutermost() && Playlist->GetOutermost()->HasAnyPackageFlags(PKG_DisallowExport))
				{
					UE_LOG(LogAvaPlaylist, Error, TEXT("Package disallow export."));
					continue;
				}

				using namespace UE::AvaPlaylistEditor::Utils;
				FString ExportFilename = GetExportFilepath(Playlist, TEXT("json file"), TEXT("json"));
				if (!ExportFilename.IsEmpty())
				{
					SavePlaylistToJson(Playlist, *ExportFilename);
				}
			}
		}
	}

	static bool CanImportFromJson(const FToolMenuContext& InContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
		for (const UAvalanchePlaylist* Playlist : Context->GetSelectedObjectsInMemory<UAvalanchePlaylist>())
		{
			if (Playlist && Playlist->IsPlaying())
			{
				return false;
			}
		}
		return true;
	}
	
	static void ImportFromJson(const FToolMenuContext& InContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);

		for (UAvalanchePlaylist* Playlist : Context->LoadSelectedObjects<UAvalanchePlaylist>())
		{
			if (Playlist)
			{
				// Ask user to confirm stopping the playlist if it is playing.
				if (Playlist->IsPlaying())
				{
					const FText MessageText = LOCTEXT("StopPagesOnImportQuestion",
						"All pages must be stopped before importing. Some pages are still playing, do you want to stop all pages?");
			
					const EAppReturnType::Type Reply = FMessageDialog::Open(EAppMsgType::YesNoCancel, EAppReturnType::Yes, MessageText);

					if (Reply == EAppReturnType::Cancel)
					{
						return;	// cancel the whole operation.
					}

					if (Reply == EAppReturnType::No)
					{
						UE_LOG(LogAvaPlaylist, Warning, TEXT("Skipping import of rundown \"%s\""), *Playlist->GetFullName());
						continue;
					}
					
					if (Reply == EAppReturnType::Yes)
					{
						Playlist->ClosePlaybackContext(true);
					}
				}
				
				// Ask user to confirm stomping the existing playlist.
				if (!Playlist->IsEmpty())
				{
					const FText MessageText = LOCTEXT("ClearPlaylistOnImportQuestion",
						"The rundown is not empty, all existing content will be overwritten. Are you sure?");
			
					const EAppReturnType::Type Reply = FMessageDialog::Open(EAppMsgType::YesNoCancel, EAppReturnType::Yes, MessageText);
					if (Reply == EAppReturnType::Cancel)
					{
						return;
					}
					if (Reply == EAppReturnType::No)
					{
						continue;
					}
				}
				
				using namespace UE::AvaPlaylistEditor::Utils;
				FString ImportFilename = GetImportFilepath(TEXT("json file"), TEXT("json"));
				if (!ImportFilename.IsEmpty())
				{
					LoadPlaylistFromJson(Playlist, *ImportFilename);
				}
			}
		}
	}

	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []
	{
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
		{
			FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UAvalanchePlaylist::StaticClass());

			FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
			Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				{
					const TAttribute<FText> Label = LOCTEXT("AvaPlaylist_ExportToJson", "Export to Json");
					const TAttribute<FText> ToolTip = LOCTEXT("AvaPlaylist_ExportToJsonTooltip", "Export To Json");

					FToolUIAction UIAction;
					UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExportToJson);
					UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&CanExportToJson);
					InSection.AddMenuEntry("AvaPlaylist_ExportToJson", Label, ToolTip, FSlateIcon(), UIAction);
				}
				{
					const TAttribute<FText> Label = LOCTEXT("AvaPlaylist_ImportFromJson", "Import from Json");
					const TAttribute<FText> ToolTip = LOCTEXT("AvaPlaylist_ImportFromJsonTooltip", "Import from Json");

					FToolUIAction UIAction;
					UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ImportFromJson);
					UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&CanImportFromJson);
					InSection.AddMenuEntry("AvaPlaylist_ImportFromJson", Label, ToolTip, FSlateIcon(), UIAction);
				}
			}));
		}));
	});
}

#undef LOCTEXT_NAMESPACE
