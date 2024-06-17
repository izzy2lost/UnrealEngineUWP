// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCSignatureTree.h"
#include "Columns/IRCSignatureColumn.h"
#include "Items/RCSignatureTreeItemBase.h"
#include "Items/RCSignatureTreeRootItem.h"
#include "Items/RCSignatureTreeSignatureItem.h"
#include "RemoteControlPreset.h"
#include "RemoteControlSignatureRegistry.h"
#include "SRCSignaturePanel.h"
#include "SRCSignatureRow.h"
#include "ScopedTransaction.h"
#include "Styling/RemoteControlStyles.h"
#include "UI/RemoteControlPanelStyle.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "SRCSignatureTreeView"

void SRCSignatureTree::Construct(const FArguments& InArgs, const TSharedRef<SRCSignaturePanel>& InSignaturePanel, const TSharedRef<SRemoteControlPanel>& InRCPanel)
{
	SRCLogicPanelListBase::Construct(SRCLogicPanelListBase::FArguments(), InSignaturePanel, InRCPanel);
	SignaturePanelWeak = InSignaturePanel;

	RootItem = MakeShared<FRCSignatureTreeRootItem>(SharedThis(this));

	const FRCPanelStyle* RCPanelStyle = &FRemoteControlPanelStyle::Get()->GetWidgetStyle<FRCPanelStyle>("RemoteControlPanel.LogicControllersPanel");

	HeaderRow = SNew(SHeaderRow)
		.Style(&RCPanelStyle->HeaderRowStyle)
		.Visibility(EVisibility::Visible)
		.CanSelectGeneratedColumn(true);

	ConstructColumns(InArgs._Columns);

	ChildSlot
	[
		SAssignNew(SignatureTreeView, STreeView<TSharedPtr<FRCSignatureTreeItemBase>>)
		.TreeItemsSource(&RootItem->GetChildrenMutable())
		.HeaderRow(HeaderRow)
		.OnGetChildren(this, &SRCSignatureTree::OnGetChildren)
		.OnGenerateRow(this, &SRCSignatureTree::OnGenerateRow)
		.OnExpansionChanged(this, &SRCSignatureTree::OnItemExpansionChanged)
		.OnSelectionChanged(this, &SRCSignatureTree::OnItemSelectionChanged)
		.OnContextMenuOpening(this, &SRCLogicPanelListBase::GetContextMenuWidget)
		.SelectionMode(ESelectionMode::Multi)
		.HighlightParentNodesForSelection(true)
	];

	Refresh();
}

URemoteControlSignatureRegistry* SRCSignatureTree::GetSignatureRegistry() const
{
	TSharedPtr<SRCSignaturePanel> SignaturePanel = SignaturePanelWeak.Pin();
	if (!SignaturePanel.IsValid())
	{
		return nullptr;
	}

	if (URemoteControlPreset* Preset = SignaturePanel->GetPreset())
	{
		return Preset->GetSignatureRegistry();
	}
	return nullptr;
}

TSharedPtr<IRCSignatureColumn> SRCSignatureTree::FindColumn(FName InColumnName) const
{
	if (const TSharedRef<IRCSignatureColumn>* Column = Columns.Find(InColumnName))
	{
		return *Column;
	}
	return nullptr;
}

void SRCSignatureTree::Refresh()
{
	if (bRefreshing)
	{
		return;
	}

	TGuardValue<bool> RefreshGuard(bRefreshing, true);

	RootItem->RebuildChildren();
	RootItem->VisitChildren([&TreeView = SignatureTreeView](const TSharedPtr<FRCSignatureTreeItemBase>& InItem)->bool
		{
			TreeView->SetItemExpansion(InItem, InItem->HasAnyFlags(ERCSignatureTreeItemFlags::Expanded));
			TreeView->SetItemSelection(InItem, InItem->HasAnyFlags(ERCSignatureTreeItemFlags::Selected));
			return true;
		}
		, /*bRecursive*/true);

	SignatureTreeView->RequestTreeRefresh();
}

TArray<TSharedPtr<FRCSignatureTreeItemBase>> SRCSignatureTree::GetSelectedItems() const
{
	return SignatureTreeView->GetSelectedItems();
}

URemoteControlPreset* SRCSignatureTree::GetPreset()
{
	if (TSharedPtr<SRCSignaturePanel> SignaturePanel = SignaturePanelWeak.Pin())
	{
		return SignaturePanel->GetPreset();
	}
	return nullptr;
}

TArray<TSharedPtr<FRCLogicModeBase>> SRCSignatureTree::GetSelectedLogicItems()
{
	return TArray<TSharedPtr<FRCLogicModeBase>>(GetSelectedItems());
}

bool SRCSignatureTree::IsEmpty() const
{
	return RootItem->GetChildren().IsEmpty();
}

bool SRCSignatureTree::IsListFocused() const
{
	return SignatureTreeView->HasAnyUserFocus().IsSet() || ContextMenuWidgetCached.IsValid();
}

int32 SRCSignatureTree::Num() const
{
	return RootItem->GetChildren().Num();
}

int32 SRCSignatureTree::NumSelectedLogicItems() const
{
	return SignatureTreeView->GetNumItemsSelected();
}

int32 SRCSignatureTree::RemoveModel(const TSharedPtr<FRCLogicModeBase> InItem)
{
	if (InItem.IsValid())
	{
		return StaticCastSharedPtr<FRCSignatureTreeItemBase>(InItem)->RemoveFromRegistry();
	}
	return 0;
}

void SRCSignatureTree::DeleteSelectedPanelItems()
{
	const TArray<TSharedPtr<FRCSignatureTreeItemBase>> SelectedSignatures = SignatureTreeView->GetSelectedItems();
	if (SelectedSignatures.IsEmpty())
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("RemoveSelectedSignatures", "Remove Selected Signatures"));
	DeleteItemsFromLogicPanel<FRCSignatureTreeItemBase>(RootItem->GetChildrenMutable(), SelectedSignatures);
}

void SRCSignatureTree::Reset()
{
	Refresh();
}

void SRCSignatureTree::ConstructColumns(TConstArrayView<TSharedRef<IRCSignatureColumn>> InColumns)
{
	HeaderRow->ClearColumns();
	Columns.Empty(InColumns.Num());

	for (const TSharedRef<IRCSignatureColumn>& Column : InColumns)
	{
		Columns.Add(Column->GetColumnId(), Column);
		HeaderRow->AddColumn(Column->ConstructHeaderRowColumn());
		HeaderRow->SetShowGeneratedColumn(Column->GetColumnId(), Column->ShouldShowColumnByDefault());
	}
}

TSharedRef<ITableRow> SRCSignatureTree::OnGenerateRow(TSharedPtr<FRCSignatureTreeItemBase> InItem, const TSharedRef<STableViewBase>& InTableView)
{
	check(InItem.IsValid());
	return SNew(SRCSignatureRow, InItem, SharedThis(this), InTableView);
}

void SRCSignatureTree::OnGetChildren(TSharedPtr<FRCSignatureTreeItemBase> InItem, TArray<TSharedPtr<FRCSignatureTreeItemBase>>& OutChildren) const
{
	if (InItem.IsValid())
	{
		OutChildren.Append(InItem->GetChildren());
	}
}

void SRCSignatureTree::OnItemExpansionChanged(TSharedPtr<FRCSignatureTreeItemBase> InItem, bool bInIsExpanded)
{
	if (bRefreshing)
	{
		return;
	}

	if (InItem.IsValid())
	{
		if (bInIsExpanded)
		{
			InItem->AddFlags(ERCSignatureTreeItemFlags::Expanded);
		}
		else
		{
			InItem->RemoveFlags(ERCSignatureTreeItemFlags::Expanded);
		}
	}
}

void SRCSignatureTree::OnItemSelectionChanged(TSharedPtr<FRCSignatureTreeItemBase> InItem, ESelectInfo::Type InSelectionType)
{
	if (bRefreshing)
	{
		return;
	}

	RootItem->VisitChildren([](const TSharedPtr<FRCSignatureTreeItemBase>& InItem)->bool
		{
			InItem->RemoveFlags(ERCSignatureTreeItemFlags::Selected);
			return true;
		}
		, /*bRecursive*/true);

	for (const TSharedPtr<FRCSignatureTreeItemBase>& SelectedItem : SignatureTreeView->GetSelectedItems())
	{
		SelectedItem->AddFlags(ERCSignatureTreeItemFlags::Selected);
	}
}

#undef LOCTEXT_NAMESPACE
