// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/MathFwd.h"

class PIXELSTREAMING2HMD_API IPixelStreaming2HMD
{
public:
	/**
	 * @brief Set the transform for the HMD.
	 *
	 */
	virtual void SetTransform(FTransform Transform) = 0;

	/**
	 * @brief Set the eye views, including the eye positions and their respective projection matrices
	 *
	 */
	virtual void SetEyeViews(FTransform Left, FMatrix LeftProj, FTransform Right, FMatrix RightProj, FTransform HMD) = 0;
};
