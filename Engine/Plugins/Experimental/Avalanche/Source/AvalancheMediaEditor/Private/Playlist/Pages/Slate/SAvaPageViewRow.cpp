// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playlist/Pages/Slate/SAvaPageViewRow.h"
#include "AvaBlueprint.h"
#include "AvaMediaEditorStyle.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Playlist/Pages/Columns/AvaPageViewColumn.h"
#include "SAvaPageList.h"
#include "ScopedTransaction.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "SAvaPageViewRow"

namespace UE::AvalancheMediaEditor::RowUtils
{
	FName ActivePageBrushName = FName(TEXT("TableView.ActivePageBrush"));
	FName ActivePageHoveredBrushName = FName(TEXT("TableView.Hovered.ActivePageBrush"));
	FName ActivePageSelectedBrushName = FName(TEXT("TableView.Selected.ActivePageBrush"));
	FName DisabledPageBrushName = FName(TEXT("TableView.DisabledPageBrush"));
	FName DisabledPageHoveredBrushName = FName(TEXT("TableView.Hovered.DisabledPageBrush"));
	FName DisabledPageSelectedBrushName = FName(TEXT("TableView.Selected.DisabledPageBrush"));
}

void SAvaPageViewRow::Construct(const FArguments& InArgs, FAvaPageViewPtr InPageView, const TSharedRef<SAvaPageList>& InPageList)
{
	PageViewWeak = InPageView;
	PageListWeak = InPageList;

	TSharedPtr<SListView<FAvaPageViewPtr>> PageListView = InPageList->GetPageListView();
	check(PageListView.IsValid());
	
	SMultiColumnTableRow::Construct(
		FSuperRowType::FArguments()
		.OnDragDetected(this, &SAvaPageViewRow::OnPageViewDragDetected)
		.OnCanAcceptDrop(this, &SAvaPageViewRow::OnPageViewCanAcceptDrop)
		.OnAcceptDrop(this, &SAvaPageViewRow::OnPageViewAcceptDrop)
		, PageListView.ToSharedRef()
	);
	SetBorderImage(TAttribute<const FSlateBrush*>(this, &SAvaPageViewRow::GetBorder));
}

TSharedRef<SWidget> SAvaPageViewRow::GenerateWidgetForColumn(const FName& InColumnName)
{
	if (const FAvaPageViewPtr PageView = PageViewWeak.Pin())
	{
		if (const TSharedPtr<IAvaPageViewColumn> Column = PageListWeak.Pin()->FindColumn(InColumnName))
		{
			const TSharedRef<SWidget> RowWidget = Column->ConstructRowWidget(PageView.ToSharedRef(), SharedThis(this));
			RowWidget->SetOnMouseDoubleClick(FPointerEventHandler::CreateLambda([this](const FGeometry&, const FPointerEvent&)
			{
				return PageViewWeak.IsValid()? PageViewWeak.Pin()->OnPreviewButtonClicked() : FReply::Handled();
			}));
			return RowWidget;
		}
	}
	return SNullWidget::NullWidget;
}

const FSlateBrush* SAvaPageViewRow::GetBorder() const
{
	if (const TSharedPtr<IAvaPageView> PageView = PageViewWeak.Pin())
	{
		const UAvalanchePlaylist* const Playlist = PageView->GetPlaylist();
		if (Playlist && !PageView->IsTemplate())
		{
			const FAvalanchePage& Page = Playlist->GetPage(PageView->GetPageId());
			const TArray<FAvalanchePageStatus> Statuses = Page.GetPageContextualStatuses(Playlist);

			if (FAvalanchePage::StatusesContainsStatus(Statuses, { EAvalanchePageStatus::Loaded, EAvalanchePageStatus::Playing }))
			{
				return GetRowColorBrush(EAvalancheRowState::Active);
			}

			if (FAvalanchePage::StatusesContainsStatus(Statuses, { EAvalanchePageStatus::Error, EAvalanchePageStatus::Missing }))
			{
				return GetRowColorBrush(EAvalancheRowState::Disabled);
			}
		}
	}

	return SMultiColumnTableRow::GetBorder();
}

FReply SAvaPageViewRow::OnPageViewDragDetected(const FGeometry& InMyGeometry, const FPointerEvent& InPointerEvent)
{
	TSharedPtr<SAvaPageList> PageList = PageListWeak.Pin();

	if (!PageList.IsValid() || PageList->GetSelectedPageIds().IsEmpty())
	{
		return FReply::Unhandled();
	}

	return FReply::Handled().BeginDragDrop(FAvaPageViewRowDragDropOp::New(PageList));
}

TOptional<EItemDropZone> SAvaPageViewRow::OnPageViewCanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, FAvaPageViewPtr InItem)
{
	if (TSharedPtr<SAvaPageList> PageList = PageListWeak.Pin())
	{
		if (PageList->CanHandleDragObjects(InDragDropEvent))
		{
			return InDropZone;
		}
	}

	return TOptional<EItemDropZone>();
}

FReply SAvaPageViewRow::OnPageViewAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, FAvaPageViewPtr InItem)
{
	if (const TSharedPtr<SAvaPageList> PageList = PageListWeak.Pin())
	{
		if (PageList->HandleDropEvent(InDragDropEvent, InDropZone, InItem))
		{
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

const FSlateBrush* SAvaPageViewRow::GetRowColorBrush(EAvalancheRowState InRowState) const
{
	using namespace UE::AvalancheMediaEditor::RowUtils;
	FName NormalStateBrushName;
	FName HoveredStateBrushName;
	FName SelectedStateBrushName;

	if (InRowState == EAvalancheRowState::Active)
	{
		NormalStateBrushName = ActivePageBrushName;
		HoveredStateBrushName = ActivePageHoveredBrushName;
		SelectedStateBrushName = ActivePageSelectedBrushName;
	}
	else
	{
		NormalStateBrushName = DisabledPageBrushName;
		HoveredStateBrushName = DisabledPageHoveredBrushName;
		SelectedStateBrushName = DisabledPageSelectedBrushName;
	}

	if (IsHovered())
	{
		return FAvaMediaEditorStyle::Get().GetBrush(HoveredStateBrushName);
	}

	if (IsSelected())
	{
		return FAvaMediaEditorStyle::Get().GetBrush(SelectedStateBrushName);
	}

	return FAvaMediaEditorStyle::Get().GetBrush(NormalStateBrushName);
}

TSharedPtr<SWidget> FAvaPageViewRowDragDropOp::GetDefaultDecorator() const
{
	TArray<FText> IdTexts;

	for (int32 DraggedId : DraggedIds)
	{
		IdTexts.Add(FText::AsNumber(DraggedId, &UE::AvalanchePlaylist::FEditorMetrics::PageIdFormattingOptions));
	}

	return SNew(SBorder)
		.Padding(5.f)
		.BorderImage(FAppStyle::GetBrush("Graph.ConnectorFeedback.Border"))
		.Content()
		[
			SNew(STextBlock)
			.Text(FText::Join(LOCTEXT("Comma", ", "), IdTexts))
		];
}

TSharedRef<FAvaPageViewRowDragDropOp> FAvaPageViewRowDragDropOp::New(const TSharedPtr<SAvaPageList>& InPageList)
{
	check(InPageList.IsValid());

	TSharedRef<FAvaPageViewRowDragDropOp> Operation = MakeShared<FAvaPageViewRowDragDropOp>();
	Operation->PageListWeak = InPageList;
	Operation->DraggedIds = InPageList->GetSelectedPageIds(); // Cache this just in case.
	Operation->Construct();
	return Operation;
}

#undef LOCTEXT_NAMESPACE
