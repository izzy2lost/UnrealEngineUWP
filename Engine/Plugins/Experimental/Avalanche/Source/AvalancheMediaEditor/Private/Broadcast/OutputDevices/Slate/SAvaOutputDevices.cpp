// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaOutputDevices.h"
#include "AvalancheBroadcast.h"
#include "AvalancheMediaEditorSettings.h"
#include "Broadcast/OutputDevices/AvaOutputRootItem.h"
#include "Broadcast/OutputDevices/AvaOutputServerItem.h"
#include "Widgets/Views/STreeView.h"

TMap<uint32, bool> SAvaOutputDevices::ItemExpansionStates = {};

SAvaOutputDevices::~SAvaOutputDevices()
{
	if (UObjectInitialized())
	{
		UAvalancheMediaEditorSettings::GetMutable().OnSettingChanged().RemoveAll(this);
	}
}

void SAvaOutputDevices::Construct(const FArguments& InArgs, const TSharedPtr<FAvaBroadcastEditor>& InBroadcastEditor)
{
	RootItem = MakeShared<FAvaOutputRootItem>();
	
	SAssignNew(OutputTree, STreeView<FAvaOutputTreeItemPtr>)
		.ItemHeight(20.0f)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SAvaOutputDevices::OnGenerateItemRow)
		.OnGetChildren(this, &SAvaOutputDevices::OnGetRowChildren)
		.OnExpansionChanged(this, &SAvaOutputDevices::OnRowExpansionChanged)
		.TreeItemsSource(&TopLevelItems);
	
	ChildSlot
	[
		OutputTree.ToSharedRef()
	];

	RefreshOutputDevices();
	
	BroadcastChangedHandle = UAvalancheBroadcast::Get().AddChangeListener(
		FOnAvaBroadcastChanged::FDelegate::CreateSP(this, &SAvaOutputDevices::OnBroadcastChanged));
	UAvalancheMediaEditorSettings::GetMutable().OnSettingChanged().AddSP(this, &SAvaOutputDevices::OnAvaMediaSettingsChanged);
}

TSharedRef<ITableRow> SAvaOutputDevices::OnGenerateItemRow(FAvaOutputTreeItemPtr InItem
	, const TSharedRef<STableViewBase>& InOwnerTable)
{
	check(InItem.IsValid());
	return SNew(STableRow<FAvaOutputTreeItemPtr>, InOwnerTable)
		.ShowWires(false)
		.Padding(FMargin(5.f, 5.f))
		.OnDragDetected(InItem.ToSharedRef(), &IAvaOutputTreeItem::OnDragDetected)
		[
			InItem->GenerateRowWidget().ToSharedRef()
		];
}

void SAvaOutputDevices::OnGetRowChildren(FAvaOutputTreeItemPtr InItem, TArray<FAvaOutputTreeItemPtr>& OutChildren) const
{
	if (InItem.IsValid())
	{
		OutChildren.Append(InItem->GetChildren());
	}
}

void SAvaOutputDevices::OnRowExpansionChanged(FAvaOutputTreeItemPtr InItem, const bool bInIsExpanded)
{
	const uint32 Hash = ItemHashRecursive(InItem);

	if (ItemExpansionStates.Contains(Hash))
	{
		ItemExpansionStates[Hash] = bInIsExpanded;
	}
	else
	{
		ItemExpansionStates.Add(Hash, bInIsExpanded);
	}
}

uint32 SAvaOutputDevices::ItemHashRecursive(const FAvaOutputTreeItemPtr& InItem) const
{
	if (!InItem.IsValid() || InItem->IsA<FAvaOutputRootItem>())
	{
		return 0;
	}

	const uint32 Hash = GetTypeHash(InItem->GetDisplayName().ToString());

	if (const TSharedPtr<FAvaOutputTreeItem>& Parent = InItem->GetParent().Pin())
	{
		const uint32 ParentHash = ItemHashRecursive(Parent);
		if (ParentHash != 0)
		{
			const uint32 CombinedHash = HashCombine(ParentHash, Hash);
			return CombinedHash;
		}
	}
		
	return Hash;
}

void SAvaOutputDevices::RefreshOutputDevices()
{
	check(RootItem.IsValid());
	
	//Refresh Items
	FAvaOutputTreeItem::RefreshTree(RootItem);
	TopLevelItems = RootItem->GetChildren();
	OutputTree->RequestTreeRefresh();

	SetExpansionStatesRecursive(RootItem);
}

void SAvaOutputDevices::OnBroadcastChanged(EAvaBroadcastChange InChange)
{
	if (EnumHasAnyFlags(InChange, EAvaBroadcastChange::OutputDevices))
	{
		RefreshOutputDevices();
	}
}

void SAvaOutputDevices::OnAvaMediaSettingsChanged(UObject*, FPropertyChangedEvent& InPropertyChangeEvent)
{
	static const FName BroadcastShowAllMediaOutputClassesName = GET_MEMBER_NAME_CHECKED(UAvalancheMediaEditorSettings, bBroadcastShowAllMediaOutputClasses);
	if (InPropertyChangeEvent.GetPropertyName() == BroadcastShowAllMediaOutputClassesName)
	{
		RefreshOutputDevices();
	}
}

void SAvaOutputDevices::SetExpansionStatesRecursive(const FAvaOutputTreeItemPtr& InRootItem)
{
	if (!InRootItem.IsValid())
	{
		return;
	}

	for (const FAvaOutputTreeItemPtr& Child : InRootItem->GetChildren())
	{
		if (Child.IsValid())
		{
			const uint32 ChildHash = ItemHashRecursive(Child);
			if (ItemExpansionStates.Contains(ChildHash))
			{
				OutputTree->SetItemExpansion(Child, ItemExpansionStates[ChildHash]);
			}

			SetExpansionStatesRecursive(Child);
		}
	}
}
