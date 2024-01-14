// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowPreviewScene.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "AssetEditorModeManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Dataflow/CollectionRenderingPatternUtility.h"
#include "Dataflow/DataflowActor.h"
#include "Dataflow/DataflowComponent.h"
#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowEditorContent.h"
#include "Dataflow/DataflowEditorStyle.h"
#include "Dataflow/DataflowEditorUtil.h"
#include "Drawing/MeshElementsVisualizer.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "InteractiveTool.h"
#include "ModelingToolTargetUtil.h"
#include "Selection.h"

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

void FDataflowPreviewScene::Tick(float DeltaTime)
{
	//@todo(brice) : Make sure this is being called. 
	for (TObjectPtr<UInteractiveToolPropertySet>& Propset : PropertyObjectsToTick)
	{
		if (Propset)
		{
			if (Propset->IsPropertySetEnabled())
			{
				Propset->CheckAndUpdateWatched();
			}
			else
			{
				Propset->SilentUpdateWatched();
			}
		}
	}

	if (WireframeDraw)
	{
		WireframeDraw->OnTick(DeltaTime);
	}
}

void FDataflowPreviewScene::ReinitializeDynamicMeshComponents()
{
	Update();
}

void FDataflowPreviewScene::Update()
{
	// Some objects, like the UMeshElementsVisualizer and Settings Objects
	// are not part of a tool, so they won't get ticked.This member holds
	// ticked objects that get rebuilt on Update
	PropertyObjectsToTick.Empty();

	// Update the SkeletalMeshComponent for animation 
	// changes.
	UpdateSkeletalMeshComponent();

	// The preview scene for the construction view will be
	// cleared and rebuilt from scratch. This will genrate a 
	// list of UPrimitiveComponents for rendering.
	UpdateDynamicMeshComponents();

	// Attach a wireframe renderer to the DynamicMeshComponents
	UpdateWireframeMeshElementsVisualizer();


	// Manage Selection and Tool Interaction
	if (DataflowModeManager.IsValid())
	{
		USelection* SelectedComponents = DataflowModeManager->GetSelectedComponents();
		for (const TObjectPtr<UDynamicMeshComponent>& DynamicMeshComponent : DynamicMeshComponents)
		{
			SelectedComponents->DeselectAll();
			SelectedComponents->Select(DynamicMeshComponent);
			DynamicMeshComponent->PushSelectionToProxy();
		}

		// @todo(brice) : Deal with this
		// Update the context object with the ConstructionViewMode and Collection used to build the DynamicMeshComponents, so
		// tools know how to use the components.
		//UEditorInteractiveToolsContext* RestSpaceToolsContext = DataflowModeManager->GetInteractiveToolsContext();
		//UClothEditorContextObject* EditorContextObject = RestSpaceToolsContext->ContextObjectStore->FindContext<UClothEditorContextObject>();
		//if (ensure(EditorContextObject))
		//{
		//	EditorContextObject->SetCollection(ConstructionViewMode, Collection);
		//}
	}
}

void FDataflowPreviewScene::Exit()
{
	PropertyObjectsToTick.Empty();

	if (WireframeDraw)
	{
		WireframeDraw->Disconnect();
	}
	WireframeDraw = nullptr;

	ResetDynamicMeshComponents();
}


void FDataflowPreviewScene::ResetDynamicMeshComponents()
{
	USelection* SelectedComponents = DataflowModeManager->GetSelectedComponents();
	for(const TObjectPtr<UDynamicMeshComponent>& DynamicMeshComponent : DynamicMeshComponents)
	{
		DynamicMeshComponent->SelectionOverrideDelegate.Unbind();
		DynamicMeshComponent->UnregisterComponent();
		DynamicMeshComponent->DestroyComponent();

		if (SelectedComponents->IsSelected(DynamicMeshComponent))
		{
			SelectedComponents->Deselect(DynamicMeshComponent);
			DynamicMeshComponent->PushSelectionToProxy();
		}
	}
	DynamicMeshComponents.Reset();
}

TObjectPtr<UDynamicMeshComponent>& FDataflowPreviewScene::AddDynamicMeshComponent(UE::Geometry::FDynamicMesh3&& DynamicMesh, const TArray<UMaterialInterface*>& MaterialSet)
{
	TObjectPtr<UDynamicMeshComponent> DynamicMeshComponent = NewObject<UDynamicMeshComponent>(DynamicMeshActor);
		
	DynamicMeshComponent->SetMesh(MoveTemp(DynamicMesh));

	// @todo(Dataflow) : Material support
	// This is just to have a material, we should transfer the materials from the assets if they have them. 
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
	DynamicMeshComponent->OnMeshChanged.Add(FSimpleMulticastDelegate::FDelegate::CreateLambda([this](){}));

	DynamicMeshComponent->UpdateBounds();
		
	const int32 ElementIndex = DynamicMeshComponents.Emplace(DynamicMeshComponent);
	return DynamicMeshComponents[ElementIndex];
}

void FDataflowPreviewScene::UpdateDynamicMeshComponents()
{
	using namespace UE::Geometry;//FDynamicMesh3

	ResetDynamicMeshComponents();

	if (EditorContent)
	{
		TObjectPtr<UDataflow> DataflowAsset = EditorContent->GetDataflowAsset();
		TSharedPtr<Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext();
		if (DataflowAsset && DataflowContext)
		{
			for (const UDataflowEdNode* Target : DataflowAsset->GetRenderTargets())
			{
				if (Target)
				{
					FDynamicMesh3 DynamicMesh;
					DynamicMesh.EnableAttributes();

					TSharedPtr<FManagedArrayCollection> RenderCollection(new FManagedArrayCollection);
					GeometryCollection::Facades::FRenderingFacade Facade(*RenderCollection);
					Facade.DefineSchema();

					Target->Render(Facade, DataflowContext);
					Dataflow::Conversion::RenderingFacadeToDynamicMesh(Facade, DynamicMesh);

					if (Target == EditorContent->GetPrimarySelectedNode())
					{
						EditorContent->SetPrimaryRenderCollection(RenderCollection);
					}

					// post updates
					{
						// Use per-triangle normals for the 2D view
						//UE::Geometry::FMeshNormals::InitializeMeshToPerTriangleNormals(&LodMesh);
					}
					{
						//@todo(Dataflow) :: Add material support
						//SetUpDynamicMeshComponentMaterial(ClothFacade, *DynamicMeshComponent);
					}


					AddDynamicMeshComponent(MoveTemp(DynamicMesh), {});
				}
			}
		}

		EditorContent->SetIsDirty(false);
	}
}


void FDataflowPreviewScene::AddWireframeMeshElementsVisualizer()
{
	ensure(WireframeDraw==nullptr);
	if (DynamicMeshComponents.Num())
	{
		// Set up the wireframe display of the rest space mesh.

		WireframeDraw = NewObject<UMeshElementsVisualizer>(DynamicMeshActor);
		WireframeDraw->CreateInWorld(GetWorld(), FTransform::Identity);

		WireframeDraw->Settings->DepthBias = 2.0;
		WireframeDraw->Settings->bAdjustDepthBiasUsingMeshSize = false;
		WireframeDraw->Settings->bShowWireframe = true;
		WireframeDraw->Settings->bShowBorders = true;
		WireframeDraw->Settings->bShowUVSeams = false;

		WireframeDraw->WireframeComponent->BoundaryEdgeThickness = 2;

		WireframeDraw->SetMeshAccessFunction([this](UMeshElementsVisualizer::ProcessDynamicMeshFunc ProcessFunc) 
		{
			for (auto DynamicMeshComponent : DynamicMeshComponents) ProcessFunc(*DynamicMeshComponent->GetMesh());
		});

		for (auto DynamicMeshComponent : DynamicMeshComponents)
		{
			DynamicMeshComponent->OnMeshChanged.Add(FSimpleMulticastDelegate::FDelegate::CreateLambda([this]()
			{
				WireframeDraw->NotifyMeshChanged();
			}));

			const bool bRestSpaceMeshVisible = DynamicMeshComponent->GetVisibleFlag();
			WireframeDraw->Settings->bVisible = bRestSpaceMeshVisible && bConstructionViewWireframe;
		}

		// Some interactive tools will hide the input DynamicMeshComponent and create their own temporary PreviewMesh for visualization. If this
		// occurs, we should also hide the corresponding Wireframe and Seam drawing (and un-hide it when the tool finishes).
		/*
		* // @todo(brice) : Deal with this.
		UActorComponent::MarkRenderStateDirtyEvent.AddWeakLambda(this, [this](UActorComponent& ActorComponent)
		{
			if (!DynamicMeshComponent)
			{
				return;
			}
		    const bool bRestSpaceMeshVisible = DynamicMeshComponent->GetVisibleFlag();
			if (WireframeDraw)
			{
				WireframeDraw->Settings->bVisible = bRestSpaceMeshVisible && bConstructionViewWireframe;
			}
		});
		*/
		PropertyObjectsToTick.Add(WireframeDraw->Settings);
	}
}

void FDataflowPreviewScene::ResetWireframeMeshElementsVisualizer()
{
	if (WireframeDraw)
	{
		WireframeDraw->Disconnect();
	}
	WireframeDraw = nullptr;
}

void FDataflowPreviewScene::UpdateWireframeMeshElementsVisualizer()
{
	ResetWireframeMeshElementsVisualizer();
	AddWireframeMeshElementsVisualizer();
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

FBox FDataflowPreviewScene::SelectedComponentBounds() const
{
	FBox Bounds(ForceInit);
	if (DataflowModeManager.IsValid())
	{
		const USelection* const SelectedComponents = DataflowModeManager->GetSelectedComponents();
		for (int32 i = 0; i < SelectedComponents->Num(); ++i)
		{
			const UObject* const SelectedObject = SelectedComponents->GetSelectedObject(i);
			if (const UDynamicMeshComponent* const DynamicMeshComponent = Cast<UDynamicMeshComponent>(SelectedObject))
			{
				Bounds += DynamicMeshComponent->Bounds.GetBox();
			}
		}
	}
	return Bounds;
}

bool FDataflowPreviewScene::HasRenderableGeometry()
{
	for (auto& DynamicMeshComponent : DynamicMeshComponents)
	{
		if (DynamicMeshComponent->GetMesh()->TriangleCount() > 0)
		{
			return true;
		}
	}
	return false;
}


void FDataflowPreviewScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	FAdvancedPreviewScene::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(SkeletalMeshComponent);
	Collector.AddReferencedObject(SkeletalMeshActor);
	Collector.AddReferencedObject(EditorContent);
	Collector.AddReferencedObject(DynamicMeshActor);
	Collector.AddReferencedObjects(DynamicMeshComponents);
	Collector.AddReferencedObject(WireframeDraw);
}

#undef LOCTEXT_NAMESPACE

