// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowPreviewScene.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "AssetEditorModeManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Dataflow/DataflowActor.h"
#include "Dataflow/DataflowComponent.h"
#include "Dataflow/DataflowEditor.h"
#include "Elements/Framework/EngineElementsLibrary.h"

#define LOCTEXT_NAMESPACE "FDataflowPreviewScene"

FDataflowPreviewScene::FDataflowPreviewScene(FPreviewScene::ConstructionValues ConstructionValues,TObjectPtr<UDataflowEditorContent> InEditorContent) 
	: FAdvancedPreviewScene(ConstructionValues), EditorContent(InEditorContent)
{
	check(EditorContent);
	SkeletalMeshActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());

	if(EditorContent->SkeletalMesh && EditorContent->bHasValidSkeletalMesh)
	{
		SkeletalMeshComponent = NewObject<USkeletalMeshComponent>(SkeletalMeshActor);
		SkeletalMeshComponent->SelectionOverrideDelegate = UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FDataflowPreviewScene::IsComponentSelected);
		SkeletalMeshComponent->SetDisablePostProcessBlueprint(true);
		UpdateSkeletalMeshComponent();
	}
	
	// @todo(DynamicMeshRendering) : Enable Dynamic Mesh Rendering for dataflow terminals. Hide the dataflow 
	DataflowActor = Cast<ADataflowActor>(GetWorld()->SpawnActor<ADataflowActor>(ADataflowActor::StaticClass()));
	DataflowComponent = DataflowActor->GetDataflowComponent();
	//DataflowComponent->SelectionOverrideDelegate = UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FDataflowPreviewScene::IsComponentSelected);
	//DataflowComponent->SetVisibility(false);
	UpdateDataflowComponent();
	

	DynamicMeshActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());	
	SkeletalMeshActor->RegisterAllComponents();
	DataflowActor->RegisterAllComponents();
	DynamicMeshActor->RegisterAllComponents();

	SetFloorVisibility(false, true);
}

FDataflowPreviewScene::~FDataflowPreviewScene()
{
	if (SkeletalMeshComponent)
	{
		SkeletalMeshComponent->TransformUpdated.RemoveAll(this);
		SkeletalMeshComponent->SelectionOverrideDelegate.Unbind();
		SkeletalMeshComponent->UnregisterComponent();
		SkeletalMeshComponent->DestroyComponent();
	}

	if (DataflowComponent)
	{
		DataflowComponent->SelectionOverrideDelegate.Unbind();
		DataflowComponent->UnregisterComponent();
		DataflowComponent->DestroyComponent();
	}

	ResetDynamicMeshComponents();
}

void FDataflowPreviewScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	FAdvancedPreviewScene::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(EditorContent);

	Collector.AddReferencedObject(DataflowComponent);
	Collector.AddReferencedObject(SkeletalMeshComponent);
	Collector.AddReferencedObject(SkeletalMeshActor);
	Collector.AddReferencedObject(DataflowActor);
	Collector.AddReferencedObject(DynamicMeshActor);
	Collector.AddReferencedObject(PreviewAnimInstance);
	Collector.AddReferencedObjects(DynamicMeshComponents);
}

void FDataflowPreviewScene::UpdateDataflowComponent()
{
	if (EditorContent->DataflowAsset)
	{
		DataflowComponent->ResetRenderTargets();

		DataflowComponent->SetDataflow(EditorContent->DataflowAsset);
		DataflowComponent->SetContext(EditorContent->DataflowContext);

		for (const UDataflowEdNode* const Node : EditorContent->DataflowAsset->GetRenderTargets())
		{
			DataflowComponent->AddRenderTarget(Node);
		}
		DataflowComponent->UpdateBounds();
	}
}

void FDataflowPreviewScene::UpdateSkeletalMeshComponent()
{
	SkeletalMeshComponent->SetSkeletalMeshAsset(EditorContent->SkeletalMesh);

	if (EditorContent->AnimationAsset)
	{
		PreviewAnimInstance = NewObject<UAnimSingleNodeInstance>(SkeletalMeshComponent);
		PreviewAnimInstance->SetAnimationAsset(EditorContent->AnimationAsset);

		SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		SkeletalMeshComponent->InitAnim(true);
		SkeletalMeshComponent->AnimationData.PopulateFrom(PreviewAnimInstance);
		SkeletalMeshComponent->AnimScriptInstance = PreviewAnimInstance;
		SkeletalMeshComponent->AnimScriptInstance->InitializeAnimation();
		SkeletalMeshComponent->ValidateAnimation();
	}
	else
	{
		SkeletalMeshComponent->Stop();
		SkeletalMeshComponent->AnimationData = FSingleAnimationPlayData();
		SkeletalMeshComponent->AnimScriptInstance = nullptr;
	}
	SkeletalMeshComponent->UpdateBounds();
}

void FDataflowPreviewScene::ResetDynamicMeshComponents()
{
	for(const TObjectPtr<UDynamicMeshComponent>& DynamicMeshComponent : DynamicMeshComponents)
	{
		DynamicMeshComponent->SelectionOverrideDelegate.Unbind();
		DynamicMeshComponent->UnregisterComponent();
		DynamicMeshComponent->DestroyComponent();
	}
	DynamicMeshComponents.Reset();
}

TObjectPtr<UDynamicMeshComponent>& FDataflowPreviewScene::AddDynamicMeshComponent(UE::Geometry::FDynamicMesh3&& DynamicMesh, const TArray<UMaterialInterface*>& MaterialSet)
{
	TObjectPtr<UDynamicMeshComponent> DynamicMeshComponent = NewObject<UDynamicMeshComponent>(DynamicMeshActor);
		
	DynamicMeshComponent->SetMesh(MoveTemp(DynamicMesh));
	DynamicMeshComponent->ConfigureMaterialSet(MaterialSet);
	check(DynamicMeshComponent->ValidateMaterialSlots(false, false));
	DynamicMeshComponent->SelectionOverrideDelegate = UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FDataflowPreviewScene::IsComponentSelected);
	
	DynamicMeshComponent->RegisterComponentWithWorld(GetWorld());
	DynamicMeshComponent->UpdateBounds();
		
	const int32 ElementIndex = DynamicMeshComponents.Emplace(DynamicMeshComponent);
	return DynamicMeshComponents[ElementIndex];
}

FBox FDataflowPreviewScene::GetBoundingBox() const
{
	FBox SceneBounds(ForceInitToZero);
	for (const TObjectPtr<UDynamicMeshComponent>& MeshComponent : DynamicMeshComponents)
	{
		if (MeshComponent)
		{
			SceneBounds += MeshComponent->Bounds.GetBox();
		}
	}
	return SceneBounds;
}

bool FDataflowPreviewScene::IsComponentSelected(const UPrimitiveComponent* InComponent) const
{
	if(DataflowModeManager.IsValid())
	{
		if (const UTypedElementSelectionSet* const TypedElementSelectionSet = DataflowModeManager->GetEditorSelectionSet())
		{
			if (const FTypedElementHandle ComponentElement = UEngineElementsLibrary::AcquireEditorComponentElementHandle(InComponent))
			{
				const bool bElementSelected = TypedElementSelectionSet->IsElementSelected(ComponentElement, FTypedElementIsSelectedOptions());
				return bElementSelected;
			}
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE

