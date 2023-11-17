// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Widgets/SCompoundWidget.h"

namespace ESelectInfo { enum Type : int; }

class ITableRow;
template <typename ItemType> class STreeView;
class STableViewBase;
class UMVVMWidgetBlueprintExtension_View;
class UMVVMBlueprintViewEvent;

namespace UE::MVVM
{
class SBindingsPanel;

struct FBindingEntry;

class SBindingsList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBindingsList) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<SBindingsPanel> Owner, UMVVMWidgetBlueprintExtension_View* MVVMExtension);
	~SBindingsList();

	void Refresh();

	void OnFilterTextChanged(const FText& InFilterText);
	void ClearFilterText();

	/** Constructs context menu used for right click and dropdown button */
	TSharedPtr<SWidget> OnSourceConstructContextMenu();

	void RequestNavigateToBinding(FGuid BindingId);
	void RequestNavigateToEvent(UMVVMBlueprintViewEvent* Event);

private:
	TSharedRef<ITableRow> GenerateEntryRow(TSharedPtr<FBindingEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable) const;
	void OnSourceListSelectionChanged(TSharedPtr<FBindingEntry> Entry, ESelectInfo::Type SelectionType) const;
	void GetChildrenOfEntry(TSharedPtr<FBindingEntry> Entry, TArray<TSharedPtr<FBindingEntry>>& OutChildren) const;

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	void HandleDeleteSelected();
	void HandleResetSelectedPin();
	void HandleBreakSelectedPin();
	void HandleRecombineSelectedPin();
	void HandleResetOrphanedSelectedPin();

	void ForceRefresh();

private:
	TWeakPtr<SBindingsPanel> BindingPanel;
	TSharedPtr<STreeView<TSharedPtr<FBindingEntry>>> TreeView;
	TArray<TSharedPtr<FBindingEntry>> AllRootGroups;
	TArray<TSharedPtr<FBindingEntry>> FilteredRootGroups;
	TWeakObjectPtr<UMVVMWidgetBlueprintExtension_View> MVVMExtension;
	mutable bool bSelectionChangedGuard = false;
	FText FilterText;
};

} // namespace UE::MVVM
