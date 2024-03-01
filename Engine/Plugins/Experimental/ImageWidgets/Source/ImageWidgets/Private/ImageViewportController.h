// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SImageViewport.h"

namespace UE::ImageWidgets
{
	/**
	 * Camera controller for the 2D viewport supporting panning and zooming.
	 */
	class FImageViewportController
	{
	public:
		enum class EZoomMode
		{
			Fit = static_cast<int32>(SImageViewport::FControllerSettings::EDefaultZoomMode::Fit),
			Fill = static_cast<int32>(SImageViewport::FControllerSettings::EDefaultZoomMode::Fill),
			Custom
		};
		static_assert(EZoomMode::Fit != EZoomMode::Fill && EZoomMode::Fit != EZoomMode::Custom && EZoomMode::Fill != EZoomMode::Custom);

		struct FZoomSettings
		{
			EZoomMode Mode;
			double Zoom;
		};

		explicit FImageViewportController(EZoomMode DefaultZoomMode);

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
		EZoomMode DefaultZoomMode;
	};
}
