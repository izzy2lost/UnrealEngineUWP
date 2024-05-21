// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshSimulationNodes.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/DataflowObject.h"
#include "Chaos/Matrix.h"
#include "Chaos/Adapters/CacheAdapter.h"
#include "Chaos/CacheManagerActor.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "BonePose.h"
#include "BoneContainer.h"
#include "BoneIndices.h"
#include "Features/IModularFeatures.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosFleshSimulationNodes)

namespace Dataflow
{
	void RegisterChaosCommonSimulationNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetSimulationTimeDataflowNode);
		
		static const FLinearColor CDefaultNodeBodyTintColor = FLinearColor(0.f, 0.f, 0.0f, 0.5f);
		
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("Simulation|Terminal", FLinearColor(1.0f, 0.0f, 0.0f), CDefaultNodeBodyTintColor);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("Simulation|Setup", FLinearColor(1.0f, 1.0f, 0.0f), CDefaultNodeBodyTintColor);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("Simulation|Update", FLinearColor(0.0f, 1.0f, 0.0f), CDefaultNodeBodyTintColor);
	}
	void RegisterChaosSkeletonSimulationNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetAnimationTimeRangeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FUpdateSkeletonAnimationDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetSkeletonAnimationDataflowNode);
	}
	void RegisterChaosFleshSimulationNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFleshSolverTerminalDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCreateFleshComponentDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCreateFleshSolverDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCreateFleshSkeletonDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FAddFleshToSolverDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FAdvanceFleshSolverDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBindFleshToSkeletonDataflowNode);
	}
}

//
// USkeletalGeneratorComponent
//

UDataflowSkeletalMeshComponent::UDataflowSkeletalMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UDataflowSkeletalMeshComponent::~UDataflowSkeletalMeshComponent() = default;

void UDataflowSkeletalMeshComponent::FlipSpaceBuffer()
{
	bNeedToFlipSpaceBaseBuffers = true;
}

void FGetSimulationTimeDataflowNode::EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const
{
	SetValue(SimulationContext, FDataflowSimulationTime(SimulationContext.GetDeltaTime(), SimulationContext.GetSimulationTime()), &SimulationTime);
}

void FCreateFleshSkeletonDataflowNode::EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const
{
	const TObjectPtr<UFleshComponent> FleshPhysicsComponent = GetValue<TObjectPtr<UFleshComponent>>(SimulationContext, &FleshComponent);

	if (Output->IsA<TObjectPtr<USkeletalMeshComponent>>(&SkeletalMesh))
	{
		if(SimulationContext.HasSimulationFlag(Dataflow::ESimulationFlags::SetupSimulation))
		{
			if(const TObjectPtr<AActor> RootActor = SimulationContext.GetRootActor())
			{
				TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = NewObject<UDataflowSkeletalMeshComponent>(RootActor);
				
				SkeletalMeshComponent->SetDisablePostProcessBlueprint(true);
				if(FleshPhysicsComponent)
				{
					SkeletalMeshComponent->SetSkeletalMesh(FleshPhysicsComponent->GetRestCollection()->SkeletalMesh);
					SkeletalMeshComponent->UpdateBounds();
				}

				SkeletalMeshComponent->RegisterComponentWithWorld(RootActor->GetWorld());
				SetValue(SimulationContext, SkeletalMeshComponent, &SkeletalMesh);
			}
		}
	}
}

void FSetSkeletonAnimationDataflowNode::EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const
{
	const TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = GetValue<TObjectPtr<USkeletalMeshComponent>>(SimulationContext, &SkeletalMesh);
	const TObjectPtr<UAnimationAsset> FleshAnimationAsset = GetValue<TObjectPtr<UAnimationAsset>>(SimulationContext, &AnimationAsset);

	if (Output->IsA<TObjectPtr<USkeletalMeshComponent>>(&SkeletalMesh))
	{
		if(SimulationContext.HasSimulationFlag(Dataflow::ESimulationFlags::SetupSimulation))
		{
			SkeletalMeshComponent->Stop();
			SkeletalMeshComponent->AnimationData = FSingleAnimationPlayData();
			SkeletalMeshComponent->AnimScriptInstance = nullptr;
			
			if(FleshAnimationAsset)
			{
				const TObjectPtr<UAnimSingleNodeInstance> AnimationNodeInstance = NewObject<UAnimSingleNodeInstance>(SkeletalMeshComponent);
				AnimationNodeInstance->SetAnimationAsset(FleshAnimationAsset);
				AnimationNodeInstance->SetPlaying(false);
				
				SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
 				SkeletalMeshComponent->InitAnim(true);
				SkeletalMeshComponent->AnimationData.PopulateFrom(AnimationNodeInstance);
				SkeletalMeshComponent->AnimScriptInstance = AnimationNodeInstance;
				SkeletalMeshComponent->AnimScriptInstance->InitializeAnimation();

#if WITH_EDITOR
				SkeletalMeshComponent->ValidateAnimation();
#endif
			}
		}
	}
	SetValue(SimulationContext, SkeletalMeshComponent, &SkeletalMesh);
}

void FBindFleshToSkeletonDataflowNode::EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const
{
	const TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = GetValue<TObjectPtr<USkeletalMeshComponent>>(SimulationContext, &SkeletalMesh);
	const TObjectPtr<UDeformableSolverComponent> FleshSolverComponent = GetValue<TObjectPtr<UDeformableSolverComponent>>(SimulationContext, &FleshSolver);

	if(SkeletalMeshComponent && FleshSolverComponent)
	{
		if(SimulationContext.HasSimulationFlag(Dataflow::ESimulationFlags::UpdateSimulation))
		{
			// @todo: loop over all the flesh components to find the matching one and create deformable data
			// For now everything is done automatically in the simulate but we will need to split that
		}
	}
	SetValue(SimulationContext, FleshSolverComponent, &FleshSolver);
}

void FGetAnimationTimeRangeDataflowNode::EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const
{
	const TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = GetValue<TObjectPtr<USkeletalMeshComponent>>(SimulationContext, &SkeletalMesh);

	if (Output->IsA<FVector2f>(&TimeRange))
	{
		if(SimulationContext.HasSimulationFlag(Dataflow::ESimulationFlags::SetupSimulation))
		{
			bool bHasValue = false;
			if(SkeletalMeshComponent && SkeletalMeshComponent->GetAnimInstance())
			{
				if(const UAnimSingleNodeInstance* SingleNodeInstance = Cast<UAnimSingleNodeInstance>(SkeletalMeshComponent->GetAnimInstance()))
				{
					if(const UAnimSequenceBase* AnimSequence = Cast<UAnimSequenceBase>(SingleNodeInstance->GetAnimationAsset()))
					{
						const FVector2f AnimationAssetRange = FVector2f(0.0f, AnimSequence->GetPlayLength());
						SetValue(SimulationContext, AnimationAssetRange, &TimeRange);
						bHasValue = true;
					}
				}
			}
			if(!bHasValue)
			{
				SetValue(SimulationContext, FVector2f(0.0f, 5.0f), &TimeRange);
			}
		}
	}
}

static void FillComponentTransforms(const int32 NumBones, const FBoneContainer& BoneContainer, const FAnimationPoseData& AnimationPoseData,
	const FReferenceSkeleton* ReferenceSkeleton, TArray<FTransform>& ComponentSpaceTransforms) 
{
	ComponentSpaceTransforms.SetNumUninitialized(NumBones);
	for (int32 Index = 0; Index < NumBones; ++Index)
	{
		const FCompactPoseBoneIndex CompactIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(Index));
		const int32 ParentIndex = ReferenceSkeleton->GetParentIndex(Index);
		ComponentSpaceTransforms[Index] = (ComponentSpaceTransforms.IsValidIndex(ParentIndex) && ParentIndex < Index) ?
			AnimationPoseData.GetPose()[CompactIndex] * ComponentSpaceTransforms[ParentIndex] : ReferenceSkeleton->GetRefBonePose()[Index];

	}
}

static void FillAnimationDatas(const UAnimSequenceBase* AnimSequence, const float CurrentTime, USkeletalMesh* InSkeletalMesh, TArray<FTransform>& ComponentSpaceTransforms)
{
	const FAnimExtractContext ExtractionContext(FMath::Clamp(CurrentTime, 0., AnimSequence->GetPlayLength()));

	const FReferenceSkeleton* ReferenceSkeleton = &InSkeletalMesh->GetRefSkeleton();
	const int32 NumBones = ReferenceSkeleton ? ReferenceSkeleton->GetNum() : 0;

	TArray<FBoneIndexType> BoneIndices;
	BoneIndices.SetNumUninitialized(NumBones);
	for (int32 Index = 0; Index < NumBones; ++Index)
	{
		BoneIndices[Index] = static_cast<FBoneIndexType>(Index);
	}

	FBoneContainer BoneContainer;
	BoneContainer.SetUseRAWData(true);
	BoneContainer.InitializeTo(BoneIndices, UE::Anim::FCurveFilterSettings(), *InSkeletalMesh->GetSkeleton());
	
	FCompactPose CompactPose;
	CompactPose.SetBoneContainer(&BoneContainer);

	FBlendedCurve BlendedCurve;
	BlendedCurve.InitFrom(BoneContainer);

	UE::Anim::FStackAttributeContainer TempAttributes;
	FAnimationPoseData AnimationPoseData(CompactPose, BlendedCurve, TempAttributes);
	AnimSequence->GetAnimationPose(AnimationPoseData, ExtractionContext);

	FillComponentTransforms(NumBones, BoneContainer, AnimationPoseData, ReferenceSkeleton, ComponentSpaceTransforms);
}

void FUpdateSkeletonAnimationDataflowNode::EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const
{
	const TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = GetValue<TObjectPtr<USkeletalMeshComponent>>(SimulationContext, &SkeletalMesh);
	const float AnimationCurrentTime = GetValue<FDataflowSimulationTime>(SimulationContext, &SimulationTime).CurrentTime;

	if (Output->IsA<TObjectPtr<USkeletalMeshComponent>>(&SkeletalMesh))
	{
		if(SimulationContext.HasSimulationFlag(Dataflow::ESimulationFlags::UpdateSimulation))
		{
			if(SkeletalMeshComponent && SkeletalMeshComponent->GetAnimInstance())
			{
				if(UAnimSingleNodeInstance* SingleNodeInstance = Cast<UAnimSingleNodeInstance>(SkeletalMeshComponent->GetAnimInstance()))
				{
					if(const UAnimSequenceBase* AnimSequence = Cast<UAnimSequenceBase>(SingleNodeInstance->GetAnimationAsset()))
					{
						TArray<FTransform> ComponentSpaceTransforms;
						FillAnimationDatas(AnimSequence, AnimationCurrentTime, SkeletalMeshComponent->GetSkeletalMeshAsset(), ComponentSpaceTransforms);
						
						SkeletalMeshComponent->GetEditableComponentSpaceTransforms() = ComponentSpaceTransforms;
						if(const TObjectPtr<UDataflowSkeletalMeshComponent> DataflowSkeletalMesh =
							Cast<UDataflowSkeletalMeshComponent>(SkeletalMeshComponent))
						{
							DataflowSkeletalMesh->FlipSpaceBuffer();
						}
						SkeletalMeshComponent->FinalizeBoneTransform();
						SkeletalMeshComponent->UpdateBounds();
					}
				}
			}
		}
		SetValue(SimulationContext, SkeletalMeshComponent, &SkeletalMesh);
	}
}

void FUpdateSkeletonAnimationDataflowNode::SetAnimationTime(Dataflow::FSimulationContext& SimulationContext, const float AnimationTime)
{
	const TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = GetValue<TObjectPtr<USkeletalMeshComponent>>(SimulationContext, &SkeletalMesh);
	
	if(SkeletalMeshComponent && SkeletalMeshComponent->GetAnimInstance())
	{
		if(UAnimSingleNodeInstance* SingleNodeInstance = Cast<UAnimSingleNodeInstance>(SkeletalMeshComponent->GetAnimInstance()))
		{
			SingleNodeInstance->SetPosition(AnimationTime);
		}
	}
}

void FFleshSolverTerminalDataflowNode::EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const
{
	if(const TObjectPtr<AChaosCacheManager> ChaosCacheManager = Cast<AChaosCacheManager>(SimulationContext.GetRootActor()))
	{
		CacheAsset = ChaosCacheManager->CacheCollection;
	}
	const TObjectPtr<UDeformableSolverComponent> FleshSolverComponent = GetValue<TObjectPtr<UDeformableSolverComponent>>(SimulationContext, &FleshSolver);
	// @todo : call the cache events from the solver interface here
}

void FCreateFleshSolverDataflowNode::EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const
{
	if(SimulationContext.HasSimulationFlag(Dataflow::ESimulationFlags::SetupSimulation))
	{
		if(const TObjectPtr<AActor> RootActor = SimulationContext.GetRootActor())
		{
			TObjectPtr<UDeformableSolverComponent> InternalFleshSolver = NewObject<UDeformableSolverComponent>(RootActor);
		
			InternalFleshSolver->RegisterComponentWithWorld(RootActor->GetWorld());
		
			InternalFleshSolver->UpdateBounds();
			InternalFleshSolver->SetSimulationTicking(false);
			
			InternalFleshSolver->SolverTiming = SolverTiming;
			InternalFleshSolver->SolverEvolution = SolverEvolution;
			InternalFleshSolver->SolverCollisions = SolverCollisions;
			InternalFleshSolver->SolverConstraints = SolverConstraints;
			InternalFleshSolver->SolverForces = SolverForces;
			InternalFleshSolver->SolverDebugging = SolverDebugging;
			InternalFleshSolver->SolverMuscleActivation = SolverMuscleActivation;
			InternalFleshSolver->SolverTiming.FixTimeStep = true;

			SetValue(SimulationContext, InternalFleshSolver, &FleshSolver);
		}
	}
}

void FCreateFleshComponentDataflowNode::EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const
{
	if(SimulationContext.HasSimulationFlag(Dataflow::ESimulationFlags::SetupSimulation))
	{
		if(const TObjectPtr<AActor> RootActor = SimulationContext.GetRootActor())
		{	
			const FName ChannelName(SimulationContext.Graph->GetName() + TEXT("_") + GetName().ToString() + TEXT("_FleshComponent"));
			
			TObjectPtr<UFleshComponent> InternalFleshComponent = NewObject<UFleshComponent>(RootActor, ChannelName);
			InternalFleshComponent->RegisterComponentWithWorld(RootActor->GetWorld());

			// Create and add the visualisation component
			if(InternalFleshComponent->Mesh)
			{
				InternalFleshComponent->Mesh->RegisterComponentWithWorld(RootActor->GetWorld());
			}
			InternalFleshComponent->SetRestCollection(FleshAsset);
			InternalFleshComponent->UpdateBounds();

			if(const TObjectPtr<AChaosCacheManager> ChaosCacheManager = Cast<AChaosCacheManager>(SimulationContext.GetRootActor()))
			{
				// Get the implementation of our adapters for identifying compatible components
				IModularFeatures&                      ModularFeatures = IModularFeatures::Get();
				TArray<Chaos::FComponentCacheAdapter*> Adapters = ModularFeatures.GetModularFeatureImplementations<Chaos::FComponentCacheAdapter>(Chaos::FComponentCacheAdapter::FeatureName);
		
				if(Chaos::FAdapterUtil::GetBestAdapterForClass(InternalFleshComponent->GetClass(), false))
				{
					ChaosCacheManager->FindOrAddObservedComponent(InternalFleshComponent, ChannelName, true);
				}
			}
			SetValue(SimulationContext, InternalFleshComponent, &FleshComponent);
		}
	}
}

void FAddFleshToSolverDataflowNode::EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const
{
	const TObjectPtr<UDeformableSolverComponent> FleshSolverComponent = GetValue<TObjectPtr<UDeformableSolverComponent>>(SimulationContext, &FleshSolver);
	const TObjectPtr<UFleshComponent> FleshPhysicsComponent = GetValue<TObjectPtr<UFleshComponent>>(SimulationContext, &FleshComponent);

	if(FleshSolverComponent && FleshPhysicsComponent)
	{
		if(SimulationContext.HasSimulationFlag(Dataflow::ESimulationFlags::SetupSimulation))
		{
			FleshPhysicsComponent->EnableSimulation(FleshSolverComponent);
			FleshSolverComponent->Reset();
		}
	}
	
	SetValue(SimulationContext, FleshSolverComponent, &FleshSolver);
}

void FAdvanceFleshSolverDataflowNode::EvaluateSimulation(Dataflow::FSimulationContext& SimulationContext, const FDataflowOutput* Output) const
{
	const TObjectPtr<UDeformableSolverComponent> FleshSolverComponent = GetValue<TObjectPtr<UDeformableSolverComponent>>(SimulationContext, &FleshSolver);
	const float SimulationDeltaTime = GetValue<FDataflowSimulationTime>(SimulationContext, &SimulationTime).DeltaTime;
	
	if(FleshSolverComponent && (SimulationContext.GetCachingMode() == Dataflow::ECachingMode::RecordCache) &&
		SimulationContext.HasSimulationFlag(Dataflow::ESimulationFlags::UpdateSimulation))
	{
		const float SimulationDuration = SimulationDeltaTime + TimeOffset;
		const int32 NumSteps = FMath::Floor(SimulationDuration / FleshSolverComponent->SolverTiming.TimeStepSize);

		for(int32 StepIndex = 0; StepIndex < NumSteps; ++StepIndex)
		{
			FleshSolverComponent->UpdateFromGameThread(FleshSolverComponent->SolverTiming.TimeStepSize);
			FleshSolverComponent->Simulate(FleshSolverComponent->SolverTiming.TimeStepSize);
			FleshSolverComponent->UpdateFromSimulation(FleshSolverComponent->SolverTiming.TimeStepSize);
		}
		TimeOffset = SimulationDuration - NumSteps * FleshSolverComponent->SolverTiming.TimeStepSize;
	}
	
	SetValue(SimulationContext, FleshSolverComponent, &FleshSolver);
}