// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSimulationUtils.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "Chaos/CacheManagerActor.h"
#include "Chaos/Adapters/CacheAdapter.h"
#include "Dataflow/DataflowContent.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowSimulationNodes.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "Features/IModularFeatures.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"


#define LOCTEXT_NAMESPACE "DataflowSimulationGenerator"

namespace Dataflow
{
	bool ShouldResetWorld(const TObjectPtr<UDataflow>& SimulationGraph, const TObjectPtr<UWorld>& SimulationWorld, Dataflow::FTimestamp& LastTimeStamp)
	{
		if(const TSharedPtr<Dataflow::FGraph> DataflowGraph = SimulationGraph->GetDataflow())
		{
			Dataflow::FTimestamp MaxTimeStamp = Dataflow::FTimestamp::Invalid;
			for(const TSharedPtr<FDataflowNode>& TerminalNode : DataflowGraph->GetFilteredNodes(FDataflowTerminalNode::StaticType()))
			{
				MaxTimeStamp.Value = FMath::Max(MaxTimeStamp.Value, TerminalNode->GetTimestamp().Value);
			}
			if(MaxTimeStamp.Value > LastTimeStamp.Value)
			{
				LastTimeStamp = MaxTimeStamp.Value;
				return true;
			}
		}
		return false;
	}
	
	void EvaluateSimulationGraph(const TObjectPtr<UDataflow>& SimulationGraph, const TSharedPtr<Dataflow::FDataflowSimulationContext>& SimulationContext,
		const float DeltaTime, const float SimulationTime)
	{
		if(SimulationContext.IsValid())
		{
			SimulationContext->SetTimingInfos(DeltaTime, SimulationTime);
			
			if(SimulationGraph)
			{
				if(const TSharedPtr<Dataflow::FGraph> DataflowGraph = SimulationGraph->GetDataflow())
				{
					// Invalidation of all the simulation nodes that are always dirty
					for(const TSharedPtr<FDataflowNode>& InvalidNode : DataflowGraph->GetFilteredNodes(FDataflowInvalidNode::StaticType()))
					{
						InvalidNode->Invalidate();
					}
					
					// Pull the graph evaluation from the solver nodes
					for(const TSharedPtr<FDataflowNode>& ExecutionNode : DataflowGraph->GetFilteredNodes(FDataflowExecutionNode::StaticType()))
					{
						SimulationContext->Evaluate(ExecutionNode.Get(), nullptr);
					}
				}
			}
		}
	}
	
	TObjectPtr<AActor> SpawnSimulatedActor(const TSubclassOf<AActor>& ActorClass,
		const TObjectPtr<AChaosCacheManager>& CacheManager, const TObjectPtr<UChaosCacheCollection>& CacheCollection,
		const bool bIsRecording, const TObjectPtr<UDataflowBaseContent>& DataflowContent)
	{
		if(CacheManager)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Name = TEXT("CacheActor");
			SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
			SpawnParameters.Owner = CacheManager.Get(); 
			SpawnParameters.bDeferConstruction = true;
			
			TObjectPtr<AActor> PreviewActor = CacheManager->GetWorld()->SpawnActor<AActor>(ActorClass, SpawnParameters);
			if(PreviewActor)
			{
				// Link the editor content properties to the BP actor one 
				DataflowContent->SetActorProperties(PreviewActor);

				// Finish spawning
				PreviewActor->FinishSpawning(FTransform::Identity, true);
			}

			CacheManager->CacheCollection = CacheCollection;
			CacheManager->StartMode = EStartMode::Timed;
			CacheManager->CacheMode = bIsRecording ? ECacheMode::Record : ECacheMode::None;
			
			// Get the implementation of our adapters for identifying compatible components
			IModularFeatures&                      ModularFeatures = IModularFeatures::Get();
			TArray<Chaos::FComponentCacheAdapter*> Adapters = ModularFeatures.GetModularFeatureImplementations<Chaos::FComponentCacheAdapter>(Chaos::FComponentCacheAdapter::FeatureName);

			if(PreviewActor)
			{
				TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
				PreviewActor->GetComponents(PrimComponents);
	
				for(UPrimitiveComponent* PrimComponent : PrimComponents)
				{
					if(Chaos::FAdapterUtil::GetBestAdapterForClass(PrimComponent->GetClass(), false))
					{
						const FName ChannelName(PrimComponent->GetName());
						CacheManager->FindOrAddObservedComponent(PrimComponent, ChannelName, true);
					}
				}
			}
			return PreviewActor;
		}
		return nullptr;
	}
	
	void SetupSkeletonAnimation(const TObjectPtr<AActor>& PreviewActor)
	{
		if(PreviewActor)
		{
			TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
			PreviewActor->GetComponents(PrimComponents);

			for(UPrimitiveComponent* PrimComponent : PrimComponents)
			{
				if(USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(PrimComponent))
				{
					SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
					SkeletalMeshComponent->InitAnim(true);
					
					if(UAnimSingleNodeInstance* AnimNodeInstance = SkeletalMeshComponent->GetSingleNodeInstance())
					{
						// Setup the animation instance
						AnimNodeInstance->SetAnimationAsset(SkeletalMeshComponent->AnimationData.AnimToPlay);
						AnimNodeInstance->InitializeAnimation();

						// Update the anim data
						SkeletalMeshComponent->AnimationData.PopulateFrom(AnimNodeInstance);
#if WITH_EDITOR
						SkeletalMeshComponent->ValidateAnimation();
#endif

						// Stop the animation 
						AnimNodeInstance->SetLooping(true);
						AnimNodeInstance->SetPlaying(false);
						
					}
				}
			}
		}
	}

	void UpdateSkeletonAnimation(const TObjectPtr<AActor>& PreviewActor, const float SimulationTime)
	{
		if(PreviewActor)
		{
			TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
			PreviewActor->GetComponents(PrimComponents);

			// Update all the animation time 
			for(UPrimitiveComponent* PrimComponent : PrimComponents)
			{
				if(USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(PrimComponent))
				{
					SkeletalMeshComponent->SetPosition(SimulationTime);
					SkeletalMeshComponent->TickAnimation(0.f, false /*bNeedsValidRootMotion*/);
					SkeletalMeshComponent->RefreshBoneTransforms(nullptr /*TickFunction*/);
				}
			}
		}
	}

	void StartSkeletonAnimation(const TObjectPtr<AActor>& PreviewActor)
	{
		if(PreviewActor)
		{
			TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
			PreviewActor->GetComponents(PrimComponents);

			for(UPrimitiveComponent* PrimComponent : PrimComponents)
			{
				if(const USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(PrimComponent))
				{
					if(UAnimSingleNodeInstance* AnimNodeInstance = SkeletalMeshComponent->GetSingleNodeInstance())
					{
						AnimNodeInstance->SetPlaying(true);
					}
				}
			}
		}
	}
	
	void PauseSkeletonAnimation(const TObjectPtr<AActor>& PreviewActor)
	{
		if(PreviewActor)
		{
			TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
			PreviewActor->GetComponents(PrimComponents);

			for(UPrimitiveComponent* PrimComponent : PrimComponents)
			{
				if(const USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(PrimComponent))
				{
					if(UAnimSingleNodeInstance* AnimNodeInstance = SkeletalMeshComponent->GetSingleNodeInstance())
					{
						AnimNodeInstance->SetPlaying(false);
					}
				}
			}
		}
	}
	
	void StepSkeletonAnimation(const TObjectPtr<AActor>& PreviewActor)
	{
		if(PreviewActor)
		{
			TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
			PreviewActor->GetComponents(PrimComponents);

			for(UPrimitiveComponent* PrimComponent : PrimComponents)
			{
				if(const USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(PrimComponent))
				{
					if(UAnimSingleNodeInstance* AnimNodeInstance = SkeletalMeshComponent->GetSingleNodeInstance())
					{
						AnimNodeInstance->SetPlaying(false);
						AnimNodeInstance->StepForward();
					}
				}
			}
		}
	}
	
}

#undef LOCTEXT_NAMESPACE
