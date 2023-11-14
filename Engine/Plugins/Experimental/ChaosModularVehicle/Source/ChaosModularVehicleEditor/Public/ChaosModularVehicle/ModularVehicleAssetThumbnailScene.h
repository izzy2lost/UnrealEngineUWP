// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThumbnailHelpers.h"

class AModularVehiclePawn;
class UModularVehicleAsset;

class FModularVehicleAssetThumbnailScene : public FThumbnailPreviewScene
{
public:
	/** Constructor */
	FModularVehicleAssetThumbnailScene();

	/** Sets the geometry collection to use in the next CreateView() */
	void SetModularVehicleAsset(UModularVehicleAsset* ModularVehicleAsset);

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;

private:
	/** The actor used to display all geometry collection thumbnails */
	AModularVehiclePawn* PreviewActor;
};
