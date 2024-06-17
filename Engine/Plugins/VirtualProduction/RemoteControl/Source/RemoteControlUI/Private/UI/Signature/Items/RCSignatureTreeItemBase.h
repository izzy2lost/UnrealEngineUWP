// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UI/BaseLogicUI/RCLogicModeBase.h"

class SRCSignatureTree;

enum ERCSignatureTreeItemFlags : uint8
{
	None = 0,
	Expanded = 1 << 0,
	Selected = 1 << 1,
};
ENUM_CLASS_FLAGS(ERCSignatureTreeItemFlags)

/** Base class for any Item represented in the Signature Tree */
class FRCSignatureTreeItemBase : public FRCLogicModeBase
{
	friend class FRCSignatureTreeRootItem;

public:
	explicit FRCSignatureTreeItemBase(const TSharedPtr<SRCSignatureTree>& InSignatureTree);

	virtual TOptional<bool> IsEnabled() const
	{
		return TOptional<bool>();
	}

	virtual void SetEnabled(bool bInEnabled)
	{
	}

	ERCSignatureTreeItemFlags GetFlags() const
	{
		return Flags;
	}

	void AddFlags(ERCSignatureTreeItemFlags InFlags);

	void RemoveFlags(ERCSignatureTreeItemFlags InFlags);

	bool HasAnyFlags(ERCSignatureTreeItemFlags InFlags) const;

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

	void VisitChildren(TFunctionRef<bool(const TSharedPtr<FRCSignatureTreeItemBase>&)> InCallable, bool bInRecursive);

protected:
	virtual void BuildPathSegment(FStringBuilderBase& InBuilder) const = 0;

	virtual void GenerateChildren(TArray<TSharedPtr<FRCSignatureTreeItemBase>>& OutChildren) const
	{
	}

private:
	void Initialize(const TSharedPtr<FRCSignatureTreeItemBase>& InParent);

	void RestoreFrom(const TSharedPtr<FRCSignatureTreeItemBase>& InOldItem);

	/** Builds the path from the root to the item. Each item will be its own segment delimited by a dot */
	FName BuildPath() const;

	/** Unique path from the root to the item. Used to identify items in the Signature Tree */
	FName Path;

	TArray<TSharedPtr<FRCSignatureTreeItemBase>> Children;

	TWeakPtr<FRCSignatureTreeItemBase> ParentWeak;

	TWeakPtr<SRCSignatureTree> SignatureTreeWeak;

	ERCSignatureTreeItemFlags Flags = ERCSignatureTreeItemFlags::Expanded;
};
