// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"
#include "RCSignatureTreeItemBase.h"
#include "UI/Signature/IRCSignatureItem.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class URemoteControlSignatureRegistry;
struct FRCExposesPropertyArgs;
struct FRCSignature;

/** Item class representing an RC Signature */
class FRCSignatureTreeSignatureItem : public FRCSignatureTreeItemBase, public IRCSignatureItem
{
public:
	explicit FRCSignatureTreeSignatureItem(const FRCSignature& InSignature, const TSharedPtr<SRCSignatureTree>& InSignatureTree);

	const FGuid& GetSignatureId() const;

	URemoteControlSignatureRegistry* GetRegistry() const;

	const FRCSignature* FindSignature() const;

	FRCSignature* FindSignatureMutable(URemoteControlSignatureRegistry* InRegistry);

	bool AddField(URemoteControlSignatureRegistry* InRegistry, const FRCExposesPropertyArgs& InPropertyArgs);

	//~ Begin IRCSignatureItem
	virtual void ApplySignature(TConstArrayView<TWeakObjectPtr<AActor>> InActors) override;
	//~ End IRCSignatureItem

protected:
	//~ Begin FRCSignatureTreeItem
	virtual void BuildPathSegment(FStringBuilderBase& InBuilder) const override;
	virtual TOptional<bool> IsEnabled() const override;
	virtual void SetEnabled(bool bInEnabled) override;
	virtual FText GetDisplayNameText() const override;
	virtual bool CanEditDisplayNameText() const override;
	virtual void SetDisplayNameText(const FText& InText) override;
	virtual FText GetDescription() const override;
	virtual int32 RemoveFromRegistry() override;
	virtual FRCSignatureTreeSignatureItem* AsSignatureItem() override;
	virtual void GenerateChildren(TArray<TSharedPtr<FRCSignatureTreeItemBase>>& OutChildren) const override;
	//~ End FRCSignatureTreeItem

private:
	TWeakObjectPtr<URemoteControlSignatureRegistry> RegistryWeak;

	FGuid SignatureId;
};
