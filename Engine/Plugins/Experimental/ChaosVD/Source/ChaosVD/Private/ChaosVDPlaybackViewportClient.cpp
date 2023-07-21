// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDPlaybackViewportClient.h"

#include "CameraController.h"
#include "ChaosVDEditorSettings.h"
#include "ChaosVDParticleActor.h"
#include "ChaosVDScene.h"
#include "EngineUtils.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Visualizers/ChaosVDDebugDrawUtils.h"
#include "SEditorViewport.h"


FChaosVDPlaybackViewportClient::FChaosVDPlaybackViewportClient() : FEditorViewportClient(nullptr), CVDWorld(nullptr)
{
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
}

void FChaosVDPlaybackViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	if (HitProxy == nullptr)
	{
		return;
	}

	if (TSharedPtr<FChaosVDScene> ScenePtr = CVDScene.Pin())
	{
		if (HitProxy->IsA(HActor::StaticGetType()))
		{
			HActor* ActorHitProxy = static_cast<HActor*>(HitProxy);
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

void FChaosVDPlaybackViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);

	if (TSharedPtr<FChaosVDScene> ScenePtr = CVDScene.Pin())
	{	
		TArray<AActor*> SelectedActors = ScenePtr->GetElementSelectionSet()->GetSelectedObjects<AActor>();

		for (AActor* SelectedActor : SelectedActors)
		{
			if (IChaosVDVisualizerContainerInterface* VisualizerContainer = Cast<IChaosVDVisualizerContainerInterface>(SelectedActor))
			{
				VisualizerContainer->DrawVisualization(View, PDI);
			}
		}
	}
}

void FChaosVDPlaybackViewportClient::DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas)
{
	FEditorViewportClient::DrawCanvas(InViewport, View, Canvas);

	FChaosVDDebugDrawUtils::DrawCanvas(InViewport, View, Canvas);
}
