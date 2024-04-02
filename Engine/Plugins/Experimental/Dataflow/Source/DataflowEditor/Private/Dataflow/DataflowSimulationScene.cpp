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
	if (TObjectPtr<UDataflowBaseContent> DataflowContent = GetDataflowContent())
	{
		DataflowContent->RegisterWorldContent(this, RootSceneActor);
	}

	TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
	RootSceneActor->GetComponents(PrimComponents);

	for(UPrimitiveComponent* PrimComponent : PrimComponents)
	{
		PrimComponent->SelectionOverrideDelegate =
			UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FDataflowPreviewSceneBase::IsComponentSelected);
	}
}

FDataflowSimulationScene::~FDataflowSimulationScene()
{
	TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
	RootSceneActor->GetComponents(PrimComponents);

	for(UPrimitiveComponent* PrimComponent : PrimComponents)
	{
		PrimComponent->SelectionOverrideDelegate.Unbind();
	}

	if (TObjectPtr<UDataflowBaseContent> DataflowContent = GetDataflowContent())
	{
		DataflowContent->UnregisterWorldContent(this);
	}
}

void FDataflowSimulationScene::TickDataflowScene(const float DeltaSeconds)
{
	GetWorld()->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
}

#undef LOCTEXT_NAMESPACE

