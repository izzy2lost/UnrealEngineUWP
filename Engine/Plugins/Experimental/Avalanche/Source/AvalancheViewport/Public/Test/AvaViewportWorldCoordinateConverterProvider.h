// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/MathFwd.h"
#include "Math/Transform.h"
#include "Math/Vector2D.h"
#include "ViewportClient/IAvaViewportWorldCoordinateConverter.h"

/**
 * Test classes that can be passed to the alignment utils.
 * Instantiate with MakeShared<FAvaViewportWorldCoordinateConverterProviderPerspective>(...)
 *   or MakeShared<FAvaViewportWorldCoordinateConverterProviderOrthographic>(...)
 */

struct AVALANCHEVIEWPORT_API FAvaViewportWorldCoordinateConverterProvider : public IAvaViewportWorldCoordinateConverter
{
	//~ Begin IAvaViewportWorldCoordinateConverter
	virtual FVector2f GetViewportSize() const override;
	virtual FTransform GetViewportViewTransform() const override { return ViewTransform; }
	//~End IAvaViewportWorldCoordinateConverter

protected:
	FTransform ViewTransform;
	FVector2f ViewportSize;

	FAvaViewportWorldCoordinateConverterProvider(const FTransform& InViewTransform, const FVector2f& InViewportSize);
};

struct AVALANCHEVIEWPORT_API FAvaViewportWorldCoordinateConverterProviderPerspective : public FAvaViewportWorldCoordinateConverterProvider
{
	FAvaViewportWorldCoordinateConverterProviderPerspective(FVector InLocation, FRotator InRotation, FVector2f InViewportSize, 
		float InFieldOfView);

	//~ Begin IAvaViewportWorldCoordinateConverter
	virtual FVector2D GetFrustumSizeAtDistance(double InDistance) const override;
	virtual FVector ViewportPositionToWorldPosition(const FVector2f& InViewportPosition, double InDistance) const override;
	virtual void WorldPositionToViewportPosition(const FVector& InWorldPosition, FVector2f& OutViewportPosition, double& OutDistance) const override;
	//~End IAvaViewportWorldCoordinateConverter

protected:
	float FieldOfView;
};

struct AVALANCHEVIEWPORT_API FAvaViewportWorldCoordinateConverterProviderOrthographic : public FAvaViewportWorldCoordinateConverterProvider
{
	FAvaViewportWorldCoordinateConverterProviderOrthographic(FVector InLocation, FRotator InRotation, FVector2f InViewportSize, 
		float InOrthographicWidth);

	//~ Begin IAvaViewportWorldCoordinateConverter
	virtual FVector2D GetFrustumSizeAtDistance(double InDistance) const override;
	virtual FVector ViewportPositionToWorldPosition(const FVector2f& InViewportPosition, double InDistance) const override;
	virtual void WorldPositionToViewportPosition(const FVector& InWorldPosition, FVector2f& OutViewportPosition, double& OutDistance) const override;
	//~End IAvaViewportWorldCoordinateConverter

protected:
	float OrthographicWidth;
};
