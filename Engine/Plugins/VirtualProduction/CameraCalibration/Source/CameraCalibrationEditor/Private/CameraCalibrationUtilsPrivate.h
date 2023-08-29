// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "OpenCVHelper.h"

namespace UE::CameraCalibration::Private
{
	/** Returns the aruco dictionary that matches the calibration point component(s) of the input calibrator actor */
	EArucoDictionary GetArucoDictionaryForCalibrator(AActor* CalibratorActor);

	/** Get the aruco dictionary from a string representation matching the dictionary name */
	EArucoDictionary GetArucoDictionaryFromName(FString Name);

	/** Get the string representation of the input aruco dictionary */
	FString GetArucoDictionaryName(EArucoDictionary Dictionary);

	/** Set every pixel in the input texture to the clear color */
	void ClearTexture(UTexture2D* Texture, FColor ClearColor = FColor::Transparent);

	/** Set the texture data to the input array of pixels */
	void SetTextureData(UTexture2D* Texture, const TArray<FColor>& PixelData);
}
