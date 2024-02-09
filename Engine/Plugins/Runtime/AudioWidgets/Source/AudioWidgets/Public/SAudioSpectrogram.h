// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioColorMapper.h"
#include "AudioSpectrogramViewport.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Slate Widget for rendering a time-frequency representation of a series of audio power spectra.
 */
class AUDIOWIDGETS_API SAudioSpectrogram : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAudioSpectrogram)
		: _ViewMinFrequency(20.0f)
		, _ViewMaxFrequency(20000.0f)
		, _ColorMapMinSoundLevel(-84.0f)
		, _ColorMapMaxSoundLevel(12.0f)
		, _ColorMap(EAudioColorMap::BlackToWhite)
		, _FrequencyAxisScale(EAudioSpectrogramFrequencyAxisScale::Logarithmic)
		, _FrequencyAxisPixelBucketMode(EAudioSpectrogramFrequencyAxisPixelBucketMode::Average)
		, _Orientation(EOrientation::Orient_Horizontal)
	{}
		SLATE_ATTRIBUTE(float, ViewMinFrequency)
		SLATE_ATTRIBUTE(float, ViewMaxFrequency)
		SLATE_ATTRIBUTE(float, ColorMapMinSoundLevel)
		SLATE_ATTRIBUTE(float, ColorMapMaxSoundLevel)
		SLATE_ATTRIBUTE(EAudioColorMap, ColorMap)
		SLATE_ATTRIBUTE(EAudioSpectrogramFrequencyAxisScale, FrequencyAxisScale)
		SLATE_ATTRIBUTE(EAudioSpectrogramFrequencyAxisPixelBucketMode, FrequencyAxisPixelBucketMode)
		SLATE_ATTRIBUTE(EOrientation, Orientation)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	/** Add the data for one spectrum frame to the spectrogram display */
	void AddFrame(const FAudioSpectrogramFrameData& SpectrogramFrameData);

	void SetViewMinFrequency(const float InViewMinFrequency) { ViewMinFrequency = InViewMinFrequency; }
	void SetViewMaxFrequency(const float InViewMaxFrequency) { ViewMaxFrequency = InViewMaxFrequency; }
	void SetColorMapMinSoundLevel(const float InColorMapMinSoundLevel) { ColorMapMinSoundLevel = InColorMapMinSoundLevel; }
	void SetColorMapMaxSoundLevel(const float InColorMapMaxSoundLevel) { ColorMapMaxSoundLevel = InColorMapMaxSoundLevel; }
	void SetColorMap(const EAudioColorMap InColorMap) { ColorMap = InColorMap; }
	void SetFrequencyAxisScale(const EAudioSpectrogramFrequencyAxisScale InFrequencyAxisScale) { FrequencyAxisScale = InFrequencyAxisScale; }
	void SetFrequencyAxisPixelBucketMode(const EAudioSpectrogramFrequencyAxisPixelBucketMode InFrequencyAxisPixelBucketMode) { FrequencyAxisPixelBucketMode = InFrequencyAxisPixelBucketMode; }
	void SetOrientation(const EOrientation InOrientation) { Orientation = InOrientation; }

private:
	// Begin SWidget overrides.
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	// End SWidget overrides.

	TAttribute<float> ViewMinFrequency;
	TAttribute<float> ViewMaxFrequency;
	TAttribute<float> ColorMapMinSoundLevel;
	TAttribute<float> ColorMapMaxSoundLevel;
	TAttribute<EAudioColorMap> ColorMap;
	TAttribute<EAudioSpectrogramFrequencyAxisScale> FrequencyAxisScale;
	TAttribute<EAudioSpectrogramFrequencyAxisPixelBucketMode> FrequencyAxisPixelBucketMode;
	TAttribute<EOrientation> Orientation;

	TSharedPtr<FAudioSpectrogramViewport> SpectrogramViewport;
};
