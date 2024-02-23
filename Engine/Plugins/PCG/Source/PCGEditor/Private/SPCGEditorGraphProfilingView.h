// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/PCGExtraCapture.h"

#include "Widgets/Views/ITableRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class FPCGEditor;
class STableViewBase;
class UPCGComponent;
class UPCGEditorGraphNode;
class UPCGEditorGraph;
class UPCGNode;
struct FPCGStack;

struct FPCGProfilingListViewItem
{
	const UPCGNode* PCGNode = nullptr;
	const UPCGEditorGraphNode* EditorNode = nullptr;

	FString Name;

	PCGUtils::FCallTime CallTime;

	bool bHasData = false;
};

typedef TSharedPtr<FPCGProfilingListViewItem> PCGProfilingListViewItemPtr;

class SPCGProfilingListViewItemRow : public SMultiColumnTableRow<PCGProfilingListViewItemPtr>
{
public:
	SLATE_BEGIN_ARGS(SPCGProfilingListViewItemRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const PCGProfilingListViewItemPtr& Item);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnId) override;

protected:
	PCGProfilingListViewItemPtr InternalItem;
};

class SPCGEditorGraphProfilingView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphProfilingView) { }
	SLATE_END_ARGS()

	~SPCGEditorGraphProfilingView();

	void Construct(const FArguments& InArgs, TSharedPtr<FPCGEditor> InPCGEditor);

private:
	TSharedRef<SHeaderRow> CreateHeaderRowWidget();

	ECheckBoxState IsSubgraphExpanded() const;
	void OnSubgraphExpandedChanged(ECheckBoxState InNewState);

	void OnDebugStackChanged(const FPCGStack& InPCGStack);

	void OnGenerateUpdated(UPCGComponent* InPCGComponent);
	
	// Callbacks
	FReply Refresh();
	FReply ResetTimers();
	TSharedRef<ITableRow> OnGenerateRow(PCGProfilingListViewItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	void OnItemDoubleClicked(PCGProfilingListViewItemPtr Item);
	void OnSortColumnHeader(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type NewSortMode);
	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;
	FText GetTotalTimeLabel() const;
	FText GetTotalWallTimeLabel() const;

	/** Called when user changes the text they are searching for */
	void OnSearchTextChanged(const FText& InText);

	/** Called when user changes commits text to the search box */
	void OnSearchTextCommitted(const FText& InText, ETextCommit::Type InCommitType);

	/** Pointer back to the PCG editor that owns us */
	TWeakPtr<FPCGEditor> PCGEditorPtr;

	/** Cached PCGGraph being viewed */
	UPCGEditorGraph* PCGEditorGraph = nullptr;

	/** Cached PCGComponent being viewed */
	TWeakObjectPtr<UPCGComponent> PCGComponent;

	TSharedPtr<SHeaderRow> ListViewHeader;
	TSharedPtr<SListView<PCGProfilingListViewItemPtr>> ListView;
	TArray<PCGProfilingListViewItemPtr> ListViewItems;

	// To allow sorting
	FName SortingColumn = NAME_None;
	EColumnSortMode::Type SortMode = EColumnSortMode::Type::None;

	double TotalTime = 0.0;
	double TotalWallTime = 0.0;

	bool bExpandSubgraph = true;

	/** The string to search for */
	FString SearchValue;
};
