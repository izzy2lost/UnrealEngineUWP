// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"
#include "RCSignatureTreeItemBase.h"
#include "UI/Signature/IRCSignatureItem.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class IPropertyHandle;
class URemoteControlSignatureRegistry;
struct FRCSignature;

/** Item class representing an RC Signature */
class FRCSignatureTreeSignatureItem : public FRCSignatureTreeItemBase, public IRCSignatureItem
{
public:
	static constexpr ERCSignatureTreeItemType StaticItemType = ERCSignatureTreeItemType::Signature;

	explicit FRCSignatureTreeSignatureItem(const FRCSignature& InSignature, const TSharedPtr<SRCSignatureTree>& InSignatureTree);

	const FGuid& GetSignatureId() const;

	URemoteControlSignatureRegistry* GetRegistry() const;

	const FRCSignature* FindSignature() const;

	FRCSignature* FindSignatureMutable(URemoteControlSignatureRegistry* InRegistry);

	bool AddField(URemoteControlSignatureRegistry* InRegistry, const TSharedRef<IPropertyHandle>& InPropertyHandle);

	//~ Begin IRCSignatureItem
	virtual void ApplySignature(TConstArrayView<TWeakObjectPtr<UObject>> InObjects) override;
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
	virtual ERCSignatureTreeItemType GetItemType() const override { return StaticItemType; }
	virtual void GenerateChildren(TArray<TSharedPtr<FRCSignatureTreeItemBase>>& OutChildren) const override;
	//~ End FRCSignatureTreeItem

private:
	TWeakObjectPtr<URemoteControlSignatureRegistry> RegistryWeak;

	FGuid SignatureId;
};
