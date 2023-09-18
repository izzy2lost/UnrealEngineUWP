// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "OpenCVHelper.h"

#include "CameraCalibrationUtilsPrivate.generated.h"

class UCalibrationPointComponent;

/** Structure representing an aruco marker, including ID, Name, and corner coordinates in 2D image space and 3D world space */
USTRUCT()
struct FArucoCalibrationPoint
{
	GENERATED_BODY()

	/** 3D locations (in world space) of each corner */
	UPROPERTY()
	FVector Corners3D[4];

	/** 2D locations (in pixels) of each corner */
	UPROPERTY()
	FVector2f Corners2D[4];

	/** Aruco Marker ID */
	UPROPERTY()
	int32 MarkerID = -1;
	
	/** Name of the marker (follows the naming convention of "[DictionaryName]-[MarkerID]") */
	UPROPERTY()
	FString Name;

	/** 
	 * Object (almost certainly a CalibrationPointComponent) that owns this aruco point. 
	 * Used to sort detected points that belong to different patterns in the same image 
	 */
	TObjectPtr<UObject> Owner = nullptr;
};

namespace UE::CameraCalibration::Private
{
	/** Returns the aruco dictionary that matches the calibration point component(s) of the input calibrator actor */
	EArucoDictionary GetArucoDictionaryForCalibrator(AActor* CalibratorActor);

	/** Get the aruco dictionary from a string representation matching the dictionary name */
	EArucoDictionary GetArucoDictionaryFromName(FString Name);

	/** Get the string representation of the input aruco dictionary */
	FString GetArucoDictionaryName(EArucoDictionary Dictionary);

	/** Find all actors in the current world that have a calibration point component attached (excluding CDOs and actors in levels that are not currently visible) */
	void FindActorsWithCalibrationComponents(TArray<AActor*>& ActorsWithCalibrationComponents);

	/** Find an aruco marker calibration point in one of the input calibration components that matches the input dictionary and marker ID */
	bool FindArucoCalibrationPoint(const TArray<UCalibrationPointComponent*>& CalibrationComponents, EArucoDictionary ArucoDictionary, const FArucoMarker& ArucoMarker, FArucoCalibrationPoint& OutArucoCalibrationPoint);

	/** Sort input calibration points based on owner */
	void SortArucoCalibrationPoints(const TArray<FArucoCalibrationPoint>& ArucoCalibrationPoints, TMap<UObject*, TArray<FArucoCalibrationPoint>>& OutSortedArucoCalibrationPointMap);

	/** Set every pixel in the input texture to the clear color */
	void ClearTexture(UTexture2D* Texture, FColor ClearColor = FColor::Transparent);

	/** Set the texture data to the input array of pixels */
	void SetTextureData(UTexture2D* Texture, const TArray<FColor>& PixelData);
}
