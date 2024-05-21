// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSimulationScene.h"
#include "Chaos/CacheManagerActor.h"
#include "Misc/TransactionObjectEvent.h"
#include "Dataflow/DataflowEditor.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "FDataflowSimulationScene"

//
// Simulation Scene
//

FDataflowSimulationScene::FDataflowSimulationScene(FPreviewScene::ConstructionValues ConstructionValues, UDataflowEditor* InEditor)
	: FDataflowPreviewSceneBase(ConstructionValues, InEditor)
{
	SimulationSceneDescription = NewObject<UDataflowSimulationSceneDescription>();
	SimulationSceneDescription->SetSimulationScene(this);

	SimulationGenerator = MakeShared<Dataflow::FDataflowSimulationGenerator>();

	const TObjectPtr<AChaosCacheManager> CacheManager = GetWorld()->SpawnActor<AChaosCacheManager>(AChaosCacheManager::StaticClass());

	CacheManager->StartMode = EStartMode::Timed;
	CacheManager->CacheMode = ECacheMode::None;
	
	RootSceneActor = CacheManager;

	CreateSimulationWorld();
}

FDataflowSimulationScene::~FDataflowSimulationScene()
{
	ResetSimulationWorld();
}

void FDataflowSimulationScene::UnbindSceneSelection()
{
	if(SimulationContext && SimulationContext->GetRootActor() && SimulationContext->GetSimulationWorld())
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
		SimulationContext->GetRootActor()->GetComponents(PrimComponents);

		for(UPrimitiveComponent* PrimComponent : PrimComponents)
		{
			PrimComponent->SelectionOverrideDelegate.Unbind();
		}
	}
}

void FDataflowSimulationScene::ResetSimulationWorld()
{
	if(SimulationContext && SimulationContext->GetRootActor())
	{
		// Unbind the scene selection
		UnbindSceneSelection();
		
		// Clear the simulation context
		CleanSimulationContext(SimulationContext);
	}
}

void FDataflowSimulationScene::BindSceneSelection()
{
	if(SimulationContext && SimulationContext->GetRootActor() && SimulationContext->GetSimulationWorld())
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
		SimulationContext->GetRootActor()->GetComponents(PrimComponents);
		
		for(UPrimitiveComponent* PrimComponent : PrimComponents)
		{
			PrimComponent->SelectionOverrideDelegate =
				UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FDataflowPreviewSceneBase::IsComponentSelected);
		}
	}
}

void FDataflowSimulationScene::CreateSimulationWorld()
{
	if(const TObjectPtr<AChaosCacheManager> CacheManager = Cast<AChaosCacheManager>(RootSceneActor))
	{
		if(const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
		{
			if(SimulationGenerator && SimulationSceneDescription)
			{
				SimulationGenerator->SetSimulationGraph(EditorContent->GetDataflowAsset());
				SimulationGenerator->SetSamplingRate(SimulationSceneDescription->SamplingRate);
				SimulationGenerator->SetCacheCollection(SimulationSceneDescription->CacheAsset);
		
				CacheManager->CacheCollection = SimulationSceneDescription->CacheAsset;
			}

			// Build the simulation context
			BuildSimulationContext(EditorContent->GetDataflowOwner(), EditorContent->GetDataflowAsset(),
				CacheManager, Dataflow::ECachingMode::PlayCache, SimulationContext);
		
			// Update the simulation time ranges and number of frames
			GetCacheDuration(EditorContent->GetDataflowAsset(), SimulationContext, SimulationSceneDescription->SamplingRate,
				TimeRange[0], TimeRange[1], NumFrames);

			// update the selection binding since we are constantly editing the graph
			BindSceneSelection();
		}
	}
}

void FDataflowSimulationScene::UpdateSimulationCache()
{
	if(SimulationGenerator.IsValid())
	{
		SimulationGenerator->RequestGeneratorAction(Dataflow::EDataflowGeneratorActions::StartGenerate);
	}
}

void FDataflowSimulationScene::TickDataflowScene(const float DeltaSeconds)
{
	GetWorld()->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);

	if(const TObjectPtr<UDataflowBaseContent>& EditorContent = GetEditorContent())
	{
		if( Dataflow::ShouldResetWorld(EditorContent->GetDataflowAsset(), SimulationContext,LastTimeStamp))
		{
			// unregister components, cache manager, selection...
			ResetSimulationWorld();

			// register components, cache manager, selection...
			CreateSimulationWorld();
		}

		// Load the cache at some point in time
		Cast<AChaosCacheManager>(RootSceneActor)->SetStartTime(SimulationTime);

		// Update all the animation at the simulation time
		Dataflow::UpdateAnimationNodes(EditorContent->GetDataflowAsset(), SimulationContext, SimulationTime);
	}
}

void FDataflowSimulationScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	FDataflowPreviewSceneBase::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(SimulationSceneDescription);
}

void FDataflowSimulationScene::SceneDescriptionPropertyChanged(const FName& PropertyName)
{
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDataflowSimulationSceneDescription, SamplingRate))
	{
		if(SimulationGenerator)
		{
			SimulationGenerator->SetSamplingRate(SimulationSceneDescription->SamplingRate);
		}
	}
	else if(PropertyName == GET_MEMBER_NAME_CHECKED(UDataflowSimulationSceneDescription, CacheAsset))
	{
		if(SimulationGenerator)
		{
			SimulationGenerator->SetCacheCollection(SimulationSceneDescription->CacheAsset);

			Cast<AChaosCacheManager>(RootSceneActor)->CacheCollection = SimulationSceneDescription->CacheAsset;
		}
	}
}

void UDataflowSimulationSceneDescription::SetSimulationScene(FDataflowSimulationScene* InSimulationScene)
{
	SimulationScene = InSimulationScene;
}

void UDataflowSimulationSceneDescription::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (SimulationScene)
	{
		SimulationScene->SceneDescriptionPropertyChanged(PropertyChangedEvent.GetMemberPropertyName());
	}

	DataflowSimulationSceneDescriptionChanged.Broadcast();
}

void UDataflowSimulationSceneDescription::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);

	// On Undo/Redo, PostEditChangeProperty just gets an empty FPropertyChangedEvent. However this function gets enough info to figure out which property changed
	if (TransactionEvent.GetEventType() == ETransactionObjectEventType::UndoRedo && TransactionEvent.HasPropertyChanges())
	{
		const TArray<FName>& PropertyNames = TransactionEvent.GetChangedProperties();
		for (const FName& PropertyName : PropertyNames)
		{
			SimulationScene->SceneDescriptionPropertyChanged(PropertyName);
		}
	}
}

#undef LOCTEXT_NAMESPACE

