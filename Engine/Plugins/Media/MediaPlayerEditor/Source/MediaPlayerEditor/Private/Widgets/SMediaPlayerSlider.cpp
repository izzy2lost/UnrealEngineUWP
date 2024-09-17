// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMediaPlayerSlider.h"
#include "MediaPlayer.h"
#include "SlateOptMacros.h"
#include "Widgets/Input/SSlider.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMediaPlayerSlider::Construct(const FArguments& InArgs, const TArrayView<TWeakObjectPtr<UMediaPlayer>> InMediaPlayers)
{
	MediaPlayerEntries.Reserve(InMediaPlayers.Num());
	for (const TWeakObjectPtr<UMediaPlayer>& MediaPlayerWeak : InMediaPlayers)
	{
		if (MediaPlayerWeak.IsValid())
		{
			MediaPlayerEntries.Emplace(MediaPlayerWeak);
		}
	}

	ChildSlot
	[
		SAssignNew(ScrubberSlider, SSlider)
		.IsEnabled_Raw(this, &SMediaPlayerSlider::DoesMediaPlayerSupportSeeking)
		.OnMouseCaptureBegin_Raw(this, &SMediaPlayerSlider::OnScrubBegin)
		.OnMouseCaptureEnd_Raw(this, &SMediaPlayerSlider::OnScrubEnd)
		.OnValueChanged_Raw(this, &SMediaPlayerSlider::Seek)
		.Value_Raw(this, &SMediaPlayerSlider::GetPlaybackPosition)
		.Visibility_Raw(this, &SMediaPlayerSlider::GetScrubberVisibility)
		.Orientation(Orient_Horizontal)
		.SliderBarColor(FLinearColor::Transparent)
		.Style(InArgs._Style)
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

bool SMediaPlayerSlider::DoesMediaPlayerSupportSeeking() const
{
	// All players must support seek.
	bool bSupportSeek = false;
	for (const FMediaPlayerEntry& Entry : MediaPlayerEntries)
	{
		if (const UMediaPlayer* MediaPlayer = Entry.MediaPlayerWeak.Get())
		{
			bSupportSeek = MediaPlayer->SupportsSeeking();
			if (!bSupportSeek)
			{
				return false;
			}
		}
	}

	return bSupportSeek;
}

void SMediaPlayerSlider::OnScrubBegin()
{
	for (FMediaPlayerEntry& Entry : MediaPlayerEntries)
	{
		if (UMediaPlayer* MediaPlayer = Entry.MediaPlayerWeak.Get())
		{
			Entry.ScrubValue = static_cast<float>(FTimespan::Ratio(MediaPlayer->GetDisplayTime(), MediaPlayer->GetDuration()));
			Entry.LastScrubValue = Entry.ScrubValue;

			if (MediaPlayer->SupportsScrubbing())
			{
				Entry.PreScrubRate = MediaPlayer->GetRate();
				MediaPlayer->SetRate(0.0f);
			}
		}
	}
}

void SMediaPlayerSlider::OnScrubEnd()
{
	for (FMediaPlayerEntry& Entry : MediaPlayerEntries)
	{
		if (UMediaPlayer* MediaPlayer = Entry.MediaPlayerWeak.Get())
		{
			// Set playback position to scrub value when drag ends
			if (Entry.LastScrubValue != Entry.ScrubValue)
			{
				MediaPlayer->Seek(MediaPlayer->GetDuration() * Entry.ScrubValue);
			}

			if (MediaPlayer->SupportsScrubbing())
			{
				MediaPlayer->SetRate(Entry.PreScrubRate);
			}
		}
	}
}

void SMediaPlayerSlider::Seek(float InPlaybackPosition)
{
	for (FMediaPlayerEntry& Entry : MediaPlayerEntries)
	{
		if (UMediaPlayer* MediaPlayer = Entry.MediaPlayerWeak.Get())
		{
			Entry.ScrubValue = InPlaybackPosition;

			if (!ScrubberSlider->HasMouseCapture() || MediaPlayer->SupportsScrubbing())
			{
				MediaPlayer->Seek(MediaPlayer->GetDuration() * InPlaybackPosition);
				Entry.LastScrubValue = Entry.ScrubValue;
			}
		}
	}
}

float SMediaPlayerSlider::GetPlaybackPosition() const
{
	// Note: returns first one. All scrub positions should match.
	for (const FMediaPlayerEntry& Entry : MediaPlayerEntries)
	{
		if (const UMediaPlayer* MediaPlayer = Entry.MediaPlayerWeak.Get())
		{
			if (ScrubberSlider->HasMouseCapture())
			{
				return Entry.ScrubValue;
			}

			return (MediaPlayer->GetDuration() > FTimespan::Zero())
				? static_cast<float>(FTimespan::Ratio(MediaPlayer->GetDisplayTime(), MediaPlayer->GetDuration()))
				: 0.0f;
		}
	}

	return 0.0f;
}

EVisibility SMediaPlayerSlider::GetScrubberVisibility() const
{
	bool bIsActive = false;
	for (const FMediaPlayerEntry& Entry : MediaPlayerEntries)
	{
		if (const UMediaPlayer* MediaPlayer = Entry.MediaPlayerWeak.Get())
		{
			bIsActive = (MediaPlayer->SupportsScrubbing() || MediaPlayer->SupportsSeeking());
			if (!bIsActive)
			{
				break; // If any player is inactive, consider widget inactive.
			}
		}
	}

	return bIsActive ? EVisibility::Visible : VisibilityWhenInactive;
}

void SMediaPlayerSlider::SetSliderHandleColor(const FSlateColor& InSliderColor)
{
	if (ScrubberSlider)
	{
		ScrubberSlider->SetSliderHandleColor(InSliderColor);
	}
}

void SMediaPlayerSlider::SetSliderBarColor(const FSlateColor& InSliderColor)
{
	if (ScrubberSlider)
	{
		ScrubberSlider->SetSliderBarColor(InSliderColor);
	}
}

void SMediaPlayerSlider::SetVisibleWhenInactive(EVisibility InVisibility)
{
	VisibilityWhenInactive = InVisibility;
}
