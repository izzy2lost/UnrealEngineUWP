// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextSchedulePort_AnimNextMeshComponentPose.h"

#include "GenerationTools.h"
#include "Graph/AnimNext_LODPose.h"
#include "Param/ParamStack.h"
#include "Scheduler/ScheduleTermContext.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "AnimNextStats.h"
#include "Component/AnimNextComponent.h"
#include "Component/AnimNextMeshComponent.h"
#include "GameFramework/Character.h"
#include "Scheduler/AnimNextScheduleGraphTask.h"
#include "Param/AnimNextParamUniversalObjectLocator.h"

DEFINE_STAT(STAT_AnimNext_Port_SkeletalMeshComponent);

void UAnimNextSchedulePort_AnimNextMeshComponentPose::Run(const UE::AnimNext::FScheduleTermContext& InContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Port_SkeletalMeshComponent);
	
	using namespace UE::AnimNext;

	const FParamStack& ParamStack = FParamStack::Get();

	const TObjectPtr<USkeletalMeshComponent>* ComponentPtr = ParamStack.GetParamPtr<TObjectPtr<USkeletalMeshComponent>>(GetMeshComponentParamId());
	if(ComponentPtr == nullptr)
	{
		return;
	}

	TObjectPtr<UAnimNextMeshComponent> Component = Cast<UAnimNextMeshComponent>(*ComponentPtr);
	if(Component == nullptr)
	{
		return;
	}
	
	const FAnimNextGraphLODPose* InputPose = InContext.GetLayerHandle().GetParamPtr<FAnimNextGraphLODPose>(GetTerms()[0].GetId());
	if(InputPose == nullptr)
	{
		return; 
	}

	if(!InputPose->LODPose.IsValid())
	{
		return;
	}
	
	USkeletalMesh* SkeletalMesh = Component->GetSkeletalMeshAsset();
	if(SkeletalMesh == nullptr)
	{
		return;
	}

	const UE::AnimNext::FReferencePose& RefPose = Component->GetReferencePose().ReferencePose.GetRef<UE::AnimNext::FReferencePose>();

	FMemMark MemMark(FMemStack::Get());

	TArray<FTransform, TMemStackAllocator<>> LocalSpaceTransforms;
	LocalSpaceTransforms.SetNumUninitialized(SkeletalMesh->GetRefSkeleton().GetNum());

	// Map LOD pose into local-space scratch buffer
	FGenerationTools::RemapPose(InputPose->LODPose, LocalSpaceTransforms);

	// Convert and dispatch to renderer
	Component->CompleteAndDispatch(
		RefPose.GetParentIndices(),
		RefPose.GetLODBoneIndexToMeshBoneIndexMap(InputPose->LODPose.LODLevel),
		LocalSpaceTransforms);
}

TConstArrayView<UE::AnimNext::FScheduleTerm> UAnimNextSchedulePort_AnimNextMeshComponentPose::GetTerms() const
{
	using namespace UE::AnimNext;

	static const FParamId InputId("UE_Internal_AnimNextMeshComponentPose_Input");

	static const FScheduleTerm Terms[] =
	{
		FScheduleTerm(InputId, FAnimNextParamType::GetType<FAnimNextGraphLODPose>(), EScheduleTermDirection::Input)
	};

	return Terms;
}

TConstArrayView<FAnimNextEditorParam> UAnimNextSchedulePort_AnimNextMeshComponentPose::GetRequiredParameters() const
{
	using namespace UE::AnimNext;

	if(RequiredParams.Num() == 0)
	{
		RequiredParams.Emplace(GetMeshComponentParamId().GetName(), FAnimNextParamType::GetType<TObjectPtr<UAnimNextMeshComponent>>(), GetCharacterInstanceId());
	}

	return RequiredParams;
}
