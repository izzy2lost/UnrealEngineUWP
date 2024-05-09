// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorViewportClient.h"
#include "ImageViewportController.h"
#include "IImageViewer.h"
#include "SImageViewport.h"

namespace UE::ImageWidgets
{
	class FABComparison;

	DECLARE_DELEGATE_RetVal(FIntPoint, FGetImageSize)
	DECLARE_DELEGATE_ThreeParams(FDrawImage, FViewport*, FCanvas*, const IImageViewer::FDrawProperties&)
	DECLARE_DELEGATE_RetVal(SImageViewport::FDrawSettings, FGetDrawSettings)
	DECLARE_DELEGATE_RetVal(float, FGetDPIScaleFactor)
	DECLARE_DELEGATE(FOnLeftMouseButtonPressed);
	DECLARE_DELEGATE(FOnLeftMouseButtonReleased);

	/**
	 * Viewport client for controlling the camera and drawing viewport contents. 
	 */
	class FImageViewportClient : public FEditorViewportClient
	{
	public:
		FImageViewportClient(const TWeakPtr<SEditorViewport>& InViewport, FGetImageSize&& InGetImageSize, FDrawImage&& InDrawImage,
		                     FGetDrawSettings&& InGetDrawSettings, FGetDPIScaleFactor&& InGetDPIScaleFactor,
		                     FOnLeftMouseButtonPressed&& InOnLeftMouseButtonPressed, FOnLeftMouseButtonReleased&& InOnLeftMouseButtonReleased,
		                     SImageViewport::FControllerSettings::EDefaultZoomMode DefaultZoomMode, EMouseCaptureMode InMouseCaptureMode = EMouseCaptureMode::CapturePermanently);
		virtual ~FImageViewportClient() override;

		virtual void Draw(FViewport* InViewport, FCanvas* Canvas) override;

		// SEditorViewport overrides - begin
		virtual EMouseCursor::Type GetCursor(FViewport* InViewport, int32 X, int32 Y) override;
		virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
		virtual void TrackingStarted(const FInputEventState& InputState, bool bIsDraggingWidget, bool bNudge) override;
		virtual void TrackingStopped() override;
		virtual EMouseCaptureMode GetMouseCaptureMode() const override;
		// SEditorViewport overrides - end

		int32 GetMipLevel() const;
		void SetMipLevel(int32 InMipLevel);
		FImageViewportController::FZoomSettings GetZoom() const;
		void SetZoom(FImageViewportController::EZoomMode Mode, double Zoom);
		void ResetController(FIntPoint ImageSize);
		void ResetZoom(FIntPoint ImageSize);

		TPair<bool, FVector2d> GetPixelCoordinatesUnderCursor() const;

	private:
		struct FCheckerTextureSettings
		{
			bool operator==(const FCheckerTextureSettings&) const = default;

			bool bEnabled = false;
			FLinearColor Color1;
			FLinearColor Color2;
			uint32 CheckerSize = 0;
		};

		FVector2d GetCurrentDragWithDPIScaling() const;
		IImageViewer::FDrawProperties::FPlacement GetPlacementProperties(FIntPoint ImageSize, FVector2d ViewportSizeWithDPIScaling) const;
		IImageViewer::FDrawProperties::FMip GetMipProperties() const;

		void CreateOrDestroyCheckerTextureIfSettingsChanged(const SImageViewport::FDrawSettings& DrawSettings);

		FVector2d GetViewportSizeWithDPIScaling() const;
		
		FGetImageSize GetImageSize;
		FDrawImage DrawImage;
		FGetDrawSettings GetDrawSettings;
		FGetDPIScaleFactor GetDPIScaleFactor;
		FOnLeftMouseButtonPressed OnLeftMouseButtonPressed;
		FOnLeftMouseButtonReleased OnLeftMouseButtonReleased;
		
		bool bDragging = false;
		FIntPoint DraggingStart;

		bool bMipSelected = true;
		int32 MipLevel = -1;

		IImageViewer::FDrawProperties::FPlacement CachedPlacement;
		bool bCachedPlacementIsValid = false;

		FImageViewportController Controller;

		TStrongObjectPtr<UTexture2D> CheckerTexture;
		FCheckerTextureSettings CachedCheckerTextureSettings;

		EMouseCaptureMode MouseCaptureMode;
	};
}
