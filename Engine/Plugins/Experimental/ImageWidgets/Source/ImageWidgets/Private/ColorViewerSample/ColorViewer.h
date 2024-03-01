// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if IMAGE_WIDGETS_BUILD_COLOR_VIEWER_SAMPLE

#include <IImageViewer.h>

namespace UE::ImageWidgets::Sample
{
	/**
	 * Provide tone mapping capabilities.
	 * In this simple example, this is limited to just normal RGB plus luminance (grayscale).
	 */
	struct FToneMapping
	{
		enum class EMode { RGB, Lum };

		FToneMapping(EMode Mode)
			: Mode(Mode)
		{
		}

		FLinearColor GetToneMappedColor(const FLinearColor& Color) const
		{
			if (Mode == EMode::Lum)
			{
				const float Luminance = 0.3f * Color.R + 0.59f * Color.G + 0.11f * Color.B;
				return FLinearColor(Luminance, Luminance, Luminance);
			}
			return Color;
		}

		EMode Mode;
	};

	/**
	 * Image viewer implementation used by the image widgets.
	 * It contains any image data, in this case just colors, and renders the image data in the viewport widgets based on the viewport provided parameters.
	 */
	class FColorViewer final : public IImageViewer
	{
	public:
		// IImageViewer overrides - begin
		virtual FImageInfo GetCurrentImageInfo() const override;
		virtual void DrawCurrentImage(FViewport* Viewport, FCanvas* Canvas, const FDrawProperties& Properties) override;
		virtual TOptional<TVariant<FColor, FLinearColor>> GetCurrentImagePixelColor(FIntPoint PixelCoords, int32 MipLevel) const override;
		// IImageViewer overrides - end

		/** Sets a random color as the "image". This is a simple proxy for the image content changing and/or users choosing different images to display. */
		void RandomizeColor();

		/** Access to tone mapping data. This is effectively used by the viewport toolbar extensions as well as when drawing the image. */
		FToneMapping::EMode GetToneMapping() const;
		void SetToneMapping(FToneMapping::EMode Mode);

	private:
		/** The tone mapping data. */
		FToneMapping ToneMapping = FToneMapping::EMode::RGB;

		/** The color for the currently displayed image. */
		FLinearColor Color;

		/** This is set to true once the color is initialized by the user, and evaluated to tell the viewport if the image is valid. */
		bool bColorIsValid = false;

		/** Hardcoded values for the image size and the GUID of the single image this is currently able to display.
		 *  In a more realistic application, these value would depend on the actual images that are available. */
		inline static constexpr FGuid ImageGuid = FGuid(1, 0, 0, 0);
		inline static const FIntPoint ImageSize = 512;
	};
}

#endif // IMAGE_WIDGETS_BUILD_COLOR_VIEWER_SAMPLE
