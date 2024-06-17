// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCSignatureTreeItemBase.h"
#include "UI/Signature/SRCSignatureTree.h"

FRCSignatureTreeItemBase::FRCSignatureTreeItemBase(const TSharedPtr<SRCSignatureTree>& InSignatureTree)
	: FRCLogicModeBase(InSignatureTree ? InSignatureTree->GetRemoteControlPanel() : nullptr)
	, SignatureTreeWeak(InSignatureTree)
{
}

void FRCSignatureTreeItemBase::AddFlags(ERCSignatureTreeItemFlags InFlags)
{
	EnumAddFlags(Flags, InFlags);
}

void FRCSignatureTreeItemBase::RemoveFlags(ERCSignatureTreeItemFlags InFlags)
{
	EnumRemoveFlags(Flags, InFlags);
}

bool FRCSignatureTreeItemBase::HasAnyFlags(ERCSignatureTreeItemFlags InFlags) const
{
	return EnumHasAnyFlags(Flags, InFlags);
}

void FRCSignatureTreeItemBase::RebuildChildren()
{
	// Save current children in a map for restoring the flags
	TMap<FName, TSharedPtr<FRCSignatureTreeItemBase>> OldChildren;
	OldChildren.Reserve(Children.Num());
	for (const TSharedPtr<FRCSignatureTreeItemBase>& Child : Children)
	{
		OldChildren.Add(Child->Path, Child);
	}

	Children.Reset();
	GenerateChildren(Children);

	TSharedRef<FRCSignatureTreeItemBase> This = SharedThis(this);
	for (const TSharedPtr<FRCSignatureTreeItemBase>& Child : Children)
	{
		Child->Initialize(This);
		if (TSharedPtr<FRCSignatureTreeItemBase>* OldChild = OldChildren.Find(Child->Path))
		{
			Child->RestoreFrom(*OldChild);
		}
		Child->RebuildChildren();
	}
}

void FRCSignatureTreeItemBase::VisitChildren(TFunctionRef<bool(const TSharedPtr<FRCSignatureTreeItemBase>&)> InCallable, bool bInRecursive)
{
	for (const TSharedPtr<FRCSignatureTreeItemBase>& Child : Children)
	{
		if (!InCallable(Child))
		{
			break;
		}

		if (bInRecursive)
		{
			Child->VisitChildren(InCallable, /*bRecursive*/true);
		}
	}
}

void FRCSignatureTreeItemBase::Initialize(const TSharedPtr<FRCSignatureTreeItemBase>& InParent)
{
	ParentWeak = InParent;
	Path = BuildPath();
}

void FRCSignatureTreeItemBase::RestoreFrom(const TSharedPtr<FRCSignatureTreeItemBase>& InOldItem)
{
	if (InOldItem.IsValid())
	{
		// The only important things in restoration are flags & children.
		// Children are only restored so they can restore their flags
		Flags = InOldItem->GetFlags();
		Children = InOldItem->GetChildren();
	}
}

FName FRCSignatureTreeItemBase::BuildPath() const
{
	// For now, Signatures would have at most 2 items here, and Fields 3.
	TArray<const FRCSignatureTreeItemBase*, TInlineAllocator<3>> AncestorItems;

	// Generate Ancestor Path Array
	{
		const FRCSignatureTreeItemBase* CurrentItem = this;
		while (CurrentItem)
		{
			AncestorItems.Add(CurrentItem);
			CurrentItem = CurrentItem->GetParent().Get();
		}
	}

	// Expectation is that signature items will generate a path from Guid (32 characters)
	// with a dot delimiter to the field path which is an index (1-2 characters for the numbers in most use cases)
	// the expected size of the string is 34 characters... rounded up to a multiple of 8 => 40
	TStringBuilder<40> PathBuilder;

	// Build Path starting from the farthest Ancestor.
	for (const FRCSignatureTreeItemBase* Item : ReverseIterate(AncestorItems))
	{
		if (PathBuilder.Len() > 0)
		{
			PathBuilder << TEXT(".");
		}
		Item->BuildPathSegment(PathBuilder);
	}

	return PathBuilder.ToString();
}
