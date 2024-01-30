// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMRQEditorModule.h"
#include "AvaMRQEditorCommands.h"
#include "AvaMRQEditorPlaylistUtils.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "IAvaMediaEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/AvalanchePlaylist.h"
#include "Toolkits/AssetEditorToolkit.h"

void FAvaMRQEditorModule::StartupModule()
{
	FAvaMRQEditorCommands::Register();

	TSharedPtr<FExtensibilityManager> PlaylistToolbarExtensibility = IAvaMediaEditorModule::Get().GetPlaylistToolBarExtensibilityManager();
	if (PlaylistToolbarExtensibility.IsValid())
	{
		PlaylistToolbarExtensibilityWeak = PlaylistToolbarExtensibility;

		TArray<FAssetEditorExtender>& ExtenderDelegates = PlaylistToolbarExtensibility->GetExtenderDelegates();
		ExtenderDelegates.Add(FAssetEditorExtender::CreateStatic(&FAvaMRQEditorModule::ExtendPlaylistToolbar));

		PlaylistToolbarExtenderHandle = ExtenderDelegates.Last().GetHandle();
	}
}

void FAvaMRQEditorModule::ShutdownModule()
{
	FAvaMRQEditorCommands::Unregister();

	if (TSharedPtr<FExtensibilityManager> PlaylistToolbarExtensibility = PlaylistToolbarExtensibilityWeak.Pin())
	{
		if (PlaylistToolbarExtenderHandle.IsValid())
		{
			PlaylistToolbarExtensibility->GetExtenderDelegates().RemoveAll([this](const FAssetEditorExtender& InExtender)
			{
				return InExtender.GetHandle() == PlaylistToolbarExtenderHandle;
			});
			PlaylistToolbarExtenderHandle.Reset();
		}
		PlaylistToolbarExtensibilityWeak.Reset();
	}
}

TSharedRef<FExtender> FAvaMRQEditorModule::ExtendPlaylistToolbar(const TSharedRef<FUICommandList> InCommandList, const TArray<UObject*> InObjects)
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();

	UAssetEditorSubsystem* const AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (!AssetEditorSubsystem)
	{
		return Extender;
	}

	TSharedRef<FAvaMRQPlaylistContext> Context = MakeShared<FAvaMRQPlaylistContext>();
	Context->PlaylistEditors.Reserve(InObjects.Num());

	const FName PlaylistEditorName = TEXT("AvaPlaylistEditor");

	// Gather Playlist Editors
	for (UObject* Object : InObjects)
	{
		if (UAvalanchePlaylist* Playlist = Cast<UAvalanchePlaylist>(Object))
		{
			IAssetEditorInstance* AssetEditor = AssetEditorSubsystem->FindEditorForAsset(Playlist, /*bFocusIfOpen*/false);
			if (!AssetEditor || AssetEditor->GetEditorName() != PlaylistEditorName)
			{
				continue;
			}

			FAvaPlaylistEditor* PlaylistEditor = static_cast<FAvaPlaylistEditor*>(AssetEditor);
			Context->PlaylistEditors.Add(StaticCastWeakPtr<FAvaPlaylistEditor>(PlaylistEditor->AsWeak()));
		}
	}

	if (Context->PlaylistEditors.IsEmpty())
	{
		return Extender;
	}

	Extender->AddToolBarExtension("Pages"
		, EExtensionHook::After
		, CreatePlaylistActions(Context)
		, FToolBarExtensionDelegate::CreateLambda([](FToolBarBuilder& ToolbarBuilder)
		{
			const FAvaMRQEditorCommands& MRQEditorCommands = FAvaMRQEditorCommands::Get();
			ToolbarBuilder.AddToolBarButton(MRQEditorCommands.RenderSelectedPages);
		})
	);

	return Extender;
}

TSharedRef<FUICommandList> FAvaMRQEditorModule::CreatePlaylistActions(TSharedRef<FAvaMRQPlaylistContext> InContext)
{
	const FAvaMRQEditorCommands& MRQEditorCommands = FAvaMRQEditorCommands::Get();

	TSharedRef<FUICommandList> CommandList = MakeShared<FUICommandList>();

	static auto HasAnySelectedPage = [](const TWeakPtr<const FAvaPlaylistEditor>& InPlaylistEditorWeak)
	{
		TSharedPtr<const FAvaPlaylistEditor> PlaylistEditor = InPlaylistEditorWeak.Pin();
		return PlaylistEditor.IsValid() && PlaylistEditor->GetSelectedPagesOnActiveSubListWidget().Num() > 0;
	};

	FCanExecuteAction CanExecuteSelectedPageAction = FCanExecuteAction::CreateLambda([InContext]
	{
		return InContext->PlaylistEditors.ContainsByPredicate(HasAnySelectedPage);
	});

	CommandList->MapAction(MRQEditorCommands.RenderSelectedPages
		, FExecuteAction::CreateLambda([InContext]{ FAvaMRQEditorPlaylistUtils::RenderSelectedPages(InContext->PlaylistEditors); })
		, CanExecuteSelectedPageAction);

	return CommandList;
}

IMPLEMENT_MODULE(FAvaMRQEditorModule, AvalancheMRQEditor)
