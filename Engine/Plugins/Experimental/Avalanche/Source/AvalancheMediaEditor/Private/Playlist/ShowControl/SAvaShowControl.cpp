// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaShowControl.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Playback/AvalanchePlayback.h"
#include "Playlist/AvaPlaylistCommands.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/Pages/Slate/SAvaInstancedPageList.h"
#include "Playlist/Pages/Slate/SAvaPageList.h"
#include "Playlist/TabFactories/AvaSubListDocumentTabFactory.h"

#define LOCTEXT_NAMESPACE "SAvaShowControl"

void SAvaShowControl::Construct(const FArguments& InArgs, const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor)
{
	PlaylistEditor = InPlaylistEditor;
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			BuildShowControlToolBar(InPlaylistEditor->GetToolkitCommands())
		]
	];
}

TSharedRef<SWidget> SAvaShowControl::BuildShowControlToolBar(const TSharedRef<FUICommandList>& InCommandList)
{
	FSlimHorizontalToolBarBuilder ToolBarBuilder(InCommandList, FMultiBoxCustomization::None);
	ToolBarBuilder.SetStyle(&FAvaMediaEditorStyle::Get(), "AvalancheMediaEditor.ToolBar");

	const FAvaPlaylistCommands& PlaylistCommands = FAvaPlaylistCommands::Get();

	ToolBarBuilder.BeginSection(TEXT("ShowControl"));
	{
		ToolBarBuilder.BeginStyleOverride("AvalancheMediaEditor.ToolBarRedButtonOverride");
		{
			ToolBarBuilder.AddToolBarButton(PlaylistCommands.Play);
		}
		ToolBarBuilder.EndStyleOverride();

		ToolBarBuilder.AddToolBarButton(PlaylistCommands.Continue);
		ToolBarBuilder.AddToolBarButton(PlaylistCommands.Stop);

		ToolBarBuilder.BeginStyleOverride("AvalancheMediaEditor.ToolBarRedButtonOverride");
		{
			ToolBarBuilder.AddToolBarButton(PlaylistCommands.PlayNext);
		}
		ToolBarBuilder.EndStyleOverride();

		ToolBarBuilder.AddSeparator();
		ToolBarBuilder.AddWidget(CreateActiveListWidget());
		ToolBarBuilder.AddSeparator();
		ToolBarBuilder.AddWidget(CreateNextPageWidget());
	}
	ToolBarBuilder.EndSection();

	return ToolBarBuilder.MakeWidget();
}

TSharedRef<SWidget> SAvaShowControl::CreateActiveListWidget()
{
	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(5.f, 0.f)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ActiveView", "Active View:"))
		]
		+ SHorizontalBox::Slot()
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(0.f, 0.f, 5.f, 0.f)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(this, &SAvaShowControl::GetActiveListName)
		];
}

FText SAvaShowControl::GetActiveListName() const
{
	if (PlaylistEditor.IsValid())
	{
		UAvalanchePlaylist* Playlist = PlaylistEditor->GetPlaylist();

		if (IsValid(Playlist))
		{
			const FAvaPageListReference& ActiveList = Playlist->GetActivePageListReference();

			// Can't happen. Here for completeness.
			if (ActiveList.Type == EAvaPageListType::Template)
			{
				static const FText Templates(LOCTEXT("Templates", "Templates"));
				return Templates;
			}

			if (ActiveList.Type == EAvaPageListType::Instance)
			{
				static const FText AllPages(LOCTEXT("AllPages", "All Pages"));
				return AllPages;
			}

			return FText::Format(LOCTEXT("PlaylistSubListDocument_TabLabel", "Page View {0}"), FText::AsNumber(ActiveList.SubListIndex + 1));
		}
	}

	return FText::GetEmpty();
}

TSharedRef<SWidget> SAvaShowControl::CreateNextPageWidget()
{
	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(5.f, 0.f)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NextUp", "Next Up:"))
		]
		+ SHorizontalBox::Slot()
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(0.f, 0.f, 5.f, 0.f)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(this, &SAvaShowControl::GetNextPageName)
		];
}

FText SAvaShowControl::GetNextPageName() const
{
	static const FText NoPage(LOCTEXT("None", "-"));

	if (PlaylistEditor.IsValid())
	{
		if (const UAvalanchePlaylist* Playlist = PlaylistEditor->GetPlaylist(); IsValid(Playlist))
		{
			if (Playlist->GetInstancedPages().Pages.IsEmpty())
			{
				return NoPage;
			}

			const TSharedPtr<SAvaInstancedPageList> ActiveSubListWidget = PlaylistEditor->GetActiveListWidget();

			if (!ActiveSubListWidget.IsValid() || ActiveSubListWidget->GetPlayingPageIds().IsEmpty())
			{
				return NoPage;
			}

			TArray<int32> NextUps = ActiveSubListWidget->GetPageIdsToTakeNext();

			FText NextUpMessage = FText::GetEmpty();
			FFormatOrderedArguments FormatOrderedArguments;
			
			for (const int32 NextUp : NextUps)
			{
				if (const int32* PageIndex = Playlist->GetInstancedPages().PageIndices.Find(NextUp))
				{
					FormatOrderedArguments.Add(FText::Format(
							LOCTEXT("NextUpFormat", "{0}: {1}"),
							FText::AsNumber(NextUp, &UE::AvalanchePlaylist::FEditorMetrics::PageIdFormattingOptions),
							FText::FromString(Playlist->GetInstancedPages().Pages[*PageIndex].GetPageName())
						));
				}
			}

			if (FormatOrderedArguments.Num() > 0)
			{
				return FText::Join(LOCTEXT("NextUpDelimiter", "\n"), FormatOrderedArguments);
			}
		}
	}

	return NoPage;
}

#undef LOCTEXT_NAMESPACE
