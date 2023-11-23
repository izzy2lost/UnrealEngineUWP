// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "LevelEditorViewport.h"

class UChaosVDEditorSettings;
struct FChaosVDGameFrameData;
class FChaosVDScene;
enum class EChaosVDActorTrackingMode;

/** Client viewport class used for to handle a Chaos Visual Debugger world Interaction/Rendering.
 * It re-routes interaction events to our Chaos VD scene
 */
class FChaosVDPlaybackViewportClient : public FEditorViewportClient
{
public:

	FChaosVDPlaybackViewportClient(const TSharedPtr<FEditorModeTools>& InModeTools);
	virtual ~FChaosVDPlaybackViewportClient() override;

	virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;

	void SetScene(TWeakPtr<FChaosVDScene> InScene);

	virtual UWorld* GetWorld() const override { return CVDWorld; };

	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas) override;
	
	void TrackActor(AActor* ActorToTrack, EChaosVDActorTrackingMode TrackingMode);
	void TrackTransform(const FTransform& TransformToTrack, EChaosVDActorTrackingMode TrackingMode);
	
	void PerformSelectedTrackingForFrame(FChaosVDGameFrameData* FrameData);

private:
	void HandleObjectFocused(UObject* FocusedObject);
	void HandleActorMoving(AActor* MovedActor) const;
	void HandleFarClippingOverrideSettingsChanged(UChaosVDEditorSettings* SettingsObject);
	
	FDelegateHandle ObjectFocusedDelegateHandle;
	UWorld* CVDWorld;
	TWeakPtr<FChaosVDScene> CVDScene;
};
