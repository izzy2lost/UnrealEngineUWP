// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSimulationScene.h"
#include "Dataflow/DataflowSimulationManager.h"
#include "Dataflow/DataflowEditor.h"
#include "Components/PrimitiveComponent.h"
#include "Chaos/CacheManagerActor.h"
#include "Misc/TransactionObjectEvent.h"
#include "EngineUtils.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "AssetEditorModeManager.h"
#include "Engine/Selection.h"

#define LOCTEXT_NAMESPACE "FDataflowSimulationScene"

//
// Simulation Scene
//

FDataflowSimulationScene::FDataflowSimulationScene(FPreviewScene::ConstructionValues ConstructionValues, UDataflowEditor* InEditor)
	: FDataflowPreviewSceneBase(ConstructionValues, InEditor)
{
	SceneDescription = NewObject<UDataflowSimulationSceneDescription>();
	SceneDescription->SetSimulationScene(this);

	SimulationGenerator = MakeShared<Dataflow::FDataflowSimulationGenerator>();
	RootSceneActor = GetWorld()->SpawnActor<AChaosCacheManager>();

	if(GetEditorContent())
	{
#if WITH_EDITORONLY_DATA
		if(SceneDescription->BlueprintClass == nullptr)
		{
			SceneDescription->BlueprintClass = GetEditorContent()->GetPreviewClass();
		}
		if(const UDataflow* DataflowAsset = GetEditorContent()->GetDataflowAsset())
		{
			SceneDescription->CacheParams = DataflowAsset->PreviewCacheParams;
			SceneDescription->CacheAsset = Cast<UChaosCacheCollection>(DataflowAsset->PreviewCacheAsset.Get());
		}
#endif
	}

	CreateSimulationScene();
}

FDataflowSimulationScene::~FDataflowSimulationScene()
{
	ResetSimulationScene();
}

void FDataflowSimulationScene::UnbindSceneSelection()
{
	if(PreviewActor)
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
		PreviewActor->GetComponents(PrimComponents);

		for(UPrimitiveComponent* PrimComponent : PrimComponents)
		{
			PrimComponent->SelectionOverrideDelegate.Unbind();
		}
	}
}

void FDataflowSimulationScene::ResetSimulationScene()
{
	// Release any selected components before the PreviewActor is deleted from the scene
	if (const TSharedPtr<FAssetEditorModeManager> ModeManager = GetDataflowModeManager())
	{
		if (USelection* const SelectedComponents = ModeManager->GetSelectedComponents())
		{
			SelectedComponents->DeselectAll();
		}
	}

	// Destroy the spawned root actor
	if(PreviewActor && GetWorld())
	{
		GetWorld()->DestroyActor(PreviewActor);

		GetWorld()->EditorDestroyActor(PreviewActor, true);
		// Since deletion can be delayed, rename to avoid future name collision
		// Call UObject::Rename directly on actor to avoid AActor::Rename which unnecessarily sunregister and re-register components
		PreviewActor->UObject::Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
	}
	
	// Unbind the scene selection
	UnbindSceneSelection();
}

void FDataflowSimulationScene::PauseSimulationScene() const
{
	if(SceneDescription && (SceneDescription->CacheAsset == nullptr))
	{
		GetWorld()->GetSubsystem<UDataflowSimulationManager>()->SetSimulationEnabled(false);
		Dataflow::PauseSkeletonAnimation(PreviewActor);
	}
}

void FDataflowSimulationScene::StartSimulationScene() const
{
	if(SceneDescription && (SceneDescription->CacheAsset == nullptr))
	{
		GetWorld()->GetSubsystem<UDataflowSimulationManager>()->SetSimulationEnabled(true);
		Dataflow::StartSkeletonAnimation(PreviewActor);
	}
}

void FDataflowSimulationScene::StepSimulationScene() const
{
	if(SceneDescription && (SceneDescription->CacheAsset == nullptr))
	{
		GetWorld()->GetSubsystem<UDataflowSimulationManager>()->SetSimulationEnabled(true);
		GetWorld()->GetSubsystem<UDataflowSimulationManager>()->SetSimulationStepping(true);
		Dataflow::StepSkeletonAnimation(PreviewActor);
	}
}

void FDataflowSimulationScene::RebuildSimulationScene(const bool bIsSimulationEnabled)
{
	if(SceneDescription && (SceneDescription->CacheAsset == nullptr))
	{
		// Unregister components, cache manager, selection...
		ResetSimulationScene();

		// Register components, cache manager, selection...
		CreateSimulationScene();

		// Override the simulation enabled flag
		GetWorld()->GetSubsystem<UDataflowSimulationManager>()->SetSimulationEnabled(bIsSimulationEnabled);
	}
}

void FDataflowSimulationScene::BindSceneSelection()
{
	if(PreviewActor)
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
		PreviewActor->GetComponents(PrimComponents);
		
		for(UPrimitiveComponent* PrimComponent : PrimComponents)
		{
			PrimComponent->SelectionOverrideDelegate =
				UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FDataflowPreviewSceneBase::IsComponentSelected);
		}
	}
}

void FDataflowSimulationScene::CreateSimulationScene()
{
	if(SimulationGenerator && SceneDescription && GetWorld())
	{
		SimulationGenerator->SetCacheParams(SceneDescription->CacheParams);
		SimulationGenerator->SetCacheAsset(SceneDescription->CacheAsset);
		SimulationGenerator->SetBlueprintClass(SceneDescription->BlueprintClass);
		SimulationGenerator->SetDataflowContent(GetEditorContent());

		TimeRange = SceneDescription->CacheParams.TimeRange;
		NumFrames = (TimeRange[1] > TimeRange[0]) ? FMath::Floor((TimeRange[1] - TimeRange[0]) * SceneDescription->CacheParams.FrameRate) : 0;
		
		PreviewActor = Dataflow::SpawnSimulatedActor(SceneDescription->BlueprintClass, Cast<AChaosCacheManager>(RootSceneActor),
			SceneDescription->CacheAsset, false, GetEditorContent());

		// Setup all the skelmesh animations
		Dataflow::SetupSkeletonAnimation(PreviewActor);
		
		GetWorld()->GetSubsystem<UDataflowSimulationManager>()->SetSimulationEnabled(false);
	}

	// update the selection binding since we are constantly editing the graph
	BindSceneSelection();
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
		if(Dataflow::ShouldResetWorld(EditorContent->GetDataflowAsset(), GetWorld(), LastTimeStamp) || EditorContent->IsSimulationDirty())
		{
			// Unregister components, cache manager, selection...
			ResetSimulationScene();

			// Register components, cache manager, selection...
			CreateSimulationScene();

			// Reset the dirty flag
			EditorContent->SetSimulationDirty(false);
		}
		// Load the cache at some point in time
		if(SceneDescription->CacheAsset)
		{
			// Update the cached simulation at some point in time
			if(RootSceneActor)
			{
				Cast<AChaosCacheManager>(RootSceneActor)->SetStartTime(SimulationTime);
			}
			// Update all the skelmesh animations at the simulation time
			Dataflow::UpdateSkeletonAnimation(PreviewActor, SimulationTime);
		}
	}
}

void FDataflowSimulationScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	FDataflowPreviewSceneBase::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(SceneDescription);
}

void FDataflowSimulationScene::SceneDescriptionPropertyChanged(const FName& PropertyName)
{
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDataflowSimulationSceneDescription, CacheParams))
	{
		if(SimulationGenerator)
		{
			SimulationGenerator->SetCacheParams(SceneDescription->CacheParams);
		}
	}
	else if(PropertyName == GET_MEMBER_NAME_CHECKED(UDataflowSimulationSceneDescription, CacheAsset))
	{
		if(SimulationGenerator)
		{
			SimulationGenerator->SetCacheAsset(SceneDescription->CacheAsset);
		}
	}
	else if(PropertyName == GET_MEMBER_NAME_CHECKED(UDataflowSimulationSceneDescription, BlueprintClass))
	{
		if(SimulationGenerator)
		{
			SimulationGenerator->SetBlueprintClass(SceneDescription->BlueprintClass);
		}
	}
	if(GetEditorContent())
	{
		if(UDataflow* DataflowAsset = GetEditorContent()->GetDataflowAsset())
		{
#if WITH_EDITORONLY_DATA
			DataflowAsset->PreviewCacheParams = SceneDescription->CacheParams;
			DataflowAsset->PreviewCacheAsset = SceneDescription->CacheAsset;
			DataflowAsset->PreviewBlueprintClass = SceneDescription->BlueprintClass;
#endif
		}
	}
	
	// Unregister components, cache manager, selection...
	ResetSimulationScene();

	// Register components, cache manager, selection...
	CreateSimulationScene();
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

