// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCSignatureTreeFieldItem.h"
#include "RCSignatureTreeSignatureItem.h"
#include "RemoteControlSignature.h"
#include "RemoteControlSignatureRegistry.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "RCSignatureTreeFieldItem"

FRCSignatureTreeFieldItem::FRCSignatureTreeFieldItem(int32 InFieldIndex, const TSharedPtr<SRCSignatureTree>& InSignatureTree)
	: FRCSignatureTreeItemBase(InSignatureTree)
	, FieldIndex(InFieldIndex)
{
}

TOptional<bool> FRCSignatureTreeFieldItem::IsEnabled() const
{
	if (const FRCSignatureField* Field = FindField())
	{
		return Field->bEnabled;
	}
	return TOptional<bool>();
}

void FRCSignatureTreeFieldItem::SetEnabled(bool bInEnabled)
{
	URemoteControlSignatureRegistry* Registry;

	FRCSignatureField* Field = FindFieldMutable(&Registry);
	if (!Field || Field->bEnabled == bInEnabled)
	{
		return;
	}

	check(Registry);

	FScopedTransaction Transaction(bInEnabled
		? LOCTEXT("EnableField", "Enable Field")
		: LOCTEXT("DisableField", "Disable Field"));

	Registry->Modify();
	Field->bEnabled = bInEnabled;
}

FText FRCSignatureTreeFieldItem::GetDisplayNameText() const
{
	if (const FRCSignatureField* Field = FindField())
	{
		return FText::FromName(Field->FieldPath.GetFieldName());
	}
	return FText::GetEmpty();
}

FText FRCSignatureTreeFieldItem::GetDescription() const
{
	const FRCSignatureField* Field = FindField();
	if (!Field)
	{
		return FText::GetEmpty();
	}

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("ObjectPath"), FText::FromString(Field->ObjectRelativePath));
	Arguments.Add(TEXT("ClassPath"), FText::FromString(Field->SupportedClass.ToString()));

	return FText::Format(LOCTEXT("DescriptionFormat", "{ObjectPath} ({ClassPath})"), MoveTemp(Arguments));
}

int32 FRCSignatureTreeFieldItem::RemoveFromRegistry()
{
	URemoteControlSignatureRegistry* Registry;

	FRCSignature* Signature = FindParentSignature(&Registry);
	if (!Signature || !Signature->Fields.IsValidIndex(FieldIndex))
	{
		return 0;
	}

	check(Registry);

	FScopedTransaction Transaction(LOCTEXT("RemoveField", "Remove Field"));
	Registry->Modify();
	Signature->Fields.RemoveAt(FieldIndex);
	return 1;
}

FRCSignatureTreeSignatureItem* FRCSignatureTreeFieldItem::GetParentSignatureItem() const
{
	if (TSharedPtr<FRCSignatureTreeItemBase> Parent = GetParent())
	{
		return Parent->AsSignatureItem();
	}
	return nullptr;
}

const FRCSignatureField* FRCSignatureTreeFieldItem::FindField() const
{
	FRCSignatureTreeSignatureItem* ParentSignatureItem = GetParentSignatureItem();
	if (!ParentSignatureItem)
	{
		return nullptr;
	}

	const FRCSignature* Signature = ParentSignatureItem->FindSignature();
	if (!Signature || !Signature->Fields.IsValidIndex(FieldIndex))
	{
		return nullptr;
	}

	return &Signature->Fields[FieldIndex];
}

FRCSignatureField* FRCSignatureTreeFieldItem::FindFieldMutable(URemoteControlSignatureRegistry** OutRegistry)
{
	FRCSignature* Signature = FindParentSignature(OutRegistry);
	if (!Signature || !Signature->Fields.IsValidIndex(FieldIndex))
	{
		return nullptr;
	}
	return &Signature->Fields[FieldIndex];
}

FRCSignature* FRCSignatureTreeFieldItem::FindParentSignature(URemoteControlSignatureRegistry** OutRegistry)
{
	FRCSignatureTreeSignatureItem* ParentSignatureItem = GetParentSignatureItem();
	if (!ParentSignatureItem)
	{
		return nullptr;
	}

	URemoteControlSignatureRegistry* Registry = ParentSignatureItem->GetRegistry();

	FRCSignature* Signature = ParentSignatureItem->FindSignatureMutable(Registry);
	if (!Signature)
	{
		return nullptr;
	}

	if (OutRegistry)
	{
		*OutRegistry = Registry;	
	}
	return Signature;
}

#undef LOCTEXT_NAMESPACE
