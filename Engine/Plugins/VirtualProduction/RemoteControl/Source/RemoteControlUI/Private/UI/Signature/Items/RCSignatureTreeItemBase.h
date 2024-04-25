// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UI/BaseLogicUI/RCLogicModeBase.h"

class SRCSignatureTree;

/** Base class for any Item represented in the Signature Tree */
class FRCSignatureTreeItemBase : public FRCLogicModeBase
{
public:
	explicit FRCSignatureTreeItemBase(const TSharedPtr<SRCSignatureTree>& InSignatureTree);

	virtual TOptional<bool> IsEnabled() const
	{
		return TOptional<bool>();
	}

	virtual void SetEnabled(bool bInEnabled)
	{
	}

	virtual FText GetDisplayNameText() const = 0;

	virtual bool CanEditDisplayNameText() const
	{
		return false;
	}

	virtual void SetDisplayNameText(const FText& InText)
	{
	}

	virtual FText GetDescription() const = 0;

	virtual int32 RemoveFromRegistry() = 0;

	virtual class FRCSignatureTreeSignatureItem* AsSignatureItem()
	{
		return nullptr;
	}

	TConstArrayView<TSharedPtr<FRCSignatureTreeItemBase>> GetChildren() const
	{
		return Children;
	}

	TSharedPtr<FRCSignatureTreeItemBase> GetParent() const
	{
		return ParentWeak.Pin();
	}

	TSharedPtr<SRCSignatureTree> GetSignatureTree() const
	{
		return SignatureTreeWeak.Pin();
	}

	void RebuildChildren();

protected:
	virtual void GenerateChildren(TArray<TSharedPtr<FRCSignatureTreeItemBase>>& OutChildren) const
	{
	}

private:
	TArray<TSharedPtr<FRCSignatureTreeItemBase>> Children;

	TWeakPtr<FRCSignatureTreeItemBase> ParentWeak;

	TWeakPtr<SRCSignatureTree> SignatureTreeWeak;
};
