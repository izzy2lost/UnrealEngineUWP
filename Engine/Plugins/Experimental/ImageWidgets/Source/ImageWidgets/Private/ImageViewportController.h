// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::ImageWidgets
{
	/**
	 * Camera controller for the 2D viewport supporting panning and zooming.
	 */
	class FImageViewportController
	{
	public:
		enum class EZoomMode { Custom, Fit, Fill };

		struct FZoomSettings
		{
			EZoomMode Mode;
			double Zoom;
		};

		FImageViewportController();

		void Pan(FVector2d Delta);
		void Reset(FIntPoint ImageSize, FIntPoint ViewportSize);
		void ZoomIn(FVector2d CursorPos, const FIntPoint& ImageSize);
		void ZoomOut(FVector2d CursorPos, const FIntPoint& ImageSize);

		FVector2d GetPan(FVector2d Drag) const;
		FZoomSettings GetZoom() const;
		void SetZoom(EZoomMode ZoomMode, double Zoom, const FIntPoint& ImageSize, const FIntPoint& ViewportSize);

	private:
		FVector2d PanAmount;
		FZoomSettings ZoomSettings;
	};
}
