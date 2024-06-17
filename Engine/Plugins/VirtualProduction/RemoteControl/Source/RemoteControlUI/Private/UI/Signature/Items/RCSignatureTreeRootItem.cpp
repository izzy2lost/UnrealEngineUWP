// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCSignatureTreeRootItem.h"
#include "RCSignatureTreeSignatureItem.h"
#include "RemoteControlSignature.h"
#include "RemoteControlSignatureRegistry.h"
#include "UI/Signature/SRCSignatureTree.h"

FRCSignatureTreeRootItem::FRCSignatureTreeRootItem(const TSharedPtr<SRCSignatureTree>& InSignatureTree)
	: FRCSignatureTreeItemBase(InSignatureTree)
{
}

void FRCSignatureTreeRootItem::GenerateChildren(TArray<TSharedPtr<FRCSignatureTreeItemBase>>& OutChildren) const
{
	TSharedPtr<SRCSignatureTree> SignatureTree = GetSignatureTree();
	if (!SignatureTree.IsValid())
	{
		return;
	}

	URemoteControlSignatureRegistry* SignatureRegistry = SignatureTree->GetSignatureRegistry();
	if (!SignatureRegistry)
	{
		return;
	}

	TConstArrayView<FRCSignature> Signatures = SignatureRegistry->GetSignatures();
	OutChildren.Reserve(OutChildren.Num() + Signatures.Num());

	for (const FRCSignature& Signature : Signatures)
	{
		OutChildren.Add(MakeShared<FRCSignatureTreeSignatureItem>(Signature, SignatureTree));
	}
}

FText FRCSignatureTreeRootItem::GetDisplayNameText() const
{
	checkNoEntry();
	return FText::GetEmpty();
}

FText FRCSignatureTreeRootItem::GetDescription() const
{
	checkNoEntry();
	return FText::GetEmpty();
}

int32 FRCSignatureTreeRootItem::RemoveFromRegistry()
{
	checkNoEntry();
	return 0;
}
