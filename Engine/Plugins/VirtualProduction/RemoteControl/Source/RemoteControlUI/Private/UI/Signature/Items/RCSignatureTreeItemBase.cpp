// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCSignatureTreeItemBase.h"
#include "UI/Signature/SRCSignatureTree.h"

FRCSignatureTreeItemBase::FRCSignatureTreeItemBase(const TSharedPtr<SRCSignatureTree>& InSignatureTree)
	: FRCLogicModeBase(InSignatureTree ? InSignatureTree->GetRemoteControlPanel() : nullptr)
	, SignatureTreeWeak(InSignatureTree)
{
}

void FRCSignatureTreeItemBase::RebuildChildren()
{
	Children.Reset();
	GenerateChildren(Children);

	TSharedRef<FRCSignatureTreeItemBase> This = SharedThis(this);
	for (const TSharedPtr<FRCSignatureTreeItemBase>& Child : Children)
	{
		Child->ParentWeak = This;
		Child->RebuildChildren();
	}
}
