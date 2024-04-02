// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowConstructionScene.h"

#include "AssetEditorModeManager.h"
#include "Dataflow/CollectionRenderingPatternUtility.h"
#include "Dataflow/DataflowEditorCollectionComponent.h"
#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowEditorStyle.h"
#include "Drawing/MeshElementsVisualizer.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Selection.h"

#define LOCTEXT_NAMESPACE "FDataflowConstructionScene"

bool bDataflowShowWireframeInConstructionView = false;
FAutoConsoleVariableRef CVARDataflowShowWireframeInConstructionView(TEXT("p.Dataflow.Editor.Construction.ShowWireframe"), bDataflowShowWireframeInConstructionView, TEXT("Show the wireframe model in the dataflows construction view[def:true]"));

//
// Construction Scene
//

FDataflowConstructionScene::FDataflowConstructionScene(FPreviewScene::ConstructionValues ConstructionValues, UDataflowEditor* InEditor)
	: FDataflowPreviewSceneBase(ConstructionValues, InEditor)
{}

FDataflowConstructionScene::~FDataflowConstructionScene()
{
	ResetDynamicMeshComponents();
}

/** Hide all or a single component */
void FDataflowConstructionScene::SetVisibility(bool bVisibility, UActorComponent* InComponent)
{
	auto SetCollectionVisiblity = [](bool bVisibility,TObjectPtr<UDataflowEditorCollectionComponent> Component) {
		Component->SetVisibility(bVisibility);
		if (Component->WireframeComponent)
		{
			Component->WireframeComponent->SetVisibility(bVisibility);
		}
	};

	for (FRenderElement& RenderElement : DynamicMeshComponents)
	{
		if (TObjectPtr<UDataflowEditorCollectionComponent> DynamicMeshComponent = Cast<UDataflowEditorCollectionComponent>(RenderElement.Value))
		{
			if (InComponent != nullptr)
			{
				if (InComponent == DynamicMeshComponent.Get())
				{
					SetCollectionVisiblity(bVisibility,DynamicMeshComponent);
				}
			}
			else
			{
				SetCollectionVisiblity(bVisibility,DynamicMeshComponent);
			}
		}
	}
}


void FDataflowConstructionScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	FDataflowPreviewSceneBase::AddReferencedObjects(Collector);

	Collector.AddReferencedObjects(DynamicMeshComponents);
	Collector.AddReferencedObjects(WireframeElements);
}

FORCEINLINE Dataflow::FTimestamp LatestTimestamp(const UDataflow* Dataflow, const ::Dataflow::FContext* Context)
{
	if (Dataflow && Context)
	{
		return FMath::Max(Dataflow->GetRenderingTimestamp().Value, Context->GetTimestamp().Value);
	}
	return ::Dataflow::FTimestamp::Invalid;
}

void FDataflowConstructionScene::TickDataflowScene(const float DeltaSeconds)
{
	if (TObjectPtr<UDataflowBaseContent> DataflowContent = GetDataflowContent())
	{
		if (const TSharedPtr<Dataflow::FContext> DataflowContext = DataflowContent->GetDataflowContext())
		{
			if (const UDataflow* Dataflow = DataflowContent->GetDataflowAsset())
			{
				const Dataflow::FTimestamp SystemTimestamp = LatestTimestamp(Dataflow, DataflowContext.Get());
				if (SystemTimestamp >= DataflowContent->GetLastModifiedTimestamp() || DataflowContent->IsDirty())
				{
					DataflowContent->SetLastModifiedTimestamp(SystemTimestamp.Value + 1);

					if (DataflowContent->IsDirty())
					{
						UpdateConstructionScene();
					}
				}
			}
		}
	}
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

	for (FRenderWireElement Elem : WireframeElements)
	{
		Elem.Value->OnTick(DeltaSeconds);
	}
}

void FDataflowConstructionScene::UpdateDynamicMeshComponents()
{
	using namespace UE::Geometry;//FDynamicMesh3

	// The preview scene for the construction view will be
	// cleared and rebuilt from scratch. This will genrate a 
	// list of UPrimitiveComponents for rendering.
	ResetDynamicMeshComponents();

	if (TObjectPtr<UDataflowBaseContent> DataflowContent = GetDataflowContent())
	{
		const TObjectPtr<UDataflow>& DataflowAsset = DataflowContent->GetDataflowAsset();
		const TSharedPtr<Dataflow::FEngineContext>& DataflowContext = DataflowContent->GetDataflowContext();
		if(DataflowAsset && DataflowContext)
		{
			for (TObjectPtr<const UDataflowEdNode> Target : DataflowAsset->GetRenderTargets())
			{
				if (Target)
				{
					TSharedPtr<FManagedArrayCollection> RenderCollection(new FManagedArrayCollection);
					GeometryCollection::Facades::FRenderingFacade Facade(*RenderCollection);
					Facade.DefineSchema();

					Target->Render(Facade, DataflowContext);

					int32 NumGeometry = Facade.NumGeometry();
					for (int32 MeshIndex = 0; MeshIndex < NumGeometry; MeshIndex++)
					{
						FDynamicMesh3 DynamicMesh;
						Dataflow::Conversion::RenderingFacadeToDynamicMesh(Facade, MeshIndex, DynamicMesh);

						if (DynamicMesh.VertexCount())
						{
							if (Target == DataflowContent->GetPrimarySelectedNode())
							{
								DataflowContent->SetPrimaryRenderCollection(RenderCollection);
							}

							AddDynamicMeshComponent({Target, MeshIndex }, MoveTemp(DynamicMesh), {});
						}
					}
				}
			}
		}
	}
}

void FDataflowConstructionScene::ResetDynamicMeshComponents()
{
	USelection* SelectedComponents = DataflowModeManager->GetSelectedComponents();
	for (FRenderElement RenderElement : DynamicMeshComponents)
	{
		TObjectPtr<UDynamicMeshComponent>& DynamicMeshComponent = RenderElement.Value;

		DynamicMeshComponent->SelectionOverrideDelegate.Unbind();
		if (SelectedComponents->IsSelected(DynamicMeshComponent))
		{
			SelectedComponents->Deselect(DynamicMeshComponent);
			DynamicMeshComponent->PushSelectionToProxy();
		}
		RemoveComponent(DynamicMeshComponent);
	}
	DynamicMeshComponents.Reset();
}

TObjectPtr<UDynamicMeshComponent>& FDataflowConstructionScene::AddDynamicMeshComponent(FDataflowRenderKey InKey, UE::Geometry::FDynamicMesh3&& DynamicMesh, const TArray<UMaterialInterface*>& MaterialSet)
{
	TObjectPtr<UDataflowEditorCollectionComponent> DynamicMeshComponent = NewObject<UDataflowEditorCollectionComponent>(RootSceneActor);
	DynamicMeshComponent->MeshIndex = InKey.Value;
	DynamicMeshComponent->Node = InKey.Key;;
	DynamicMeshComponent->SetMesh(MoveTemp(DynamicMesh));
	
	// @todo(Material) This is just to have a material, we should transfer the materials from the assets if they have them. 
	TObjectPtr<UDataflowBaseContent> DataflowContent = GetDataflowContent();
	if (DataflowContent && DataflowContent->GetDataflowAsset() && DataflowContent->GetDataflowAsset()->Material)
	{
		DynamicMeshComponent->ConfigureMaterialSet({ DataflowContent->GetDataflowAsset()->Material });
	}
	else
	{
		DynamicMeshComponent->SetOverrideRenderMaterial(FDataflowEditorStyle::Get().VertexMaterial);
		DynamicMeshComponent->SetShadowsEnabled(false);
	}
	//else if (FDataflowEditorStyle::Get().DefaultMaterial)
	//{
	//	DynamicMeshComponent->ConfigureMaterialSet({ FDataflowEditorStyle::Get().DefaultMaterial });
	//}
	//else
	//{
	//	DynamicMeshComponent->ValidateMaterialSlots(true, false);
	//}

	DynamicMeshComponent->SelectionOverrideDelegate = UPrimitiveComponent::FSelectionOverride::CreateRaw(this, &FDataflowPreviewSceneBase::IsComponentSelected);
	DynamicMeshComponent->UpdateBounds();

	AddComponent(DynamicMeshComponent, DynamicMeshComponent->GetRelativeTransform());	
	DynamicMeshComponents.Emplace(InKey, DynamicMeshComponent);
	return DynamicMeshComponents[InKey];
}

void FDataflowConstructionScene::AddWireframeMeshElementsVisualizer()
{
	if(!bDataflowShowWireframeInConstructionView) return;

	ensure(WireframeElements.Num()==0);
	for(FRenderElement Elem : DynamicMeshComponents)
	{
		if( TObjectPtr<UDataflowEditorCollectionComponent> DynamicMeshComponent = Cast<UDataflowEditorCollectionComponent>(Elem.Value) )
		{
			// Set up the wireframe display of the rest space mesh.

			TObjectPtr<UMeshElementsVisualizer> WireframeDraw = NewObject<UMeshElementsVisualizer>(RootSceneActor);
			WireframeElements.Add(DynamicMeshComponent, WireframeDraw);

			WireframeDraw->CreateInWorld(GetWorld(), FTransform::Identity);
			WireframeDraw->Settings->DepthBias = 2.0;
			WireframeDraw->Settings->bAdjustDepthBiasUsingMeshSize = false;
			WireframeDraw->Settings->bShowWireframe = true;
			WireframeDraw->Settings->bShowBorders = true;
			WireframeDraw->Settings->bShowUVSeams = false;
			WireframeDraw->WireframeComponent->BoundaryEdgeThickness = 2;
			DynamicMeshComponent->WireframeComponent = WireframeDraw->WireframeComponent;

			WireframeDraw->SetMeshAccessFunction([DynamicMeshComponent](UMeshElementsVisualizer::ProcessDynamicMeshFunc ProcessFunc)
				{
					ProcessFunc(*DynamicMeshComponent->GetMesh());
				});

			for (FRenderElement RenderElement : DynamicMeshComponents)
			{
				RenderElement.Value->OnMeshChanged.Add(FSimpleMulticastDelegate::FDelegate::CreateLambda([WireframeDraw, this]()
					{
						WireframeDraw->NotifyMeshChanged();
					}));

				const bool bRestSpaceMeshVisible = RenderElement.Value->GetVisibleFlag();
				WireframeDraw->Settings->bVisible = bRestSpaceMeshVisible && bConstructionViewWireframe;
			}
			PropertyObjectsToTick.Add(WireframeDraw->Settings);
		}
	}
}

void FDataflowConstructionScene::ResetWireframeMeshElementsVisualizer()
{
	for (FRenderWireElement Elem : WireframeElements)
	{
		Elem.Value->Disconnect();
	}
	WireframeElements.Empty();
}

void FDataflowConstructionScene::UpdateWireframeMeshElementsVisualizer()
{
	ResetWireframeMeshElementsVisualizer();
	AddWireframeMeshElementsVisualizer();
}

bool FDataflowConstructionScene::HasRenderableGeometry()
{
	for (FRenderElement RenderElement : DynamicMeshComponents)
	{
		if (RenderElement.Value->GetMesh()->TriangleCount() > 0)
		{
			return true;
		}
	}
	return false;
}

void FDataflowConstructionScene::ResetConstructionScene()
{
	// Some objects, like the UMeshElementsVisualizer and Settings Objects
	// are not part of a tool, so they won't get ticked.This member holds
	// ticked objects that get rebuilt on Update
	PropertyObjectsToTick.Empty();

	ResetWireframeMeshElementsVisualizer();

	ResetDynamicMeshComponents();
}

void FDataflowConstructionScene::UpdateConstructionScene()
{
	ResetConstructionScene();

	// The preview scene for the construction view will be
	// cleared and rebuilt from scratch. This will genrate a 
	// list of UPrimitiveComponents for rendering.
	UpdateDynamicMeshComponents();
	
	// Attach a wireframe renderer to the DynamicMeshComponents
	UpdateWireframeMeshElementsVisualizer();

	if (TObjectPtr<UDataflowBaseContent> DataflowContent = GetDataflowContent())
	{
		DataflowContent->SetIsDirty(false);
	}
}

#undef LOCTEXT_NAMESPACE

