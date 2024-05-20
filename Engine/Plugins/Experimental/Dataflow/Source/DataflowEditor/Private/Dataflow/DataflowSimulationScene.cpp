// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSimulationScene.h"

#include "Dataflow/DataflowEditor.h"
#include "Selection.h"

#define LOCTEXT_NAMESPACE "FDataflowSimulationScene"

//
// FDataflowSimulationScene
//

FDataflowSimulationScene::FDataflowSimulationScene(FPreviewScene::ConstructionValues ConstructionValues, UDataflowEditor* InEditor)
	: FDataflowPreviewSceneBase(ConstructionValues, InEditor)
{
	if(RootSceneActor)
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
		RootSceneActor->GetComponents(PrimComponents);

		for(UPrimitiveComponent* PrimComponent : PrimComponents)
		{
			PrimComponent->SelectionOverrideDelegate =
				UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FDataflowPreviewSceneBase::IsComponentSelected);
		}
	}
}

FDataflowSimulationScene::~FDataflowSimulationScene()
{
	if(RootSceneActor)
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
		RootSceneActor->GetComponents(PrimComponents);

		for(UPrimitiveComponent* PrimComponent : PrimComponents)
		{
			PrimComponent->SelectionOverrideDelegate.Unbind();
		}
	}
}

void FDataflowSimulationScene::TickDataflowScene(const float DeltaSeconds)
{
	GetWorld()->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
}

#undef LOCTEXT_NAMESPACE

