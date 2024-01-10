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
	SkeletalMeshActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());
	if (EditorContent->GetSkeletalMesh())
	{
		SkeletalMeshComponent = NewObject<USkeletalMeshComponent>(SkeletalMeshActor);
		SkeletalMeshComponent->SelectionOverrideDelegate = UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FDataflowPreviewScene::IsComponentSelected);
		SkeletalMeshComponent->SetDisablePostProcessBlueprint(true);
		UpdateSkeletalMeshComponent();
	}
	SkeletalMeshActor->RegisterAllComponents();

	DynamicMeshActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());	
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

	ResetDynamicMeshComponents();
}

void FDataflowPreviewScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	FAdvancedPreviewScene::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(SkeletalMeshComponent);
	Collector.AddReferencedObject(SkeletalMeshActor);
	Collector.AddReferencedObject(EditorContent);
	Collector.AddReferencedObject(DynamicMeshActor);
	Collector.AddReferencedObjects(DynamicMeshComponents);
}

void FDataflowPreviewScene::Update()
{
	using namespace UE::Geometry;//FDynamicMesh3

	// Update the SkeletalMeshComponent for animation 
	// changes.
	UpdateSkeletalMeshComponent();


	// The preview scene for the construction view will be
	// cleared and rebuilt from scratch. This will genrate a 
	// list of UPrimitiveComponents for rendering.
	ResetDynamicMeshComponents();

	if (EditorContent)
	{
		TObjectPtr<UDataflow> DataflowAsset = EditorContent->GetDataflowAsset();
		TSharedPtr<Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext();
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

		EditorContent->SetIsDirty(false);
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

void FDataflowPreviewScene::UpdateSkeletalMeshComponent()
{
	if (SkeletalMeshComponent)
	{
		if (EditorContent->GetSkeletalMesh())
		{
			if (EditorContent->GetSkeletalMesh() != SkeletalMeshComponent->GetSkeletalMeshAsset())
			{
				SkeletalMeshComponent->SetSkeletalMeshAsset(EditorContent->GetSkeletalMesh());
			}

			if (EditorContent->GetAnimationAsset())
			{
				PreviewAnimInstance = NewObject<UAnimSingleNodeInstance>(SkeletalMeshComponent);
				PreviewAnimInstance->SetAnimationAsset(EditorContent->GetAnimationAsset());
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
		}
		else
		{
			SkeletalMeshComponent->SetSkeletalMeshAsset(nullptr);
			SkeletalMeshComponent->Stop();
			SkeletalMeshComponent->AnimationData = FSingleAnimationPlayData();
			SkeletalMeshComponent->AnimScriptInstance = nullptr;
		}

		SkeletalMeshComponent->UpdateBounds();
	}
}

FBox FDataflowPreviewScene::GetBoundingBox() const
{
	FBox SceneBounds(EForceInit::ForceInit);

	if (SkeletalMeshComponent)
	{
		FTransform ComponentTransform = SkeletalMeshComponent->GetComponentTransform();
		FBox LocalBox = SkeletalMeshComponent->GetLocalBounds().GetBox();
		SceneBounds += LocalBox.TransformBy(ComponentTransform);
	}

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

