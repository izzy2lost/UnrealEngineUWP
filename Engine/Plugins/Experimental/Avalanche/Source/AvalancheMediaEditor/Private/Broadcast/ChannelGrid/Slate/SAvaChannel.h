// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Broadcast/ChannelGrid/AvaOutputTileItem.h"
#include "Channel/AvaOutputChannel.h"
#include "CoreMinimal.h"
#include "MediaOutput.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STileView.h"

class FAvaBroadcastEditor;
class FUICommandList;
class IStructureDetailsView;
class SAvaChannel;
class SInlineEditableTextBlock;
class SMenuAnchor;
class UMediaOutput;
struct FAvaOutputChannel;
struct FSlateImageBrush;

DECLARE_DELEGATE_RetVal(bool, FAvaCanMaximize);
DECLARE_DELEGATE_OneParam(FAvaOnMaximizeClicked, const TSharedRef<SAvaChannel>&);

class SAvaChannel : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SAvaChannel) {}
		SLATE_ARGUMENT(FName, ChannelName)
		SLATE_ATTRIBUTE(bool, CanMaximize)
		SLATE_EVENT(FAvaOnMaximizeClicked, OnMaximizeClicked)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedPtr<FAvaBroadcastEditor>& InBroadcastEditor);
	virtual ~SAvaChannel() override;

	void RegisterCommands();
	FName GetChannelName() const { return ChannelName; }

	void SetPosition(int32 InColumnIndex, int32 InRowIndex);
	int32 GetRowIndex() const { return RowIndex; }
	int32 GetColumnIndex() const { return ColumnIndex; }

	void OnChannelChanged(const FAvaOutputChannel& InChannel, EAvaChannelChange InChange);
	
	void OnChannelBroadcastStateChanged(const FAvaOutputChannel& InChannel);
	void OnChannelRenderTargetChanged(const FAvaOutputChannel& InChannel);
	void OnChannelMediaOutputsChanged(const FAvaOutputChannel& InChannel);
	
	void OnOutputTileSelectionChanged(const TSharedPtr<FAvaOutputTileItem>& InMediaOutputItem);
	
	void DeleteSelectedOutputTiles();

	FText GetChannelStatusText() const;
	
	TSharedRef<SWidget> GetChannelStatusOptions();
	TSharedRef<SWidget> MakeChannelVerticalBox();
	TSharedRef<SWidget> MakeMediaOutputsTileView();

	TSharedRef<ITableRow> OnGenerateMediaOutputTile(FAvaOutputTileItemPtr Item, const TSharedRef<STableViewBase>& InOwnerTable) const;
	void OnMediaOutputTileSelectionChanged(FAvaOutputTileItemPtr Item,	ESelectInfo::Type SelectInfo);
	TSharedPtr<SWidget> OnMediaOutputTileContextMenuOpening() const;
	
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;;
	
	void SetDragging(bool bIsDragging);

	FText GetChannelNameText() const;
	bool OnVerifyChannelNameTextChanged(const FText& InText, FText& OutErrorMessage);
	void OnChannelNameTextCommitted(const FText& InText, ETextCommit::Type CommitInfo);

	FReply OnChannelStatusButtonClicked();
	FReply OnChannelSettingsButtonClicked();
	FReply OnChannelPinButtonClicked();
	FReply OnChannelTypeToggleButtonClicked();
	FReply OnChannelMaximizeButtonClicked();
	FReply OnChannelRemoveButtonClicked();

	void OnChannelStatusSelected(EAvaChannelState State);

	const FSlateBrush* GetChannelStatusBrush() const;
	const FSlateBrush* GetChannelPreviewBrush() const;
	const FSlateBrush* GetChannelPinBrush() const;
	const FSlateBrush* GetChannelMaximizeRestoreBrush() const;
	const FSlateBrush* GetChannelTypeBrush() const;

	EVisibility GetMediaOutputEmptyTextVisibility() const;
	EVisibility GetChannelPreviewVisibility() const;
	EVisibility GetChannelDragVisibility() const;

	bool ShouldInvertAlpha() const;

	bool IsReadOnly() const;
	bool CanEditChanges() const;
	
protected:
	FText GetChannelMaximizeRestoreTooltipText() const;
	
	FName ChannelName = NAME_None;

	TSharedPtr<SMenuAnchor> ChannelStatusOptions;
	
	TSharedPtr<SListView<FAvaOutputTileItemPtr>> OutputTileListView;
	
	TSharedPtr<IStructureDetailsView> ChannelSettings;

	TSharedPtr<SMenuAnchor> ChannelSettingsMenuAnchor;

	TArray<FAvaOutputTileItemPtr> OutputTileItems;
	
	TSharedPtr<FSlateImageBrush> ChannelPreviewBrush;

	const FSlateBrush* ChannelStatusBrush = nullptr;
	
	TSharedPtr<SInlineEditableTextBlock> ChannelNameTextBlock;
	
	TWeakPtr<FAvaBroadcastEditor> BroadcastEditorWeak;

	TSharedPtr<FUICommandList> MediaOutputCommandList;
	
	EVisibility DragVisibility = EVisibility::Hidden;

	EAvaChannelState ChannelState = EAvaChannelState::Idle;
	
	FText ChannelStateText;

	TAttribute<bool> CanMaximize;
	FAvaOnMaximizeClicked OnMaximizeClicked;

	int32 RowIndex = INDEX_NONE;
	
	int32 ColumnIndex= INDEX_NONE;
	
	bool bShouldInvertAlpha = false;
};
