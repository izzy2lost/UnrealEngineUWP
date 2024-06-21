// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCSignatureTreeActionItem.h"
#include "RCSignatureAction.h"
#include "RCSignatureTreeFieldItem.h"
#include "RCSignatureTreeRootItem.h"
#include "RemoteControlSignature.h"
#include "RemoteControlSignatureRegistry.h"
#include "ScopedTransaction.h"
#include "UI/Signature/RCSignatureTreeItemSelection.h"

#define LOCTEXT_NAMESPACE "RCSignatureTreeActionItem"

FRCSignatureTreeActionItem::FRCSignatureTreeActionItem(int32 InActionIndex, const TSharedPtr<SRCSignatureTree>& InSignatureTree)
	: FRCSignatureTreeItemBase(InSignatureTree)
	, ActionIndex(InActionIndex)
{
	// Action Items are hidden in Tree View and instead shown in a Horizontal List next to its Parent
	AddTreeViewFlags(ERCSignatureTreeItemViewFlags::Hidden);
}

const FRCSignatureActionDefinition* FRCSignatureTreeActionItem::FindActionDefinition() const
{
	TSharedPtr<FRCSignatureTreeFieldItem> FieldItem = GetParentFieldItem();
	if (!FieldItem.IsValid())
	{
		return nullptr;
	}

	const FRCSignatureField* Field = FieldItem->FindField();
	if (!Field || !Field->ActionDefinitions.IsValidIndex(ActionIndex))
	{
		return nullptr;
	}

	return &Field->ActionDefinitions[ActionIndex];
}

FRCSignatureActionDefinition* FRCSignatureTreeActionItem::FindActionDefinitionMutable()
{
	FRCSignatureField* Field = FindParentFieldMutable();
	if (!Field || !Field->ActionDefinitions.IsValidIndex(ActionIndex))
	{
		return nullptr;
	}

	return &Field->ActionDefinitions[ActionIndex];
}

FRCSignatureActionIcon FRCSignatureTreeActionItem::GetIcon() const
{
	if (const FRCSignatureActionDefinition* ActionDefinition = FindActionDefinition())
	{
		if (const FRCSignatureAction* Action = ActionDefinition->GetAction())
		{
			return Action->GetIcon();
		}
	}
	return FRCSignatureActionIcon();
}

void FRCSignatureTreeActionItem::BuildPathSegment(FStringBuilderBase& InStringBuilder) const
{
	InStringBuilder << ActionIndex;
}

TOptional<bool> FRCSignatureTreeActionItem::IsEnabled() const
{
	return true;
}

int32 FRCSignatureTreeActionItem::RemoveFromRegistry()
{
	URemoteControlSignatureRegistry* Registry;

	FRCSignatureField* Field = FindParentFieldMutable(&Registry);
	if (!Field || !Field->ActionDefinitions.IsValidIndex(ActionIndex))
	{
		return 0;
	}

	check(Registry);

	FScopedTransaction Transaction(LOCTEXT("RemoveAction", "Remove Action"));
	Registry->Modify();
	Field->ActionDefinitions.RemoveAt(ActionIndex);
	return 1;
}

FText FRCSignatureTreeActionItem::GetDisplayNameText() const
{
	// Unused, as Action Items are hidden in Tree View
	return FText::GetEmpty();
}

FText FRCSignatureTreeActionItem::GetDescription() const
{
	// Unused, as Action Items are hidden in Tree View
	return FText::GetEmpty();
}

TSharedPtr<FStructOnScope> FRCSignatureTreeActionItem::MakeSelectionStruct()
{
	if (FRCSignatureActionDefinition* ActionDefinition = FindActionDefinitionMutable())
	{
		return ActionDefinition->MakeStructOnScope();
	}
	return nullptr;
}

TSharedPtr<FRCSignatureTreeFieldItem> FRCSignatureTreeActionItem::GetParentFieldItem() const
{
	if (TSharedPtr<FRCSignatureTreeItemBase> Parent = GetParent())
	{
		return Parent->MutableCast<FRCSignatureTreeFieldItem>();
	}
	return nullptr;
}

FRCSignatureField* FRCSignatureTreeActionItem::FindParentFieldMutable(URemoteControlSignatureRegistry** OutRegistry)
{
	if (TSharedPtr<FRCSignatureTreeFieldItem> ParentFieldItem = GetParentFieldItem())
	{
		return ParentFieldItem->FindFieldMutable(OutRegistry);
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
