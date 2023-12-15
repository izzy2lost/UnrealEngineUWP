// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowPreviewScene.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "AssetEditorModeManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Dataflow/DataflowComponent.h"
#include "Dataflow/DataflowEditor.h"
#include "Elements/Framework/EngineElementsLibrary.h"

#define LOCTEXT_NAMESPACE "FDataflowPreviewScene"

FDataflowPreviewScene::FDataflowPreviewScene(FPreviewScene::ConstructionValues ConstructionValues, FDataflowEditorDatas& DataflowAssetDatas) :
	FAdvancedPreviewScene(ConstructionValues), DataflowDatas(DataflowAssetDatas)
{
	SkeletalMeshActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());

	if(DataflowDatas.SkeletalMesh && DataflowDatas.bHasValidSkeletalMesh)
	{
		SkeletalMeshComponent = NewObject<USkeletalMeshComponent>(SkeletalMeshActor);
		SkeletalMeshComponent->SelectionOverrideDelegate = UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FDataflowPreviewScene::IsComponentSelected);
		SkeletalMeshComponent->SetDisablePostProcessBlueprint(true);
		UpdateSkeletalMeshComponent();
	}
	
	DataflowActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());

	if(DataflowDatas.DataflowAsset)
	{
		DataflowComponent = NewObject<UDataflowComponent>(DataflowActor);
		//DataflowComponent->SelectionOverrideDelegate = UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FDataflowPreviewScene::IsComponentSelected);
		
		// @todo(DynamicMeshRendering) : Enable Dynamic Mesh Rendering for dataflow terminals. Hide the dataflow
		//DataflowComponent->SetVisibility(false);
		UpdateDataflowComponent();
	}
	
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
	DataflowComponent->ResetRenderTargets();
	
	DataflowComponent->SetDataflow(DataflowDatas.DataflowAsset);
	DataflowComponent->SetContext(DataflowDatas.DataflowContext);
	
	for (const UDataflowEdNode* const Node : DataflowDatas.DataflowAsset->GetRenderTargets())
	{
		DataflowComponent->AddRenderTarget(Node);
	}
	DataflowComponent->UpdateBounds();
}

void FDataflowPreviewScene::UpdateSkeletalMeshComponent()
{
	SkeletalMeshComponent->SetSkeletalMeshAsset(DataflowDatas.SkeletalMesh);

	if (DataflowDatas.AnimationAsset)
	{
		PreviewAnimInstance = NewObject<UAnimSingleNodeInstance>(SkeletalMeshComponent);
		PreviewAnimInstance->SetAnimationAsset(DataflowDatas.AnimationAsset);

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

