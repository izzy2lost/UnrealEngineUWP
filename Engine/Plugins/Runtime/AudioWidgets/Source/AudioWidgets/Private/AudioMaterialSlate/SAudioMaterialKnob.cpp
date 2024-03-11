// Copyright Epic Games, Inc. All Rights Reserved.


#include "AudioMaterialSlate/SAudioMaterialKnob.h"
#include "AudioMaterialSlate/AudioMaterialSlateTypes.h"
#include "AudioWidgetsStyle.h"
#include "SlateOptMacros.h"
#include "Components/Widget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SAudioMaterialKnob::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	AudioMaterialKnobStyle = InArgs._AudioMaterialKnobStyle;
	OnValueChanged = InArgs._OnFloatValueChanged;
	OnMouseCaptureBegin = InArgs._OnMouseCaptureBegin;
	OnMouseCaptureEnd = InArgs._OnMouseCaptureEnd;

	//For now check if owner is not a widget -> use the default style.
	if (!Cast<UWidget>(Owner))
	{
		AudioMaterialKnobStyle = &FAudioWidgetsStyle::Get().GetWidgetStyle<FAudioMaterialKnobStyle>("AudioMaterialKnob.Style");
	}

	ApplyNewMaterial();

	if (InArgs._Value.IsSet())
	{
		CommitValue(InArgs._Value.Get());
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAudioMaterialKnob::SetValue(float InValue)
{
	CommitValue(InValue);
}

UMaterialInstanceDynamic* SAudioMaterialKnob::ApplyNewMaterial()
{
	if (AudioMaterialKnobStyle)
	{
		DynamicMaterial = AudioMaterialKnobStyle->CreateDynamicMaterial(Owner.Get());
	}

	return DynamicMaterial.Get();
}
const float SAudioMaterialKnob::GetOutputValue(const float InSliderValue)
{
	return FMath::GetMappedRangeValueClamped(NormalizedLinearSliderRange, OutputRange, InSliderValue);
}

const float SAudioMaterialKnob::GetSliderValue(const float OutputValue)
{
	return FMath::GetMappedRangeValueClamped(OutputRange, NormalizedLinearSliderRange, OutputValue);
}

void SAudioMaterialKnob::SetOutputRange(const FVector2D Range)
{
	OutputRange = Range;
	// if Range.Y < Range.X, set Range.X to Range.Y
	OutputRange.X = FMath::Min(Range.X, Range.Y);

	const float OutputValue = GetOutputValue(ValueAttribute.Get());
	const float ClampedOutputValue = FMath::Clamp(OutputValue, OutputRange.X, OutputRange.Y);
	const float ClampedSliderValue = GetSliderValue(ClampedOutputValue);
	ValueAttribute.Set(ClampedSliderValue);
}

void SAudioMaterialKnob::SetDesiredSizeOverride(const FVector2D Size)
{
	SetAttribute(DesiredSizeOverride, TAttribute<TOptional<FVector2D>>(Size), EInvalidateWidgetReason::Layout);
}

int32 SAudioMaterialKnob::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (AudioMaterialKnobStyle)
	{
		if (DynamicMaterial.IsValid())
		{
			const float KnobPercent = ValueAttribute.Get();

			//TODO unify material parameter names
			DynamicMaterial.Get()->SetVectorParameterValue(FName("Color_2"), AudioMaterialKnobStyle->KnobMainColor);
			DynamicMaterial.Get()->SetVectorParameterValue(FName("Color_1"), AudioMaterialKnobStyle->KnobAccentColor);

			DynamicMaterial.Get()->SetVectorParameterValue(FName("BarColor"), AudioMaterialKnobStyle->KnobBarColor);
			DynamicMaterial.Get()->SetVectorParameterValue(FName("BarShadowColor"), AudioMaterialKnobStyle->KnobBarShadowColor);
			DynamicMaterial.Get()->SetVectorParameterValue(FName("LEDglowMax"), AudioMaterialKnobStyle->KnobBarFillMaxColor);
			DynamicMaterial.Get()->SetVectorParameterValue(FName("Led_Max"), AudioMaterialKnobStyle->KnobBarFillMaxColor);
			DynamicMaterial.Get()->SetVectorParameterValue(FName("LEDglowMid"), AudioMaterialKnobStyle->KnobBarFillMidColor);
			DynamicMaterial.Get()->SetVectorParameterValue(FName("Led_Mid"), AudioMaterialKnobStyle->KnobBarFillMidColor);
			DynamicMaterial.Get()->SetVectorParameterValue(FName("LEDglowMin"), AudioMaterialKnobStyle->KnobBarFillMinColor);
			DynamicMaterial.Get()->SetVectorParameterValue(FName("LED_Min"), AudioMaterialKnobStyle->KnobBarFillMinColor);
			DynamicMaterial.Get()->SetVectorParameterValue(FName("LineColor"), AudioMaterialKnobStyle->KnobIndicatorColor);
			DynamicMaterial.Get()->SetVectorParameterValue(FName("DotColor"), AudioMaterialKnobStyle->KnobIndicatorColor);
			DynamicMaterial.Get()->SetVectorParameterValue(FName("LedTint"), AudioMaterialKnobStyle->KnobBarFillTintColor);

			DynamicMaterial.Get()->SetScalarParameterValue(FName("VALUE"), FMath::Clamp(KnobPercent, 0.f, 1.f));

			DynamicMaterial.Get()->SetScalarParameterValue(FName("LocalWidth"), AllottedGeometry.GetLocalSize().X);
			DynamicMaterial.Get()->SetScalarParameterValue(FName("LocalHeigth"), AllottedGeometry.GetLocalSize().Y);

			const bool bEnabled = ShouldBeEnabled(bParentEnabled);
			const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

			const FLinearColor FinalColorAndOpacity(InWidgetStyle.GetColorAndOpacityTint());
			
			const float AllottedWidth = AllottedGeometry.GetLocalSize().X;
			const float AllottedHeight = AllottedGeometry.GetLocalSize().Y;
			
			const float SliderRadius = FMath::Min(AllottedWidth, AllottedHeight) * 0.5f;
			const FVector2D SliderMidPoint(AllottedGeometry.GetLocalSize() * 0.5f);
			const FVector2D SliderDiameter(SliderRadius * 2);

			FSlateBrush Brush;
			Brush.SetResourceObject(DynamicMaterial.Get());
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId++, AllottedGeometry.ToPaintGeometry(SliderDiameter, FSlateLayoutTransform(SliderMidPoint-SliderRadius)), &Brush, DrawEffects, FinalColorAndOpacity);
		}
		else
		{
			if (AudioMaterialKnobStyle)
			{
				DynamicMaterial = AudioMaterialKnobStyle->CreateDynamicMaterial(Owner.Get());
			}
		}
	}

	return LayerId;
}

FVector2D SAudioMaterialKnob::ComputeDesiredSize(float) const
{
	if (DesiredSizeOverride.Get().IsSet())
	{
		return DesiredSizeOverride.Get().GetValue();
	}
	
	if (AudioMaterialKnobStyle)
	{
		return FVector2D(AudioMaterialKnobStyle->DesiredSize);
	}

	return FVector2D::ZeroVector;
}

FReply SAudioMaterialKnob::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (this->HasMouseCapture())
	{
		SetCursor(EMouseCursor::GrabHandClosed);

		int32 CurrentYValue = MouseEvent.GetLastScreenSpacePosition().Y;
		const float MouseSpeed = 0.2f;

		float ValueDelta = (float)(MouseDownPosition.Y - CurrentYValue) / PixelDelta * MouseSpeed;
		float NewValue = FMath::Clamp(MouseDownValue + ValueDelta, 0.0f, 1.0f);
		CommitValue(NewValue);

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SAudioMaterialKnob::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton))
	{
		CachedCursor = GetCursor().Get(EMouseCursor::Default);

		MouseDownPosition = MouseEvent.GetScreenSpacePosition();
		MouseDownValue = ValueAttribute.Get();
		OnMouseCaptureBegin.ExecuteIfBound();

		FReply Reply = FReply::Handled().CaptureMouse(SharedThis(this));
		return Reply;
	}

	return FReply::Unhandled();
}

FReply SAudioMaterialKnob::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && this->HasMouseCapture())
	{
		SetCursor(CachedCursor);
		OnMouseCaptureEnd.ExecuteIfBound();
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

void SAudioMaterialKnob::CommitValue(float NewValue)
{
	const float OldValue = ValueAttribute.Get();
	float Val = FMath::Clamp(NewValue, 0.f, 1.f);

	if (NewValue != OldValue)
	{
		if (!ValueAttribute.IsBound())
		{
			ValueAttribute.Set(Val);
		}

		Invalidate(EInvalidateWidgetReason::Paint);
		OnValueChanged.ExecuteIfBound(Val);
	}
}