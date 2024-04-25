// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCSignatureTreeSignatureItem.h"
#include "RCSignatureTreeFieldItem.h"
#include "RemoteControlPreset.h"
#include "RemoteControlSignatureRegistry.h"
#include "ScopedTransaction.h"
#include "UI/Signature/SRCSignatureTree.h"

#define LOCTEXT_NAMESPACE "RCSignatureTreeSignatureItem"

FRCSignatureTreeSignatureItem::FRCSignatureTreeSignatureItem(const FRCSignature& InSignature, const TSharedPtr<SRCSignatureTree>& InSignatureTree)
	: FRCSignatureTreeItemBase(InSignatureTree)
	, SignatureId(InSignature.Id)
{
	if (URemoteControlPreset* Preset = GetPreset())
	{
		RegistryWeak = Preset->GetSignatureRegistry();
	}
}

const FGuid& FRCSignatureTreeSignatureItem::GetSignatureId() const
{
	return SignatureId;
}

URemoteControlSignatureRegistry* FRCSignatureTreeSignatureItem::GetRegistry() const
{
	return RegistryWeak.Get();
}

const FRCSignature* FRCSignatureTreeSignatureItem::FindSignature() const
{
	if (URemoteControlSignatureRegistry* SignatureRegistry = GetRegistry())
	{
		return SignatureRegistry->FindSignature(SignatureId);
	}
	return nullptr;
}

FRCSignature* FRCSignatureTreeSignatureItem::FindSignatureMutable(URemoteControlSignatureRegistry* InRegistry)
{
	if (InRegistry)
	{
		return InRegistry->FindSignatureMutable(SignatureId);
	}
	return nullptr;
}

void FRCSignatureTreeSignatureItem::AddFieldEntities(TConstArrayView<FGuid> InFieldEntityIds)
{
	if (InFieldEntityIds.IsEmpty())
	{
		return;
	}

	URemoteControlPreset* Preset = GetPreset();
	if (!Preset)
	{
		return;
	}

	URemoteControlSignatureRegistry* Registry = GetRegistry();

	FRCSignature* Signature = FindSignatureMutable(Registry);
	if (!Signature)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("AddProperties", "Add Properties"));
	Registry->Modify();

	const int32 AddedFieldCount = Signature->AddFieldsFromEntities(Preset, InFieldEntityIds);

	if (AddedFieldCount == 0)
	{
		// Nothing happened, cancel transaction
		Transaction.Cancel();
	}
	else if (TSharedPtr<SRCSignatureTree> SignatureTree = GetSignatureTree())
	{
		SignatureTree->Refresh();
	}
}

void FRCSignatureTreeSignatureItem::ApplySignature(TConstArrayView<TWeakObjectPtr<AActor>> InActors)
{
	if (InActors.IsEmpty())
	{
		return;
	}

	URemoteControlPreset* Preset = GetPreset();
	if (!Preset)
	{
		return;
	}

	const FRCSignature* Signature = FindSignature();
	if (!Signature || !Signature->bEnabled)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("ApplySignatureActors", "Apply Signature to Actors"));

	int32 ExposedCount = Signature->ApplySignature(Preset, InActors);

	if (ExposedCount == 0)
	{
		// Nothing was exposed, cancel transaction
		Transaction.Cancel();
	}
}

TOptional<bool> FRCSignatureTreeSignatureItem::IsEnabled() const
{
	if (const FRCSignature* Signature = FindSignature())
	{
		return Signature->bEnabled;
	}
	return TOptional<bool>();
}

void FRCSignatureTreeSignatureItem::SetEnabled(bool bInEnabled)
{
	URemoteControlSignatureRegistry* SignatureRegistry = GetRegistry();

	FRCSignature* Signature = FindSignatureMutable(SignatureRegistry);
	if (!Signature || Signature->bEnabled == bInEnabled)
	{
		return;
	}

	FScopedTransaction Transaction(bInEnabled
		? LOCTEXT("EnableSignature", "Enable Signature")
		: LOCTEXT("DisableSignature", "Disable Signature"));

	SignatureRegistry->Modify();
	Signature->bEnabled = bInEnabled;
}

FText FRCSignatureTreeSignatureItem::GetDisplayNameText() const
{
	if (const FRCSignature* Signature = FindSignature())
	{
		return Signature->DisplayName;
	}
	return FText::GetEmpty();
}

bool FRCSignatureTreeSignatureItem::CanEditDisplayNameText() const
{
	return true;
}

void FRCSignatureTreeSignatureItem::SetDisplayNameText(const FText& InText)
{
	URemoteControlSignatureRegistry* SignatureRegistry = GetRegistry();

	FRCSignature* Signature = FindSignatureMutable(SignatureRegistry);
	if (!Signature || Signature->DisplayName.EqualTo(InText))
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("SetSignatureName", "Set Signature Name"));
	SignatureRegistry->Modify();
	Signature->DisplayName = InText;
}

FText FRCSignatureTreeSignatureItem::GetDescription() const
{
	return FText::GetEmpty();
}

int32 FRCSignatureTreeSignatureItem::RemoveFromRegistry()
{
	URemoteControlSignatureRegistry* SignatureRegistry = GetRegistry();
	if (!SignatureRegistry)
	{
		return 0;
	}

	// Remove Signatures from Preset
	FScopedTransaction Transaction(LOCTEXT("RemoveSignature", "Remove Signature"));
	SignatureRegistry->Modify();
	return SignatureRegistry->RemoveSignature(SignatureId);
}

FRCSignatureTreeSignatureItem* FRCSignatureTreeSignatureItem::AsSignatureItem()
{
	return this;
}

void FRCSignatureTreeSignatureItem::GenerateChildren(TArray<TSharedPtr<FRCSignatureTreeItemBase>>& OutChildren) const
{
	const FRCSignature* Signature = FindSignature();
	if (!Signature)
	{
		return;
	}

	const TSharedPtr<SRCSignatureTree> SignatureTree = GetSignatureTree();

	OutChildren.Reserve(OutChildren.Num() + Signature->Fields.Num());
	for (int32 FieldIndex = 0; FieldIndex < Signature->Fields.Num(); ++FieldIndex)
	{
		OutChildren.Add(MakeShared<FRCSignatureTreeFieldItem>(FieldIndex, SignatureTree));
	}
}

#undef LOCTEXT_NAMESPACE
