// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Playlist/AvaPlaylistDefines.h"
#include "Playlist/AvaPlaylistDefines.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class SAvaPageList;

enum class EAvalancheRowState
{
	Active,
	Disabled
};

class FAvaPageViewRowDragDropOp : public FDragDropOperation, public TSharedFromThis<FAvaPageViewRowDragDropOp>
{
public:
	DRAG_DROP_OPERATOR_TYPE(FAvaPageViewRowDragDropOp, FDragDropOperation)

	FName Color;

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;

	static TSharedRef<FAvaPageViewRowDragDropOp> New(const TSharedPtr<SAvaPageList>& InPageList);

	TSharedPtr<SAvaPageList> GetPageList() const { return PageListWeak.Pin(); }
	const TArray<int32>& GetDraggedIds() const { return DraggedIds; }

protected:
	TWeakPtr<SAvaPageList> PageListWeak;
	TArray<int32> DraggedIds;
};

class AVALANCHEMEDIAEDITOR_API SAvaPageViewRow : public SMultiColumnTableRow<FAvaPageViewPtr>
{
public:	
	SLATE_BEGIN_ARGS(SAvaPageViewRow){}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, FAvaPageViewPtr InPageView, const TSharedRef<SAvaPageList>& InPageList);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override;

	virtual const FSlateBrush* GetBorder() const override;

protected:
	FReply OnPageViewDragDetected(const FGeometry& InMyGeometry, const FPointerEvent& InPointerEvent);
	TOptional<EItemDropZone> OnPageViewCanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, FAvaPageViewPtr InItem);
	FReply OnPageViewAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, FAvaPageViewPtr InItem);

	TWeakPtr<IAvaPageView> PageViewWeak;
	TWeakPtr<SAvaPageList> PageListWeak;

private:
	const FSlateBrush* GetRowColorBrush(EAvalancheRowState InRowState) const;
};
