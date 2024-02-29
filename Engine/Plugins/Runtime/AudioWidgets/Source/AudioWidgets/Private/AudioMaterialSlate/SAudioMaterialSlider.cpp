// Copyright Epic Games, Inc. All Rights Reserved.


#include "AudioMaterialSlate/SAudioMaterialSlider.h"
#include "AudioMaterialSlate/AudioMaterialSlider.h"
#include "AudioWidgetsStyle.h"
#include "SlateOptMacros.h"
#include "Styling/SlateBrush.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAudioMaterialSlider::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	Orientation = InArgs._Orientation;
	AudioMaterialSliderStyle = InArgs._AudioMaterialSliderStyle;
	OnValueChanged = InArgs._OnValueChanged;
	OnValueCommitted = InArgs._OnValueCommitted;

	//For now check if owner is not a widget -> use the default style.
	if (!Cast<UWidget>(Owner))
	{
		AudioMaterialSliderStyle = &FAudioWidgetsStyle::Get().GetWidgetStyle<FAudioMaterialSliderStyle>("AudioMaterialSlider.Style");
	}

	ApplyNewMaterial();

	if (InArgs._ValueAttribute.IsSet())
	{
		CommitValue(InArgs._ValueAttribute.Get());
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

int32 SAudioMaterialSlider::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (AudioMaterialSliderStyle)
	{
		if (DynamicMaterial.IsValid())
		{
			const float Value = ValueAttribute.Get();
			DynamicMaterial.Get()->SetVectorParameterValue(FName("BarColor"), AudioMaterialSliderStyle->BarMainColor);
			DynamicMaterial.Get()->SetVectorParameterValue(FName("LedColor"), AudioMaterialSliderStyle->BarShadowColor);
			DynamicMaterial.Get()->SetVectorParameterValue(FName("BarInnerShadow"), AudioMaterialSliderStyle->BarAccentColor);
			DynamicMaterial.Get()->SetVectorParameterValue(FName("ValueColor"), AudioMaterialSliderStyle->HandleMainColor);
			DynamicMaterial.Get()->SetVectorParameterValue(FName("DotBevel1"), AudioMaterialSliderStyle->HandleOutlineColor);
			DynamicMaterial.Get()->SetScalarParameterValue(FName("VALUE"), FMath::Clamp(Value, 0.f, 1.f));
			DynamicMaterial.Get()->SetScalarParameterValue(FName("LedInt"), FMath::GetMappedRangeValueClamped(FVector2D(0.f,1.f),FVector2D(0.7f,4.f), Value));

			DynamicMaterial.Get()->SetScalarParameterValue(FName("LocalWidth"), AllottedGeometry.GetLocalSize().X);
			DynamicMaterial.Get()->SetScalarParameterValue(FName("LocalHeigth"), AllottedGeometry.GetLocalSize().Y);


			// We draw the slider as a vertical slider regardless of the orientation and apply a render transform to make it display in the correct orientation.
			FGeometry SliderGeometry = AllottedGeometry;
			const float AllottedWidth = Orientation == Orient_Vertical ? AllottedGeometry.GetLocalSize().X : AllottedGeometry.GetLocalSize().Y;
			const float AllottedHeight = Orientation == Orient_Vertical ? AllottedGeometry.GetLocalSize().Y : AllottedGeometry.GetLocalSize().X;

			// rotate the slider 90deg if should be horizontal.
			if (Orientation == Orient_Horizontal)
			{
				// rotate
				FSlateRenderTransform SlateRenderTransform = TransformCast<FSlateRenderTransform>(Concatenate(Inverse(FVector2f(0, AllottedHeight)), FQuat2D(FMath::DegreesToRadians(90.0f))));
				// create a child geometry matching this one, but with the rotated render transform that will be passed to the drawed slider.
				SliderGeometry = AllottedGeometry.MakeChild(
					FVector2f(AllottedWidth, AllottedHeight),
					FSlateLayoutTransform(),
					SlateRenderTransform, FVector2f::ZeroVector);
			}

			const bool bEnabled = ShouldBeEnabled(bParentEnabled);
			const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
				
			const FLinearColor FinalColorAndOpacity(InWidgetStyle.GetColorAndOpacityTint());

			FSlateBrush Brush;
			Brush.SetResourceObject(DynamicMaterial.Get());
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId++, SliderGeometry.ToPaintGeometry(FVector2D(AllottedWidth, AllottedHeight), FSlateLayoutTransform()), &Brush, DrawEffects, FinalColorAndOpacity);

		}
		else
		{
			if (AudioMaterialSliderStyle)
			{
				DynamicMaterial = AudioMaterialSliderStyle->CreateDynamicMaterial(Owner.Get());
			}
		}
	}

	return LayerId;
}

FVector2D SAudioMaterialSlider::ComputeDesiredSize(float) const
{
	if (DesiredSizeOverride.Get().IsSet())
	{
		return DesiredSizeOverride.Get().GetValue();
	}

	if (AudioMaterialSliderStyle)
	{
		const float Width = Orientation == Orient_Vertical ? AudioMaterialSliderStyle->DesiredSize.X : AudioMaterialSliderStyle->DesiredSize.Y;
		const float Heigth = Orientation == Orient_Vertical ? AudioMaterialSliderStyle->DesiredSize.Y : AudioMaterialSliderStyle->DesiredSize.X;
		return FVector2D(Width, Heigth);
	}

	return FVector2D::ZeroVector;
}

void SAudioMaterialSlider::SetValue(TAttribute<float> InValueAttribute)
{
	CommitValue(InValueAttribute.Get());
}

void SAudioMaterialSlider::ApplyNewMaterial()
{
	if (AudioMaterialSliderStyle)
	{
		DynamicMaterial = AudioMaterialSliderStyle->CreateDynamicMaterial(Owner.Get());
	}
}

void SAudioMaterialSlider::SetOrientation(EOrientation InOrientation)
{
	if (Orientation != InOrientation)
	{
		Orientation = InOrientation;
		Invalidate(EInvalidateWidgetReason::Layout);
	}
}

FReply SAudioMaterialSlider::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton))
	{
		FVector2D Value2D = PositionToValue(MyGeometry, MouseEvent.GetLastScreenSpacePosition());
		const float Value = Orientation == Orient_Horizontal ? Value2D.X : Value2D.Y;
		CommitValue(Value);

		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	return FReply::Unhandled();
}

FReply SAudioMaterialSlider::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && HasMouseCapture())
	{
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

FReply SAudioMaterialSlider::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (this->HasMouseCapture())
	{
		FVector2D Value2D = PositionToValue(MyGeometry, MouseEvent.GetLastScreenSpacePosition());
		const float Value = Orientation == Orient_Horizontal ? Value2D.X : Value2D.Y;
		CommitValue(Value);

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SAudioMaterialSlider::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	OnValueCommitted.ExecuteIfBound(ValueAttribute.Get());
	SLeafWidget::OnMouseCaptureLost(CaptureLostEvent);
}
void SAudioMaterialSlider::CommitValue(float NewValue)
{
	const float OldValue = ValueAttribute.Get();
	float Val = FMath::Clamp(NewValue, 0.f , 1.f);

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

FVector2D SAudioMaterialSlider::PositionToValue(const FGeometry& MyGeometry, const FVector2D& AbsolutePosition)
{
	const FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(AbsolutePosition);

	float RelativeValueX = (LocalPosition.X) / (MyGeometry.Size.X);
	float RelativeValueY = 1 - (LocalPosition.Y) / (MyGeometry.Size.Y);

	RelativeValueX = FMath::Clamp(RelativeValueX, 0.0f, 1.0f);
	RelativeValueY = FMath::Clamp(RelativeValueY, 0.0f, 1.0f);

	return FVector2D(RelativeValueX, RelativeValueY);
}
