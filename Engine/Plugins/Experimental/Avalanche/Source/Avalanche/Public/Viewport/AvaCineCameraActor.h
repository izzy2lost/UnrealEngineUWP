// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "AvaCineCameraActor.generated.h"

/** 
 * Motion Design Cine Camera Actor is derived from Cine Camera Actor.
 * Its function is to provide a Cine Camera which can be used right away inside Motion Design.
 * This is done by customizing some of its default values.
 * In particular, Motion Design Editor configuration property "Camera Distance" is used to initialize camera position and manual focus.
 * See: Editor Preferences --> Motion Design --> Editor Settings --> Camera Distance
 */
UCLASS(DisplayName = "Motion Design Cine Camera Actor")
class AVALANCHE_API AAvaCineCameraActor : public ACineCameraActor
{
	friend class UAvalancheEditorSettings;
	
	GENERATED_BODY()

public:
	AAvaCineCameraActor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
	{
		/* todo: when the reset to defaults issue will be solved, Configure(GetDefaultCameraDistance()) could be called here to to initialize defaults */
	}

	/**
	 * Initialize the camera with Avalanche scene default values: field of view, camera position, focus distance.
	 * @param InCameraDistance: camera distance value, used to initialize camera position and manual focus distance
	 */
	void Configure(float InCameraDistance)
	{
		// initialize default camera distance - todo: we might use this value later for a custom reset?
		DefaultCameraDistance = InCameraDistance;
		
		if (UCineCameraComponent* const CineCameraComp = GetCineCameraComponent())
		{
			CineCameraComp->SetFieldOfView(90.0f);

			// smaller camera mesh, todo: this will be substituted with a custom visualizer, or other solution, to visualize frustum + direction
			CineCameraComp->SetWorldScale3D(FVector(0.5f));

			// using default ManualFocusDistance value from Default CineCameraComponent, since that is handled by UAvalancheEditorSettings
			CineCameraComp->FocusSettings.ManualFocusDistance = DefaultCameraDistance;

			FVector CameraPosition = FVector::ZeroVector;
			CameraPosition.X = -DefaultCameraDistance;
			SetActorLocation(CameraPosition);
		}
	}

private:
	/**
	 * Updates some defaults of the Mutable CDO's CineCameraComponent, based on the provided CameraDistance value
	 */
	static void SetDefaultCameraDistance(float InCameraDistance)
	{
		AAvaCineCameraActor* const DefaultObject = GetMutableDefault<AAvaCineCameraActor>();

		FVector CameraPosition = FVector::ZeroVector;
		CameraPosition.X = -InCameraDistance;
		DefaultObject->SetActorLocation(CameraPosition);
		
		if (UCineCameraComponent* const DefaultCineCameraComponent = DefaultObject->GetCineCameraComponent())
		{
			DefaultCineCameraComponent->FocusSettings.ManualFocusDistance = InCameraDistance;
		}

		DefaultObject->DefaultCameraDistance = InCameraDistance;
	}

	static float GetDefaultCameraDistance()
	{
		return GetMutableDefault<AAvaCineCameraActor>()->DefaultCameraDistance;
	}

	/** The CameraDistance value used when configuring this AvaCineCamera*/
	UPROPERTY()
	float DefaultCameraDistance;
};
