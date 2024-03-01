// Copyright Epic Games, Inc. All Rights Reserved.

#if IMAGE_WIDGETS_BUILD_COLOR_VIEWER_SAMPLE

#include "ColorViewer.h"

#include <CanvasItem.h>
#include <CanvasTypes.h>

#define LOCTEXT_NAMESPACE "ColorViewer"

namespace UE::ImageWidgets::Sample
{
	IImageViewer::FImageInfo FColorViewer::GetCurrentImageInfo() const
	{
		return {ImageGuid, ImageSize, 0, bColorIsValid};
	}

	void FColorViewer::DrawCurrentImage(FViewport* Viewport, FCanvas* Canvas, const FDrawProperties& Properties)
	{
		// Get color value after tone mapping.
		const FLinearColor ToneMappedColor = ToneMapping.GetToneMappedColor(Color);

		// Draw simple quad with current tone mapped color.
		// In a less trivial use case, this would require rendering quads with textures and the like. 
		FCanvasTileItem Tile(Properties.Placement.Offset, Properties.Placement.Size, ToneMappedColor);
		Canvas->DrawItem(Tile);
	}

	TOptional<TVariant<FColor, FLinearColor>> FColorViewer::GetCurrentImagePixelColor(FIntPoint PixelCoords, int32 MipLevel) const
	{
		if (bColorIsValid)
		{
			// Returns the current color as float values.
			// In a less trivial use case, the pixel coordinates and potentially the MIP level would be needed to look up the color value.
			return TVariant<FColor, FLinearColor>(TInPlaceType<FLinearColor>(), Color);
		}
		return {};
	}

	void FColorViewer::RandomizeColor()
	{
		auto Random = []
		{
			return FMath::RandRange(0.0f, 1.0f);
		};

		Color = {Random(), Random(), Random()};
		bColorIsValid = true;
	}

	FToneMapping::EMode FColorViewer::GetToneMapping() const
	{
		return ToneMapping.Mode;
	}

	void FColorViewer::SetToneMapping(FToneMapping::EMode Mode)
	{
		ToneMapping.Mode = Mode;
	}
}

#undef LOCTEXT_NAMESPACE

#endif // IMAGE_WIDGETS_BUILD_COLOR_VIEWER_SAMPLE