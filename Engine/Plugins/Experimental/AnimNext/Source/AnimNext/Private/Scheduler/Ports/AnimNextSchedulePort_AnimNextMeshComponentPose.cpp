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
#include "GameFramework/Character.h"
#include "Scheduler/AnimNextScheduleGraphTask.h"
#include "Param/AnimNextObjectCastLocatorFragment.h"
#include "Param/AnimNextObjectFunctionLocatorFragment.h"
#include "Param/AnimNextParamUniversalObjectLocator.h"
#include "Component/SkinnedMeshComponentExtensions.h"

DEFINE_STAT(STAT_AnimNext_Port_SkeletalMeshComponent);

namespace UE::AnimNext
{
	// TODO: Currently we hard-code the ACharacter mesh component as the source of the reference pose and LOD index, but this should be a pin-input in the
	// final schedule incarnation.
	TInstancedStruct<FAnimNextParamUniversalObjectLocator> GetCharacterInstanceId()
	{
		TInstancedStruct<FAnimNextParamUniversalObjectLocator> Locator = TInstancedStruct<FAnimNextParamUniversalObjectLocator>::Make();
		Locator.GetMutable().Locator.AddFragment<FAnimNextObjectFunctionLocatorFragment>(UAnimNextComponent::StaticClass()->FindFunctionByName("GetOwner"));
		Locator.GetMutable().Locator.AddFragment<FAnimNextObjectCastLocatorFragment>(ACharacter::StaticClass());
		return Locator;
	};

	FName GetCharacterInstanceIdName()
	{
		static FName Name(NAME_None);
		if (Name.IsNone())
		{
			const TInstancedStruct<FAnimNextParamUniversalObjectLocator>& Locator = GetCharacterInstanceId();
			Name = Locator.Get().ToName();
		}
		return Name;
	};

	FParamId GetMeshComponentParamId()
	{
		static const FParamId MeshComponentParamId("/Script/Engine.Character:Mesh", GetCharacterInstanceIdName());
		return MeshComponentParamId;
	}
}

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

	TObjectPtr<USkeletalMeshComponent> Component = Cast<USkeletalMeshComponent>(*ComponentPtr);
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

	UE::AnimNext::FDataHandle RefPoseHandle = UE::AnimNext::FDataRegistry::Get()->GetOrGenerateReferencePose(Component);
	const UE::AnimNext::FReferencePose& RefPose = RefPoseHandle.GetRef<UE::AnimNext::FReferencePose>();

	FMemMark MemMark(FMemStack::Get());

	TArray<FTransform, TMemStackAllocator<>> LocalSpaceTransforms;
	LocalSpaceTransforms.SetNumUninitialized(SkeletalMesh->GetRefSkeleton().GetNum());

	// Map LOD pose into local-space scratch buffer
	FGenerationTools::RemapPose(InputPose->LODPose, LocalSpaceTransforms);

	// Convert and dispatch to renderer
	UE::Anim::FSkinnedMeshComponentExtensions::CompleteAndDispatch(
		Component,
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
		RequiredParams.Emplace(GetMeshComponentParamId().GetName(), FAnimNextParamType::GetType<TObjectPtr<USkeletalMeshComponent>>(), GetCharacterInstanceId());
	}

	return RequiredParams;
}
