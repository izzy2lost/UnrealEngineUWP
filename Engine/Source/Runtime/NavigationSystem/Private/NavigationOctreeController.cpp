// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavigationOctreeController.h"
#include "AI/Navigation/NavRelevantInterface.h"
#include "NavigationSystem.h"


//----------------------------------------------------------------------//
// FNavigationOctreeController
//----------------------------------------------------------------------//
void FNavigationOctreeController::Reset()
{
	if (NavOctree.IsValid())
	{
		NavOctree->Destroy();
		NavOctree = NULL;
	}
	PendingOctreeUpdates.Empty(32);
}

void FNavigationOctreeController::SetNavigableGeometryStoringMode(FNavigationOctree::ENavGeometryStoringMode NavGeometryMode)
{
	check(NavOctree.IsValid());
	NavOctree->SetNavigableGeometryStoringMode(NavGeometryMode);
}

bool FNavigationOctreeController::GetNavOctreeElementData(const UObject& NodeOwner, ENavigationDirtyFlag& OutDirtyFlags, FBox& OutDirtyBounds)
{
	const FOctreeElementId2* ElementId = GetObjectsNavOctreeId(NodeOwner);
	if (ElementId != nullptr && IsValidElement(*ElementId))
	{
		// mark area occupied by given actor as dirty
		const FNavigationOctreeElement& ElementData = NavOctree->GetElementById(*ElementId);
		OutDirtyFlags = ElementData.Data->GetDirtyFlag();
		OutDirtyBounds = ElementData.Bounds.GetBox();
		return true;
	}

	return false;
}

// Deprecated
bool FNavigationOctreeController::GetNavOctreeElementData(const UObject& NodeOwner, int32& DirtyFlags, FBox& DirtyBounds)
{
	ENavigationDirtyFlag TmpDirtyFlags = ENavigationDirtyFlag::None;
	const bool bSuccess = GetNavOctreeElementData(NodeOwner, TmpDirtyFlags, DirtyBounds);
	DirtyFlags = static_cast<int32>(TmpDirtyFlags);
	return bSuccess;
}

const FNavigationRelevantData* FNavigationOctreeController::GetDataForObject(const UObject& Object) const
{
	const FOctreeElementId2* ElementId = GetObjectsNavOctreeId(Object);

	if (ElementId != nullptr && IsValidElement(*ElementId))
	{
		return NavOctree->GetDataForID(*ElementId);
	}

	return nullptr;
}

FNavigationRelevantData* FNavigationOctreeController::GetMutableDataForObject(const UObject& Object)
{
	return const_cast<FNavigationRelevantData*>(GetDataForObject(Object));
}