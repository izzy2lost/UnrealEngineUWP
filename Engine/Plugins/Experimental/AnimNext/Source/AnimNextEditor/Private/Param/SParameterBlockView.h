// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "AssetRegistry/AssetData.h"
#include "Templates/SharedPointer.h"

class UAnimNextParameterBlockBinding;
class UAnimNextParameter;
class UAnimNextParameterBlock_EditorData;
class UAnimNextParameterBlockEntry;
class URigVMGraph;
class FUICommandList;

namespace UE::AnimNext::Editor
{

enum class EFilterParameterResult : int32;
struct FParameterBindingReference;

struct FParameterBlockViewEntry; 

enum class EParameterBlockCategoryType : uint8
{
	Parameter,
	Graph,
	// --- ---
	Invalid
};

class SParameterBlockView : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnSelectionChanged, const TArray<UObject*>& /*InEntries*/);

	DECLARE_DELEGATE_OneParam(FOnOpenGraph, URigVMGraph* /*InGraph*/);

	DECLARE_DELEGATE_OneParam(FOnDeleteEntries, const TArray<UAnimNextParameterBlockEntry*>& /*InEntries*/);
	
	SLATE_BEGIN_ARGS(SParameterBlockView) {}

	SLATE_EVENT(SParameterBlockView::FOnSelectionChanged, OnSelectionChanged)
	
	SLATE_EVENT(SParameterBlockView::FOnOpenGraph, OnOpenGraph)

	SLATE_EVENT(SParameterBlockView::FOnDeleteEntries, OnDeleteEntries)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UAnimNextParameterBlock_EditorData* InEditorData);

private:
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	void RequestRefresh();

	void RefreshEntries();

	void RefreshFilter();

	// Bind input commands
	void BindCommands();

	// Handle modifications to the block
	void HandleBlockModified(UAnimNextParameterBlock_EditorData* InEditorData);

	// Get the content for the context menu
	TSharedRef<SWidget> HandleGetContextContent();

	void HandleDelete();

	void HandleRename();

	bool HasValidSelection() const;

	bool HasValidSingleSelection() const;

	// Generate a row for the list view
	TSharedRef<ITableRow> HandleGenerateRow(TSharedRef<FParameterBlockViewEntry> InEntry, const TSharedRef<STableViewBase>& InOwnerTable);

	void HandleGetChildren(TSharedRef<FParameterBlockViewEntry> InEntry, TArray<TSharedRef<FParameterBlockViewEntry>>& OutChildren);

	// Handle rename after scrolling into view
	void HandleItemScrolledIntoView(TSharedRef<FParameterBlockViewEntry> Entry, const TSharedPtr<ITableRow>& Widget);

	// Handle selection
	void HandleSelectionChanged(TSharedPtr<FParameterBlockViewEntry> InEntry, ESelectInfo::Type InSelectionType);

	EFilterParameterResult HandleFilterLinkedParameter(const FParameterBindingReference& InParameterBinding);

	TSharedRef<FParameterBlockViewEntry> GetCategory(EParameterBlockCategoryType CategoryType);
	
private:
	friend class SParameterBlockViewRow;
	
	TArray<TSharedRef<FParameterBlockViewEntry>> Categories;

	TSharedPtr<STreeView<TSharedRef<FParameterBlockViewEntry>>> EntriesList;

	TArray<TSharedRef<FParameterBlockViewEntry>> Entries;

	FText FilterText;

	TArray<TSharedRef<FParameterBlockViewEntry>> FilteredEntries;

	UAnimNextParameterBlock_EditorData* EditorData = nullptr;

	TSharedPtr<FUICommandList> UICommandList = nullptr;

	FOnSelectionChanged OnSelectionChangedDelegate;

	FOnOpenGraph OnOpenGraphDelegate;

	FOnDeleteEntries OnDeleteEntriesDelegate;

	FAssetData BlockAssetData;

	TArray<UAnimNextParameterBlockEntry*> PendingSelection;

	bool bRefreshRequested = false;
};

}