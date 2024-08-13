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
#if IMAGE_WIDGETS_WITH_AB_COMPARISON
		if (Properties.ABComparison.IsActive())
		{
			DrawImage(Properties.ABComparison.GuidA.B, Canvas, Properties.Placement, {0.0, 0.0}, {Properties.ABComparison.Threshold, 1.0});
			DrawImage(Properties.ABComparison.GuidB.B, Canvas, Properties.Placement, {Properties.ABComparison.Threshold, 0.0}, {1.0, 1.0});
		}
		else
		{
#endif
			DrawImage(SelectedColorIndex, Canvas, Properties.Placement, {0.0, 0.0}, {1.0, 1.0});
#if IMAGE_WIDGETS_WITH_AB_COMPARISON
		}
#endif
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

#if IMAGE_WIDGETS_WITH_AB_COMPARISON
	bool FColorViewer::IsValidImage(const FGuid& Guid) const
	{
		return ColorIsValid(Guid.B) && Colors[Guid.B].Guid == Guid;
	}

	FText FColorViewer::GetImageName(const FGuid& Guid) const
	{
		if (IsValidImage(Guid))
		{
			const FColor& Color = Colors[Guid.B].Color;
			const FString HexColor = FString::Printf(TEXT("#%02X%02X%02X"), Color.R, Color.G, Color.B);
			return FText::FromString(HexColor);
		}
		return {};
	}
#endif

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

	FLinearColor FColorViewer::GetDefaultToneMappedColor(const FColor& Color) const
	{
		return ToneMapping.GetToneMappedColor(Color);
	}

	bool FColorViewer::ColorIsValid(int32 Index) const
	{
		return 0 <= Index && Index < Colors.Num();
	}

	void FColorViewer::DrawImage(int32 Index, FCanvas* Canvas, const FDrawProperties::FPlacement& Placement, const FVector2d& UV0, const FVector2d& UV1) const
	{
		if (ColorIsValid(Index))
		{
			// Get color value after tone mapping.
			const FLinearColor ToneMappedColor = ToneMapping.GetToneMappedColor(Colors[Index].Color);

			// Adjust offset and size based on which part of the image to draw.
			const FVector2d Offset = Placement.Offset + Placement.Size * UV0;
			const FVector2d Size = Placement.Size * (UV1 - UV0);

			// Draw simple quad with current tone mapped color.
			// In a less trivial use case, this would require rendering quads with textures and the like. 
			FCanvasTileItem Tile(Offset, Size, ToneMappedColor);
			Canvas->DrawItem(Tile);
		}
	}
}

#undef LOCTEXT_NAMESPACE

#endif // IMAGE_WIDGETS_BUILD_COLOR_VIEWER_SAMPLE
