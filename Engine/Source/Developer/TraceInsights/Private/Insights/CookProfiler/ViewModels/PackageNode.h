// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// TraceInsightsCore
#include "InsightsCore/Table/ViewModels/TableTreeNode.h"

// TraceInsights
#include "Insights/CookProfiler/ViewModels/PackageTable.h"
#include "Insights/CookProfiler/ViewModels/PackageEntry.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

class FPackageNode;

/** Type definition for shared pointers to instances of FTaskNode. */
typedef TSharedPtr<class FPackageNode> FPackageNodePtr;

/** Type definition for shared references to instances of FTaskNode. */
typedef TSharedRef<class FPackageNode> FPackageNodeRef;

/** Type definition for shared references to const instances of FTaskNode. */
typedef TSharedRef<const class FPackageNode> FPackageNodeRefConst;

/** Type definition for weak references to instances of FTaskNode. */
typedef TWeakPtr<class FTaskNode> FPackageNodeWeak;

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Class used to store information about a package node (used in the SPackageTableTreeView).
 */
class FPackageNode : public UE::Insights::FTableTreeNode
{
	INSIGHTS_DECLARE_RTTI(FPackageNode, UE::Insights::FTableTreeNode)

public:
	/** Initialization constructor for the Task node. */
	explicit FPackageNode(const FName InName, TWeakPtr<FPackageTable> InParentTable, int32 InRowIndex)
		: UE::Insights::FTableTreeNode(InName, InParentTable, InRowIndex)
	{
	}

	/** Initialization constructor for the group node. */
	explicit FPackageNode(const FName InGroupName, TWeakPtr<FPackageTable> InParentTable)
		: UE::Insights::FTableTreeNode(InGroupName, InParentTable)
	{
	}

	FPackageTable& GetPackageTableChecked() const
	{
		const TSharedPtr<UE::Insights::FTable>& TablePin = GetParentTable().Pin();
		check(TablePin.IsValid());
		return *StaticCastSharedPtr<FPackageTable>(TablePin);
	}

	bool IsValidPackage() const { return GetPackageTableChecked().IsValidRowIndex(GetRowIndex()); }
	const FPackageEntry* GetPackage() const { return GetPackageTableChecked().GetPackage(GetRowIndex()); }
	const FPackageEntry& GetPackageChecked() const { return GetPackageTableChecked().GetPackageChecked(GetRowIndex()); }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
