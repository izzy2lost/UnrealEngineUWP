// Copyright Epic Games, Inc. All Rights Reserved.

#include "SLiveLinkHubPlaybackWidget.h"

#include "FrameNumberNumericInterface.h"
#include "SSimpleTimeSlider.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"

#define LOCTEXT_NAMESPACE "SLiveLinkHubPlaybackWidget"

void SLiveLinkHubPlaybackWidget::Construct(const FArguments& InArgs)
{
	OnPlayForwardDelegate = InArgs._OnPlayForward;
	OnPlayReverseDelegate = InArgs._OnPlayReverse;

	OnFirstFrameDelegate = InArgs._OnFirstFrame;
	OnLastFrameDelegate = InArgs._OnLastFrame;

	OnPreviousFrameDelegate = InArgs._OnPreviousFrame;
	OnNextFrameDelegate = InArgs._OnNextFrame;

	OnGetPausedDelegate = InArgs._IsPaused;
	OnGetIsInReverseDelegate = InArgs._IsInReverse;

	OnGetCurrentTimeDelegate = InArgs._GetCurrentTime;
	OnGetTotalLengthDelegate = InArgs._GetTotalLength;

	OnSetCurrentTimeDelegate = InArgs._SetCurrentTime;

	OnGetViewRangeDelegate = InArgs._GetViewRange;
	OnSetViewRangeDelegate = InArgs._SetViewRange;

	OnGetCurrentFrameDelegate = InArgs._GetCurrentFrame;

	OnGetSelectionStartTimeDelegate = InArgs._GetSelectionStartTime;
	OnGetSelectionEndTimeDelegate = InArgs._GetSelectionEndTime;
	OnSetSelectionStartTimeDelegate = InArgs._SetSelectionStartTime;
	OnSetSelectionEndTimeDelegate = InArgs._SetSelectionEndTime;
	
	OnSetLoopingDelegate = InArgs._OnSetLooping;
	OnGetLoopingDelegate = InArgs._IsLooping;

	OnGetTimeDelta = InArgs._GetTimeDelta;

	TAttribute<EFrameNumberDisplayFormats> GetDisplayFormatAttr = TAttribute<EFrameNumberDisplayFormats>(this, &SLiveLinkHubPlaybackWidget::GetDisplayFormat);
	TAttribute<FFrameRate> GetTickResolutionAttr = TAttribute<FFrameRate>(this, &SLiveLinkHubPlaybackWidget::GetFocusedTickResolution);
	TAttribute<FFrameRate> GetDisplayRateAttr    = TAttribute<FFrameRate>(this, &SLiveLinkHubPlaybackWidget::GetFocusedDisplayRate);
	
	// Create our numeric type interface so we can pass it to the time slider below.
	TSharedPtr<FFrameNumberInterface> NumberInterface = MakeShareable(new FFrameNumberInterface(GetDisplayFormatAttr, 0, GetTickResolutionAttr, GetDisplayRateAttr));
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(16.f, 8.f)
		[
			SNew(SSimpleTimeSlider)
			.ClampRangeHighlightSize(0.15f)
			.ClampRangeHighlightColor(FLinearColor::Gray.CopyWithNewOpacity(0.5f))
			.ScrubPosition(this, &SLiveLinkHubPlaybackWidget::GetCurrentTime)
			.ViewRange(this, &SLiveLinkHubPlaybackWidget::GetViewRange)
			.OnViewRangeChanged(this, &SLiveLinkHubPlaybackWidget::SetViewRange)
			.ClampRange(this, &SLiveLinkHubPlaybackWidget::GetClampRange)
			.OnScrubPositionChanged_Lambda(
				[this](double NewScrubTime, bool bIsScrubbing)
						{
							if (bIsScrubbing)
							{
								SetCurrentTime(NewScrubTime);
							}
						})
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.f, 8.f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "Animation.PlayControlsButton")
				.OnClicked(this, &SLiveLinkHubPlaybackWidget::OnFirstFramePressed)
				.ToolTipText(LOCTEXT("ToFront", "To Front"))
				.ContentPadding(0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Image(FAppStyle::Get().GetBrush("Animation.Backward_End"))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "Animation.PlayControlsButton")
				.OnClicked(this, &SLiveLinkHubPlaybackWidget::OnPreviousFramePressed)
				.ToolTipText(LOCTEXT("ToPrevious", "To Previous"))
				.ContentPadding(0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Image(FAppStyle::Get().GetBrush("Animation.Backward_Step"))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &SLiveLinkHubPlaybackWidget::OnPlayReversePressed)
				.ToolTipText_Lambda([this]()
				{
					const bool bIsPaused = IsPaused();
					return bIsPaused ? LOCTEXT("ReverseButton", "Reverse") : LOCTEXT("PauseButton", "Pause");
				})
				.ButtonStyle( FAppStyle::Get(), "Animation.PlayControlsButton" )
				.ContentPadding(0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Image(this, &SLiveLinkHubPlaybackWidget::GetPlayReverseIcon)
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &SLiveLinkHubPlaybackWidget::OnPlayForwardPressed)
				.ToolTipText_Lambda([this]()
				{
					const bool bIsPaused = IsPaused();
					return bIsPaused ? LOCTEXT("PlayButton", "Play") : LOCTEXT("PauseButton", "Pause");
				})
				.ButtonStyle( FAppStyle::Get(), "Animation.PlayControlsButton" )
				.ContentPadding(0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Image(this, &SLiveLinkHubPlaybackWidget::GetPlayForwardIcon)
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "Animation.PlayControlsButton")
				.OnClicked(this, &SLiveLinkHubPlaybackWidget::OnNextFramePressed)
				.ToolTipText(LOCTEXT("ToNext", "To Next"))
				.ContentPadding(0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Image(FAppStyle::Get().GetBrush("Animation.Forward_Step"))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "Animation.PlayControlsButton")
				.OnClicked(this, &SLiveLinkHubPlaybackWidget::OnLastFramePressed)
				.ToolTipText(LOCTEXT("ToEnd", "To End"))
				.ContentPadding(0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Image(FAppStyle::Get().GetBrush("Animation.Forward_End"))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "Animation.PlayControlsButton")
				.OnClicked(this, &SLiveLinkHubPlaybackWidget::OnLoopPressed)
				.ToolTipText_Raw(this, &SLiveLinkHubPlaybackWidget::GetLoopTooltip)
				.ContentPadding(0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Image(this, &SLiveLinkHubPlaybackWidget::GetLoopIcon)
				]
			]
			+SHorizontalBox::Slot()
			.Padding(12.f, 0.f, 0.f, 0.f)
			[
				SNew(SSpinBox<double>)
				.Value(this, &SLiveLinkHubPlaybackWidget::GetSelectionStartTime)
				.ToolTipText(LOCTEXT("SelectionStartTime", "Selection start time"))
				.OnValueCommitted(this, &SLiveLinkHubPlaybackWidget::OnSelectionStartTimeCommitted)
				.OnValueChanged(this, &SLiveLinkHubPlaybackWidget::SetSelectionStartTime)
				.MinValue(TOptional<double>())
				.MaxValue(TOptional<double>())
				.Style(&FAppStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
				.TypeInterface(NumberInterface)
				.ClearKeyboardFocusOnCommit(true)
				.Delta(this, &SLiveLinkHubPlaybackWidget::GetSpinboxDelta)
				.LinearDeltaSensitivity(25)
			]
			+SHorizontalBox::Slot()
			.Padding(4.f, 0.f)
			[
				SNew(SSpinBox<double>)
				.Value(this, &SLiveLinkHubPlaybackWidget::GetCurrentTime)
				.ToolTipText(LOCTEXT("CurrentTime", "Current playback time"))
				.OnValueCommitted(this, &SLiveLinkHubPlaybackWidget::OnCurrentTimeCommitted)
				.OnValueChanged(this, &SLiveLinkHubPlaybackWidget::SetCurrentTime)
				.MinValue(TOptional<double>())
				.MaxValue(TOptional<double>())
				.Style(&FAppStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
				.TypeInterface(NumberInterface)
				.ClearKeyboardFocusOnCommit(true)
				.Delta(this, &SLiveLinkHubPlaybackWidget::GetSpinboxDelta)
				.LinearDeltaSensitivity(25)
			]
			+SHorizontalBox::Slot()
			.Padding(4.f, 0.f)
			[
				SNew(SSpinBox<double>)
				.Value(this, &SLiveLinkHubPlaybackWidget::GetSelectionEndTime)
				.ToolTipText(LOCTEXT("SelectionEndTime", "Selection end time"))
				.OnValueCommitted(this, &SLiveLinkHubPlaybackWidget::OnSelectionEndTimeCommitted)
				.OnValueChanged(this, &SLiveLinkHubPlaybackWidget::SetSelectionEndTime)
				.MinValue(TOptional<double>())
				.MaxValue(TOptional<double>())
				.Style(&FAppStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
				.TypeInterface(NumberInterface)
				.ClearKeyboardFocusOnCommit(true)
				.Delta(this, &SLiveLinkHubPlaybackWidget::GetSpinboxDelta)
				.LinearDeltaSensitivity(25)
			]
			+SHorizontalBox::Slot()
			.Padding(4.f, 0.f)
			[
				SNew(SSpinBox<double>)
				.Value(this, &SLiveLinkHubPlaybackWidget::GetTotalLength)
				.IsEnabled(false)
				.ToolTipText(LOCTEXT("RecordingLength", "Recording length"))
				.MinValue(TOptional<double>())
				.MaxValue(TOptional<double>())
				.Style(&FAppStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
				.TypeInterface(NumberInterface)
			]
		]
	];
}

FReply SLiveLinkHubPlaybackWidget::OnPlayForwardPressed()
{
	OnPlayForwardDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SLiveLinkHubPlaybackWidget::OnPlayReversePressed()
{
	OnPlayReverseDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SLiveLinkHubPlaybackWidget::OnFirstFramePressed()
{
	OnFirstFrameDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SLiveLinkHubPlaybackWidget::OnLastFramePressed()
{
	OnLastFrameDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SLiveLinkHubPlaybackWidget::OnPreviousFramePressed()
{
	OnPreviousFrameDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SLiveLinkHubPlaybackWidget::OnNextFramePressed()
{
	OnNextFrameDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SLiveLinkHubPlaybackWidget::OnLoopPressed()
{
	check(OnSetLoopingDelegate.IsBound() && OnGetLoopingDelegate.IsBound());
	OnSetLoopingDelegate.Execute(!OnGetLoopingDelegate.Execute());
	return FReply::Handled();
}

void SLiveLinkHubPlaybackWidget::SetCurrentTime(double InTime)
{
	check(OnSetCurrentTimeDelegate.IsBound());
	OnSetCurrentTimeDelegate.Execute(InTime);
}

void SLiveLinkHubPlaybackWidget::OnCurrentTimeCommitted(double InTime, ETextCommit::Type InTextCommit)
{
	SetCurrentTime(InTime);
}

double SLiveLinkHubPlaybackWidget::GetCurrentTime() const
{
	check(OnGetCurrentTimeDelegate.IsBound());
	return OnGetCurrentTimeDelegate.Execute();
}

double SLiveLinkHubPlaybackWidget::GetTotalLength() const
{
	check(OnGetTotalLengthDelegate.IsBound());
	return OnGetTotalLengthDelegate.Execute();
}

int32 SLiveLinkHubPlaybackWidget::GetCurrentFrame() const
{
	check(OnGetCurrentFrameDelegate.IsBound());
	return OnGetCurrentFrameDelegate.Execute();
}

double SLiveLinkHubPlaybackWidget::GetSelectionStartTime() const
{
	check(OnGetSelectionStartTimeDelegate.IsBound());
	return OnGetSelectionStartTimeDelegate.Execute();
}

void SLiveLinkHubPlaybackWidget::OnSelectionStartTimeCommitted(double InTime, ETextCommit::Type InTextCommit)
{
	SetSelectionStartTime(InTime);
}

void SLiveLinkHubPlaybackWidget::SetSelectionStartTime(double InTime)
{
	check(OnSetSelectionStartTimeDelegate.IsBound());
	OnSetSelectionStartTimeDelegate.Execute(InTime);
}

double SLiveLinkHubPlaybackWidget::GetSelectionEndTime() const
{
	check(OnGetSelectionEndTimeDelegate.IsBound());
	return OnGetSelectionEndTimeDelegate.Execute();
}

void SLiveLinkHubPlaybackWidget::OnSelectionEndTimeCommitted(double InTime, ETextCommit::Type InTextCommit)
{
	SetSelectionEndTime(InTime);
}

void SLiveLinkHubPlaybackWidget::SetSelectionEndTime(double InTime)
{
	check(OnSetSelectionEndTimeDelegate.IsBound());
	OnSetSelectionEndTimeDelegate.Execute(InTime);
}

TRange<double> SLiveLinkHubPlaybackWidget::GetViewRange() const
{
	check(OnGetViewRangeDelegate.IsBound());
	return OnGetViewRangeDelegate.Execute();
}

void SLiveLinkHubPlaybackWidget::SetViewRange(TRange<double> InRange)
{
	check(OnSetViewRangeDelegate.IsBound());
	OnSetViewRangeDelegate.Execute(InRange);
}

TRange<double> SLiveLinkHubPlaybackWidget::GetClampRange() const
{
	return TRange<double>(GetSelectionStartTime(), GetSelectionEndTime());
}

bool SLiveLinkHubPlaybackWidget::IsPaused() const
{
	return OnGetPausedDelegate.IsBound() ? OnGetPausedDelegate.Execute() : false;
}

bool SLiveLinkHubPlaybackWidget::IsPlayingInReverse() const
{
	return OnGetIsInReverseDelegate.IsBound() ? OnGetIsInReverseDelegate.Execute() : false;
}

double SLiveLinkHubPlaybackWidget::GetSpinboxDelta() const
{
	check(OnGetTimeDelta.IsBound());
	return OnGetTimeDelta.Execute();
}

EFrameNumberDisplayFormats SLiveLinkHubPlaybackWidget::GetDisplayFormat() const
{
	return DisplayFormat;
}

FFrameRate SLiveLinkHubPlaybackWidget::GetFocusedTickResolution() const
{
	// todo
	const int32 TickResolutionValue = 1000 * 24;
	return FFrameRate(1, 1);
}

FFrameRate SLiveLinkHubPlaybackWidget::GetFocusedDisplayRate() const
{
	// todo
	const int32 SequenceFrameRate = 24;
	return FFrameRate(SequenceFrameRate, 1);
}

const FSlateBrush* SLiveLinkHubPlaybackWidget::GetPlayForwardIcon() const
{
	return IsPaused() || IsPlayingInReverse() ? FAppStyle::Get().GetBrush("Animation.Forward") : FAppStyle::Get().GetBrush("Animation.Pause");
}

const FSlateBrush* SLiveLinkHubPlaybackWidget::GetPlayReverseIcon() const
{
	return IsPaused() || !IsPlayingInReverse() ? FAppStyle::Get().GetBrush("Animation.Backward") : FAppStyle::Get().GetBrush("Animation.Pause");
}

const FSlateBrush* SLiveLinkHubPlaybackWidget::GetLoopIcon() const
{
	check(OnGetLoopingDelegate.IsBound());
	const bool bLooping = OnGetLoopingDelegate.Execute();
	return bLooping ? FAppStyle::Get().GetBrush("Animation.Loop.Enabled")
		: FAppStyle::Get().GetBrush("Animation.Loop.Disabled");
}

FText SLiveLinkHubPlaybackWidget::GetLoopTooltip() const
{
	check(OnGetLoopingDelegate.IsBound());
	const bool bLooping = OnGetLoopingDelegate.Execute();

	return bLooping ? LOCTEXT("Loop", "Loop") : LOCTEXT("NoLoop", "No looping");
}

#undef LOCTEXT_NAMESPACE
