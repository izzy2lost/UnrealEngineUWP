// Copyright Epic Games, Inc. All Rights Reserved.


#include "AudioMaterialSlate/SAudioMaterialSlider.h"
#include "AudioMaterialSlate/AudioMaterialSlider.h"
#include "SlateOptMacros.h"
#include "Styling/SlateBrush.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAudioMaterialSlider::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	OnValueChanged = InArgs._OnValueChanged;
	ValueAttribute = InArgs._ValueAttribute.Get();
	AudioMaterialSliderStyle = InArgs._AudioMaterialSliderStyle;

	ApplyNewMaterial();
	CommitValue(InArgs._ValueAttribute.Get());
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

int32 SAudioMaterialSlider::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{

	if (AudioMaterialSliderStyle)
	{
		UMaterialInstanceDynamic* DynamicMaterial = AudioMaterialSliderStyle->GetDynamicMaterial();
		if (IsValid(DynamicMaterial))
		{
			const float Value = ValueAttribute.Get();
			DynamicMaterial->SetVectorParameterValue(FName("BarColor"), AudioMaterialSliderStyle->BarMainColor);
			DynamicMaterial->SetVectorParameterValue(FName("LedColor"), AudioMaterialSliderStyle->BarShadowColor);
			DynamicMaterial->SetVectorParameterValue(FName("BarInnerShadow"), AudioMaterialSliderStyle->BarAccentColor);
			DynamicMaterial->SetVectorParameterValue(FName("ValueColor"), AudioMaterialSliderStyle->HandleMainColor);
			DynamicMaterial->SetVectorParameterValue(FName("DotBevel1"), AudioMaterialSliderStyle->HandleOutlineColor);
			DynamicMaterial->SetScalarParameterValue(FName("VALUE"), FMath::Clamp(Value, 0.f, 1.f));
		}

		const bool bEnabled = ShouldBeEnabled(bParentEnabled);
		const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
			
		const FLinearColor FinalColorAndOpacity(InWidgetStyle.GetColorAndOpacityTint());

		FSlateBrush Brush;
		Brush.SetResourceObject(DynamicMaterial);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId++, AllottedGeometry.ToPaintGeometry(), &Brush, DrawEffects, FinalColorAndOpacity);
	}

	return LayerId;
}

FVector2D SAudioMaterialSlider::ComputeDesiredSize(float) const
{
	FVector2D Vector = FVector2D(SlateWidth, SlateHeight);
	return Vector;
}

void SAudioMaterialSlider::SetValue(TAttribute<float> InValueAttribute)
{
	CommitValue(InValueAttribute.Get());
}

void SAudioMaterialSlider::ApplyNewMaterial()
{
	if (AudioMaterialSliderStyle)
	{
		AudioMaterialSliderStyle->CreateDynamicMaterial(Owner.Get());
	}
}

FReply SAudioMaterialSlider::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton))
	{
		FVector2D Value2D = PositionToValue(MyGeometry, MouseEvent.GetLastScreenSpacePosition());
		CommitValue(Value2D.Y);

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
		FVector2D Vector2D = PositionToValue(MyGeometry, MouseEvent.GetLastScreenSpacePosition());

		CommitValue(Vector2D.Y);

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SAudioMaterialSlider::CommitValue(float NewValue)
{
	float Val = FMath::Clamp(NewValue, 0.f , 1.f);
	if (!ValueAttribute.IsBound())
	{
		ValueAttribute.Set(Val);
	}

	OnValueChanged.ExecuteIfBound(Val);
}

FVector2D SAudioMaterialSlider::PositionToValue(const FGeometry& MyGeometry, const FVector2D& AbsolutePosition)
{
	const FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(AbsolutePosition);

	float RelativeValueX = 1 - (LocalPosition.X) / (MyGeometry.Size.X);
	float RelativeValueY = 1 - (LocalPosition.Y) / (MyGeometry.Size.Y);

	RelativeValueX = FMath::Clamp(RelativeValueX, 0.0f, 1.0f);
	RelativeValueY = FMath::Clamp(RelativeValueY, 0.0f, 1.0f);

	return FVector2D(RelativeValueX, RelativeValueY);
}
