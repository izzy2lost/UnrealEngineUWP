// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCSignatureTreeSignatureItem.h"
#include "IRemoteControlUIModule.h"
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

bool FRCSignatureTreeSignatureItem::AddField(URemoteControlSignatureRegistry* InRegistry, const FRCExposesPropertyArgs& InPropertyArgs)
{
	FRCSignature* Signature = FindSignatureMutable(InRegistry);
	if (!Signature)
	{
		return false;
	}

	TArray<FRCSignatureField, TInlineAllocator<1>> Fields;
	{
		FRCFieldPathInfo PathInfo(InPropertyArgs.PropertyHandle->GeneratePathToProperty(), /*bSkipDuplicates*/true);
		Fields.Emplace(FRCSignatureField:: CreateField(MoveTemp(PathInfo), InPropertyArgs.OwnerObject.Get(), InPropertyArgs.GetProperty()));
	}

	return Signature->AddFields(Fields) > 0;
}

void FRCSignatureTreeSignatureItem::ApplySignature(TConstArrayView<TWeakObjectPtr<UObject>> InObjects)
{
	if (InObjects.IsEmpty())
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

	FScopedTransaction Transaction(LOCTEXT("ApplySignature", "Apply Signature"));

	int32 ExposedCount = Signature->ApplySignature(Preset, InObjects);

	if (ExposedCount == 0)
	{
		// Nothing was exposed, cancel transaction
		Transaction.Cancel();
	}
}

void FRCSignatureTreeSignatureItem::BuildPathSegment(FStringBuilderBase& InBuilder) const
{
	SignatureId.AppendString(InBuilder, EGuidFormats::DigitsLower);
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
