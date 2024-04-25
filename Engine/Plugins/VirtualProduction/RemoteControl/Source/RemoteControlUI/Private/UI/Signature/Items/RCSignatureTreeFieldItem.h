// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RCSignatureTreeItemBase.h"

class URemoteControlSignatureRegistry;
struct FRCSignature;
struct FRCSignatureField;

/** Item class representing a Field owned by a Signature */
class FRCSignatureTreeFieldItem : public FRCSignatureTreeItemBase
{
public:
	explicit FRCSignatureTreeFieldItem(int32 InFieldIndex, const TSharedPtr<SRCSignatureTree>& InSignatureTree);

	//~ Begin FRCSignatureTreeItemBase
	virtual TOptional<bool> IsEnabled() const override;
	virtual void SetEnabled(bool bInEnabled) override;
	virtual FText GetDisplayNameText() const override;
	virtual FText GetDescription() const override;
	virtual int32 RemoveFromRegistry() override;
	//~ End FRCSignatureTreeItemBase

private:
	FRCSignatureTreeSignatureItem* GetParentSignatureItem() const;

	const FRCSignatureField* FindField() const;

	FRCSignatureField* FindFieldMutable(URemoteControlSignatureRegistry** OutRegistry);

	FRCSignature* FindParentSignature(URemoteControlSignatureRegistry** OutRegistry);

	int32 FieldIndex;
};
