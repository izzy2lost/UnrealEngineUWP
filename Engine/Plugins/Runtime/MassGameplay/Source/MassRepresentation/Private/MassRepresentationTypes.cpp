// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassRepresentationTypes.h"
#include "MassRepresentationUtils.h"
#include "MassCommandBuffer.h"
#include "Components/InstancedStaticMeshComponent.h"

DEFINE_LOG_CATEGORY(LogMassRepresentation);

namespace UE::Mass::Representation
{
void PushSwapTagsCommand(FMassCommandBuffer& CommandBuffer, const FMassEntityHandle Entity, const EMassVisibility PrevVisibility, const EMassVisibility NewVisibility)
{
#define CASE_SWAP_TAGS(OldVisibility, NewVisibility) \
	case NewVisibility: \
		CommandBuffer.SwapTags<TMassVisibilityTagForLevel<OldVisibility>::FTag, TMassVisibilityTagForLevel<NewVisibility>::FTag>(Entity); \
		break

#define CASE_ADD_TAG(NewVisibility) \
case NewVisibility: \
	CommandBuffer.AddTag<TMassVisibilityTagForLevel<NewVisibility>::FTag>(Entity); \
	break

#define DEFAULT_REMOVE_TAG(OldVisibility) \
case EMassVisibility::Max: /* fall through on purpose */ \
default: \
	CommandBuffer.RemoveTag<TMassVisibilityTagForLevel<OldVisibility>::FTag>(Entity); \
	break

	check(PrevVisibility != NewVisibility);

	switch (PrevVisibility)
	{
	case EMassVisibility::CanBeSeen:
		switch (NewVisibility)
		{
		CASE_SWAP_TAGS(EMassVisibility::CanBeSeen, EMassVisibility::CulledByFrustum);
		CASE_SWAP_TAGS(EMassVisibility::CanBeSeen, EMassVisibility::CulledByDistance);
		DEFAULT_REMOVE_TAG(EMassVisibility::CanBeSeen);
		}
		break;
	case EMassVisibility::CulledByFrustum:
		switch (NewVisibility)
		{
		CASE_SWAP_TAGS(EMassVisibility::CulledByFrustum, EMassVisibility::CanBeSeen);
		CASE_SWAP_TAGS(EMassVisibility::CulledByFrustum, EMassVisibility::CulledByDistance);
		DEFAULT_REMOVE_TAG(EMassVisibility::CulledByFrustum);
		}
		break;
	case EMassVisibility::CulledByDistance:
		switch (NewVisibility)
		{
		CASE_SWAP_TAGS(EMassVisibility::CulledByDistance, EMassVisibility::CanBeSeen);
		CASE_SWAP_TAGS(EMassVisibility::CulledByDistance, EMassVisibility::CulledByFrustum);
		DEFAULT_REMOVE_TAG(EMassVisibility::CulledByDistance);
		}
		break;
	case EMassVisibility::Max:
		switch (NewVisibility)
		{
		CASE_ADD_TAG(EMassVisibility::CanBeSeen);
		CASE_ADD_TAG(EMassVisibility::CulledByFrustum);
		CASE_ADD_TAG(EMassVisibility::CulledByDistance);
		default:
			checkf(false, TEXT("Unsupported Visibility types!"));
			break;
		}
		break;
	default:
		checkf(false, TEXT("Unsupported Visibility type!"));
		break;
	}

#undef CASE_SWAP_TAGS
}
} // UE::Mass::Representation

//-----------------------------------------------------------------------------
// FMassInstancedStaticMeshInfo
//-----------------------------------------------------------------------------
void FMassInstancedStaticMeshInfo::AddISMComponent(FMassISMCSharedData& SharedData)
{
	if (ensure(SharedData.GetISMComponent()))
	{
		const uint32 ISMComponentPathHash = GetTypeHash(SharedData.GetISMComponentChecked().GetPathName());
		AddISMComponent(ISMComponentPathHash, SharedData);
	}
}

void FMassInstancedStaticMeshInfo::ReplaceISMComponent(const uint32 PathHash, UInstancedStaticMeshComponent& ISMComponent)
{
	const int32 EntryIndex = PathHashes.Find(PathHash);
	if (EntryIndex != INDEX_NONE)
	{
		// using GetValid rather than TObjectPtr.Get since the latter won't fail if the object in question is marked as Garbage
		const UInstancedStaticMeshComponent* PrevInstance = GetValid(InstancedStaticMeshComponents[EntryIndex]);
		ensure(PrevInstance == nullptr || PrevInstance == &ISMComponent);
		if (PrevInstance != &ISMComponent)
		{
			InstancedStaticMeshComponents[EntryIndex] = &ISMComponent;
		}
	}
}

//-----------------------------------------------------------------------------
// FMassStaticMeshInstanceVisualizationMeshDesc
//-----------------------------------------------------------------------------
FMassStaticMeshInstanceVisualizationMeshDesc::FMassStaticMeshInstanceVisualizationMeshDesc()
{
	ISMComponentClass = UInstancedStaticMeshComponent::StaticClass();
}
