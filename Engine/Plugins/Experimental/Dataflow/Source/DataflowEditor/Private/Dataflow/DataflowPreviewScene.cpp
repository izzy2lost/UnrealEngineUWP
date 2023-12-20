// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowPreviewScene.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "AssetEditorModeManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Dataflow/DataflowActor.h"
#include "Dataflow/DataflowComponent.h"
#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowEditorContent.h"
#include "Dataflow/DataflowEditorStyle.h"
#include "Dataflow/DataflowEditorUtil.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "ModelingToolTargetUtil.h"

#define LOCTEXT_NAMESPACE "FDataflowPreviewScene"


FDataflowPreviewScene::FDataflowPreviewScene(FPreviewScene::ConstructionValues ConstructionValues,TObjectPtr<UDataflowEditorContent> InEditorContent) 
	: FAdvancedPreviewScene(ConstructionValues), EditorContent(InEditorContent)
{
	check(EditorContent);
	DynamicMeshActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());	
	DynamicMeshActor->RegisterAllComponents();
	SetFloorVisibility(false, true);
}

FDataflowPreviewScene::~FDataflowPreviewScene()
{
	ResetDynamicMeshComponents();
}

void FDataflowPreviewScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	FAdvancedPreviewScene::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(EditorContent);
	Collector.AddReferencedObject(DynamicMeshActor);
	Collector.AddReferencedObjects(DynamicMeshComponents);
}

void FDataflowPreviewScene::Update()
{
	using namespace UE::Geometry;//FDynamicMesh3

	// The preview scene for the construction view will be
	// cleared and rebuilt from scratch. This will genrate a 
	// list of UPrimitiveComponents for rendering.
	ResetDynamicMeshComponents();

	if (EditorContent)
	{
		TObjectPtr<UDataflow> DataflowAsset = EditorContent->DataflowAsset;
		TSharedPtr<Dataflow::FEngineContext> DataflowContext = EditorContent->DataflowContext;
		if(DataflowAsset && DataflowContext)
		{
			for (const UDataflowEdNode* Target : DataflowAsset->GetRenderTargets())
			{
				if (Target)
				{
					FDynamicMesh3 DynamicMesh;
					FManagedArrayCollection RenderCollection;
					GeometryCollection::Facades::FRenderingFacade Facade(RenderCollection);
					Facade.DefineSchema();

					Target->Render(Facade, DataflowContext);
					UE::Conversion::RenderingFacadeToDynamicMesh(Facade, DynamicMesh);
					AddDynamicMeshComponent(MoveTemp(DynamicMesh), {});
				}
			}
		}
	}
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

	// @todo(Material) This is just to have a material, we should transfer the materials from the assets if they have them. 
	if (FDataflowEditorStyle::Get().DefaultMaterial)
	{
		DynamicMeshComponent->ConfigureMaterialSet({ FDataflowEditorStyle::Get().DefaultMaterial });
	}
	else
	{
		DynamicMeshComponent->ValidateMaterialSlots(true, false);
	}
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

