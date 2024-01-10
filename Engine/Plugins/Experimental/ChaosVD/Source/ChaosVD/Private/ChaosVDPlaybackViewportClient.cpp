// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDPlaybackViewportClient.h"

#include "ChaosVDEditorSettings.h"
#include "ChaosVDModule.h"
#include "ChaosVDParticleActor.h"
#include "ChaosVDPlaybackController.h"
#include "ChaosVDScene.h"
#include "ChaosVDSkySphereInterface.h"
#include "ComponentVisualizer.h"
#include "EditorModeManager.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "SEditorViewport.h"
#include "Selection.h"
#include "UnrealWidget.h"
#include "Actors/ChaosVDSolverInfoActor.h"
#include "Components/ChaosVDSolverCollisionDataComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Visualizers/ChaosVDDebugDrawUtils.h"
#include "Widgets/SChaosVDMainTab.h"

FChaosVDPlaybackViewportClient::FChaosVDPlaybackViewportClient(const TSharedPtr<FEditorModeTools>& InModeTools) : FEditorViewportClient(InModeTools.Get()), CVDWorld(nullptr)
{
	Widget->SetUsesEditorModeTools(InModeTools.Get());

	if (GEngine)
	{
		GEngine->OnActorMoving().AddRaw(this, &FChaosVDPlaybackViewportClient::HandleActorMoving);
	}

	if (UChaosVDEditorSettings* Settings = GetMutableDefault<UChaosVDEditorSettings>())
	{
		Settings->OnFarClippingOverrideChanged().AddRaw(this, &FChaosVDPlaybackViewportClient::HandleViewportSettingsChanged);
		Settings->OnVisibilitySettingsChanged().AddRaw(this, &FChaosVDPlaybackViewportClient::HandleViewportSettingsChanged);

		HandleViewportSettingsChanged(Settings);
	}
}

FChaosVDPlaybackViewportClient::~FChaosVDPlaybackViewportClient()
{
	if (ObjectFocusedDelegateHandle.IsValid())
	{
		if (TSharedPtr<FChaosVDScene> ScenePtr = CVDScene.Pin())
		{
			ScenePtr->OnObjectFocused().Remove(ObjectFocusedDelegateHandle);
		}
	}

	if (GEngine)
	{
		GEngine->OnActorMoving().RemoveAll(this);
	}
	
	if (UChaosVDEditorSettings* Settings = GetMutableDefault<UChaosVDEditorSettings>())
	{
		Settings->OnFarClippingOverrideChanged().RemoveAll(this);
		Settings->OnVisibilitySettingsChanged().RemoveAll(this);
	}
}

void FChaosVDPlaybackViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	FEditorViewportClient::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);

	if (HitProxy == nullptr)
	{
		return;
	}
	
	const TSharedPtr<SChaosVDMainTab> MainTabToolkitHost = ModeTools.IsValid() ? StaticCastSharedPtr<SChaosVDMainTab>(ModeTools->GetToolkitHost()) : nullptr;
	if (!MainTabToolkitHost.IsValid())
	{
		return;
	}

	const FViewportClick Click(&View, this, Key, Event, HitX, HitY);

	if (TSharedPtr<FChaosVDScene> ScenePtr = CVDScene.Pin())
	{
		bool bClickHandled = false;

		// TODO: Iterate trough all registered visualizers
		HComponentVisProxy* ComponentVisProxy = HitProxyCast<HComponentVisProxy>(HitProxy);
		if (const TSharedPtr<FComponentVisualizer> Visualizer = MainTabToolkitHost->FindComponentVisualizer(UChaosVDSolverCollisionDataComponent::StaticClass()))
		{
			// Not sure if this is compliant with the normal use of the component visualizers,
			// but passing a null hitproxy when the hit proxy was not a component
			// It allow us to handle things like clear selection on the Collision Data Visualizer
			bClickHandled = Visualizer->VisProxyHandleClick(this, ComponentVisProxy, Click);
		}

		const IChaosVDGeometryComponent* AsCVDGeometryComponent = nullptr;
		int32 MeshInstanceIndex = INDEX_NONE;

		if (const HInstancedStaticMeshInstance* InstancedStaticMeshProxy = HitProxyCast<HInstancedStaticMeshInstance>(HitProxy))
		{
			AsCVDGeometryComponent = Cast<IChaosVDGeometryComponent>(InstancedStaticMeshProxy->Component);
			MeshInstanceIndex = InstancedStaticMeshProxy->InstanceIndex;
		}
		else if (const HActor* ActorHitProxy = HitProxyCast<HActor>(HitProxy))
		{
			AsCVDGeometryComponent = Cast<IChaosVDGeometryComponent>(ActorHitProxy->PrimComponent.Get());
			MeshInstanceIndex = 0;
		}

		if (AsCVDGeometryComponent && MeshInstanceIndex != INDEX_NONE)
		{
			if (const TSharedPtr<FChaosVDMeshDataInstanceHandle> MeshDataHandle = AsCVDGeometryComponent->GetMeshDataInstanceHandle(MeshInstanceIndex))
			{
				if (AChaosVDParticleActor* ClickedActor = ScenePtr->GetParticleActor(MeshDataHandle->GetOwningSolverID(), MeshDataHandle->GetOwningParticleID()))
				{
					ScenePtr->SetSelectedObject(ClickedActor);
					bClickHandled = true;
				}
			}
		}

		if (bClickHandled)
		{
			return;
		}

		if (HitProxy->IsA(HActor::StaticGetType()))
		{
			const HActor* ActorHitProxy = static_cast<HActor*>(HitProxy);
			if (AActor* ClickedActor = ActorHitProxy->Actor)
			{
				ScenePtr->SetSelectedObject(ClickedActor);
			}
		}
	}
}

void FChaosVDPlaybackViewportClient::SetScene(TWeakPtr<FChaosVDScene> InScene)
{
	if (TSharedPtr<FChaosVDScene> ScenePtr = InScene.Pin())
	{
		CVDWorld = ScenePtr->GetUnderlyingWorld();
		CVDScene = InScene;

		ObjectFocusedDelegateHandle = ScenePtr->OnObjectFocused().AddRaw(this, &FChaosVDPlaybackViewportClient::HandleObjectFocused);
	}
}

void FChaosVDPlaybackViewportClient::HandleObjectFocused(UObject* FocusedObject)
{
	if (AActor* FocusedActor = Cast<AActor>(FocusedObject))
	{
		FocusViewportOnBox(FocusedActor->GetComponentsBoundingBox(false));
	}
}

void FChaosVDPlaybackViewportClient::HandleActorMoving(AActor* MovedActor) const
{
	if (Cast<ADirectionalLight>(MovedActor))
	{
		if (const TSharedPtr<FChaosVDScene> SceneSharedPtr = CVDScene.Pin())
		{
			if (SceneSharedPtr->GetSkySphereActor()->Implements<UChaosVDSkySphereInterface>())
			{
				IChaosVDSkySphereInterface::Execute_Refresh(SceneSharedPtr->GetSkySphereActor());
			}
		}
	}
}

void FChaosVDPlaybackViewportClient::HandleViewportSettingsChanged(UChaosVDEditorSettings* SettingsObject)
{
	if (SettingsObject)
	{
		OverrideFarClipPlane(SettingsObject->FarClippingOverride);
		EngineShowFlags.SetMeshEdges(EnumHasAnyFlags(static_cast<EChaosVDGeometryVisibilityFlags>(SettingsObject->GeometryVisibilityFlags), EChaosVDGeometryVisibilityFlags::ShowTriangleEdges));
		Invalidate();
	}
}

void FChaosVDPlaybackViewportClient::TrackActor(AActor* ActorToTrack, EChaosVDActorTrackingMode TrackingMode)
{
	if (!ActorToTrack)
	{
		return;
	}
	
	if (const UChaosVDEditorSettings* CVDEditorSettings = GetDefault<UChaosVDEditorSettings>())
	{
		switch(TrackingMode)
		{
			case EChaosVDActorTrackingMode::ByBoundingBox:
				{
					FBox ActorBounds = ActorToTrack->GetComponentsBoundingBox(false);
					FocusViewportOnBox(ActorBounds.ExpandBy(CVDEditorSettings->ExpandViewTrackingBy), true);
					break;
				}
			case EChaosVDActorTrackingMode::ByDistanceOffset:
			case EChaosVDActorTrackingMode::MatchTransform:
				{
					TrackTransform(ActorToTrack->GetActorTransform(), TrackingMode);
					break;
				}
			default:
				{
					ensureMsgf(false, TEXT("Actor tracking requested with invalid options. The actor will not be tracked"));
					break;
				}
		}	
	}
}

void FChaosVDPlaybackViewportClient::TrackTransform(const FTransform& TransformToTrack, EChaosVDActorTrackingMode TrackingMode)
{
	if (const UChaosVDEditorSettings* CVDEditorSettings = GetDefault<UChaosVDEditorSettings>())
	{
		switch(TrackingMode)
		{
		case EChaosVDActorTrackingMode::ByBoundingBox:
			{
				ensureMsgf(false, TEXT("Tracking by Bounding box only supported with Actors"));
				break;
			}
		case EChaosVDActorTrackingMode::ByDistanceOffset:
			{
				FViewportCameraTransform& ViewTransform = GetViewTransform();
				FVector ActorLocation = TransformToTrack.GetLocation();
					
				constexpr bool bEnable = false;
				ToggleOrbitCamera(bEnable);

				FVector ActorToCamDir = ViewTransform.GetLocation() - ActorLocation;
				ActorToCamDir.Normalize();

				FVector TargetLocation = ActorLocation + ActorToCamDir * CVDEditorSettings->TrackingDistanceOffset;

				ViewTransform.SetRotation((-ActorToCamDir).Rotation());
				ViewTransform.SetLookAt(ActorLocation);
				ViewTransform.TransitionToLocation(TargetLocation, EditorViewportWidget, true);
					
				// Tell the viewport to redraw itself.
				Invalidate();
				break;
			}
		case EChaosVDActorTrackingMode::MatchTransform:
			{
				constexpr bool bEnable = false;
				ToggleOrbitCamera(bEnable);
					
				FViewportCameraTransform& ViewTransform = GetViewTransform();
				ViewTransform.SetRotation(TransformToTrack.GetRotation().Rotator());

				ViewTransform.SetLookAt(TransformToTrack.GetLocation());
				ViewTransform.TransitionToLocation(TransformToTrack.GetLocation(), EditorViewportWidget, true);

				Invalidate();
				break;
			}		
		default:
			{
				ensureMsgf(false, TEXT("Actor tracking requested with invalid options. The actor will not be tracked"));
				break;
			}
		}	
	}
}

void FChaosVDPlaybackViewportClient::PerformSelectedTrackingForFrame(FChaosVDGameFrameData* FrameData)
{
	if (const TSharedPtr<FChaosVDScene> CVDSceneSharedPtr = CVDScene.Pin())
	{
		if (const UChaosVDEditorSettings* CVDEditorSettings = GetDefault<UChaosVDEditorSettings>())
		{
			switch (CVDEditorSettings->TrackingTarget)
			{
			case EChaosVDActorTrackingTarget::SelectedObject:
				{
					if (ModeTools.IsValid())
					{
						USelection* CurrentSelection = ModeTools->GetSelectedActors();
		
						//TODO: Update this if we add multi selection support
						if (AActor* SelectedActor = CurrentSelection ? CurrentSelection->GetTop<AActor>() : nullptr)
						{
							TrackActor(SelectedActor, CVDEditorSettings->TrackingOptions);
						}
					}
					break;
				}
			case EChaosVDActorTrackingTarget::RecordedTransform:
				{
					// TODO: Find a better place to store the current selected Transform name, it should not be the editor settings object
					if (const TSharedPtr<FName>& TransformName = CVDEditorSettings->SelectedTrackedTransformName)
					{
						if (const FChaosVDTrackedTransform* TrackedTransform = FrameData->RecordedNonSolverTransformsByID.Find(*TransformName))
						{
							TrackTransform(TrackedTransform->Transform, CVDEditorSettings->TrackingOptions);
						}
					}
								
					break;
				}
			case EChaosVDActorTrackingTarget::RecordedLocation:
				{
					// TODO: Find a better place to store the current selected Location name, it should not be the editor settings object
					if (const TSharedPtr<FName>& LocationName = CVDEditorSettings->SelectedTrackedLocationName)
					{
						if (const FChaosVDTrackedLocation* TrackedTransform = FrameData->RecordedNonSolverLocationsByID.Find(*LocationName))
						{
							FTransform LocationTransform;
							LocationTransform.SetLocation(TrackedTransform->Location);
							TrackTransform(LocationTransform, CVDEditorSettings->TrackingOptions);
						}
					}
								
					break;
				}
			default:
				break;
			}			
		}
	}
}

void FChaosVDPlaybackViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const TSharedPtr<SChaosVDMainTab> MainTabToolkitHost = ModeTools.IsValid() ? StaticCastSharedPtr<SChaosVDMainTab>(ModeTools->GetToolkitHost()) : nullptr;
	if (!MainTabToolkitHost.IsValid())
	{
		return;
	}

	if (TSharedPtr<FChaosVDScene> ScenePtr = CVDScene.Pin())
	{
	
		for (const TPair<int32, AChaosVDSolverInfoActor*>& SolverInfoWithID : ScenePtr->GetSolverInfoActorsMap())
		{
			UChaosVDSolverCollisionDataComponent* CollisionDataComponent = SolverInfoWithID.Value ? SolverInfoWithID.Value->GetCollisionDataComponent() : nullptr;
			if (CollisionDataComponent)
			{
				if (TSharedPtr<FComponentVisualizer> Visualizer = MainTabToolkitHost->FindComponentVisualizer(CollisionDataComponent->StaticClass()))
				{
					Visualizer->DrawVisualization(CollisionDataComponent, View, PDI);
				}
			}
		}
		
		TArray<AActor*> SelectedActors = ScenePtr->GetElementSelectionSet()->GetSelectedObjects<AActor>();

		for (AActor* SelectedActor : SelectedActors)
		{
			if (IChaosVDVisualizerContainerInterface* VisualizerContainer = Cast<IChaosVDVisualizerContainerInterface>(SelectedActor))
			{
				VisualizerContainer->DrawVisualization(View, PDI);
			}
		}
	}

	FEditorViewportClient::Draw(View, PDI);
}

void FChaosVDPlaybackViewportClient::DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas)
{
	FEditorViewportClient::DrawCanvas(InViewport, View, Canvas);

	FChaosVDDebugDrawUtils::DrawCanvas(InViewport, View, Canvas);
}
