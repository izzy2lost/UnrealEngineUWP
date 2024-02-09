// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAudioSpectrogram.h"

void SAudioSpectrogram::Construct(const FArguments& InArgs)
{
	ViewMinFrequency = InArgs._ViewMinFrequency;
	ViewMaxFrequency = InArgs._ViewMaxFrequency;
	ColorMapMinSoundLevel = InArgs._ColorMapMinSoundLevel;
	ColorMapMaxSoundLevel = InArgs._ColorMapMaxSoundLevel;
	ColorMap = InArgs._ColorMap;
	FrequencyAxisScale = InArgs._FrequencyAxisScale;
	FrequencyAxisPixelBucketMode = InArgs._FrequencyAxisPixelBucketMode;
	Orientation = InArgs._Orientation;

	SpectrogramViewport = MakeShareable(new FAudioSpectrogramViewport());
}

void SAudioSpectrogram::AddFrame(const FAudioSpectrogramFrameData& SpectrogramFrameData)
{
	SpectrogramViewport->AddFrame(SpectrogramFrameData);
}

int32 SAudioSpectrogram::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// Get post DPI-scaled sizing:
	const FVector2f AbsoluteSize = AllottedGeometry.GetAbsoluteSize();

	// Round down size to whole pixels:
	const int RenderWidth = FMath::FloorToInt(AbsoluteSize.X);
	const int RenderHeight = FMath::FloorToInt(AbsoluteSize.Y);
	const FVector2f RenderSize(RenderWidth, RenderHeight);
	const FVector2f AdjustedLocalSize = TransformVector(Inverse(AllottedGeometry.GetAccumulatedRenderTransform()), RenderSize);

	// Apply rotation to viewport as required by Orientation attribute:
	const bool bRotateViewport = (Orientation.Get() == EOrientation::Orient_Horizontal);
	const FMatrix2x2f ScaleAndRotate(0.0f, -RenderSize.Y / RenderSize.X, RenderSize.X / RenderSize.Y, 0.0f); // Scale and rotate to swap X and Y.
	const FSlateRenderTransform ViewportRenderTransform = (bRotateViewport) ? FSlateRenderTransform(ScaleAndRotate) : FSlateRenderTransform();
	const FVector2f ViewportRenderTransformPivot(0.5f);

	// Create a child geometry using adjusted size and possible render rotation:
	const FGeometry ChildGeometry = AllottedGeometry.MakeChild(AdjustedLocalSize, FSlateLayoutTransform(), ViewportRenderTransform, ViewportRenderTransformPivot);
	
	// Create spectrogram render params, setting viewport history size to exact pixel size:
	const FAudioSpectrogramViewportRenderParams RenderParams
	{
		.NumRows = (bRotateViewport) ? RenderWidth : RenderHeight,
		.NumPixelsPerRow = FMath::Max((bRotateViewport) ? RenderHeight : RenderWidth, 2),
		.ViewMinFrequency = ViewMinFrequency.Get(),
		.ViewMaxFrequency = ViewMaxFrequency.Get(),
		.ColorMapMinSoundLevel = ColorMapMinSoundLevel.Get(),
		.ColorMapMaxSoundLevel = ColorMapMaxSoundLevel.Get(),
		.ColorMap = ColorMap.Get(),
		.FrequencyAxisScale = FrequencyAxisScale.Get(),
		.FrequencyAxisPixelBucketMode = FrequencyAxisPixelBucketMode.Get(),
	};
	SpectrogramViewport->SetRenderParams(RenderParams);

	// Create the viewport using child geometry:
	FSlateDrawElement::MakeViewport(OutDrawElements, LayerId, ChildGeometry.ToPaintGeometry(), SpectrogramViewport, ESlateDrawEffect::NoGamma);

	return LayerId + 1;
}
