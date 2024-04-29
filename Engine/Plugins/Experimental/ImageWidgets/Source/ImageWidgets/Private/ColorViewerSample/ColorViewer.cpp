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
		if (ColorIsValid(SelectedColorIndex))
		{
			return {Colors[SelectedColorIndex].Guid, ImageSize, 0, true};
		}
		return {FGuid(), FIntPoint::ZeroValue, 0, false};
	}

	void FColorViewer::DrawCurrentImage(FViewport* Viewport, FCanvas* Canvas, const FDrawProperties& Properties)
	{
		if (ColorIsValid(SelectedColorIndex))
		{
			// Get color value after tone mapping.
			const FLinearColor ToneMappedColor = ToneMapping.GetToneMappedColor(Colors[SelectedColorIndex].Color);

			// Draw simple quad with current tone mapped color.
			// In a less trivial use case, this would require rendering quads with textures and the like. 
			FCanvasTileItem Tile(Properties.Placement.Offset, Properties.Placement.Size, ToneMappedColor);
			Canvas->DrawItem(Tile);
		}
	}

	TOptional<TVariant<FColor, FLinearColor>> FColorViewer::GetCurrentImagePixelColor(FIntPoint PixelCoords, int32 MipLevel) const
	{
		if (ColorIsValid(SelectedColorIndex))
		{
			// Returns the current color as float values.
			// In a less trivial use case, the pixel coordinates and potentially the MIP level would be needed to look up the color value.
			return TVariant<FColor, FLinearColor>(TInPlaceType<FColor>(), Colors[SelectedColorIndex].Color);
		}
		return {};
	}

	void FColorViewer::OnImageSelected(const FGuid& Guid)
	{
		if (ColorIsValid(Guid.B) && Colors[Guid.B].Guid == Guid)
		{
			SelectedColorIndex = Guid.B;
		}
	}

	const FColorViewer::FColorItem* FColorViewer::AddColor()
	{
		Colors.Add({FGuid(1, Colors.Num(), 0, 0), {}, FDateTime::Now()});

		SelectedColorIndex = Colors.Num() - 1;

		RandomizeColor();

		return &Colors[SelectedColorIndex];
	}

	const FColorViewer::FColorItem* FColorViewer::RandomizeColor()
	{
		if (ColorIsValid(SelectedColorIndex))
		{
			auto Random = []
			{
				return static_cast<uint8>(FMath::RandRange(0, 255));
			};

			Colors[SelectedColorIndex].Color = {Random(), Random(), Random()};

			return &Colors[SelectedColorIndex];
		}

		return nullptr;
	}

	FToneMapping::EMode FColorViewer::GetToneMapping() const
	{
		return ToneMapping.Mode;
	}

	void FColorViewer::SetToneMapping(FToneMapping::EMode Mode)
	{
		ToneMapping.Mode = Mode;
	}

	bool FColorViewer::ColorIsValid(int32 Index) const
	{
		return SelectedColorIndex != INDEX_NONE && SelectedColorIndex < Colors.Num();
	}
}

#undef LOCTEXT_NAMESPACE

#endif // IMAGE_WIDGETS_BUILD_COLOR_VIEWER_SAMPLE
