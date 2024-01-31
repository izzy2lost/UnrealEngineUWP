// Copyright Epic Games, Inc. All Rights Reserved.


#include "AudioMaterialSlate/SAudioMaterialKnob.h"
#include "SlateOptMacros.h"
#include "Components/Widget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SAudioMaterialKnob::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	AudioMaterialKnobStyle = InArgs._AudioMaterialKnobStyle;
	OnValueChanged = InArgs._OnFloatValueChanged;

	ApplyNewMaterial();
	CommitValue(InArgs._Value.Get());
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAudioMaterialKnob::SetValue(float InValue)
{
	CommitValue(InValue);
}

void SAudioMaterialKnob::ApplyNewMaterial()
{
	if (AudioMaterialKnobStyle)
	{
		AudioMaterialKnobStyle->CreateDynamicMaterial(Owner.Get());
	}
}

int32 SAudioMaterialKnob::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (AudioMaterialKnobStyle)
	{
		UMaterialInstanceDynamic* DynamicMaterial = AudioMaterialKnobStyle->GetDynamicMaterial();

		if (IsValid(DynamicMaterial))
		{
			const float KnobPercent = ValueAttribute.Get();

			//TODO unify material parameter names
			DynamicMaterial->SetVectorParameterValue(FName("Color_2"), AudioMaterialKnobStyle->KnobMainColor);
			DynamicMaterial->SetVectorParameterValue(FName("Color_1"), AudioMaterialKnobStyle->KnobAccentColor);

			DynamicMaterial->SetVectorParameterValue(FName("BarColor"), AudioMaterialKnobStyle->KnobBarColor);
			DynamicMaterial->SetVectorParameterValue(FName("BarShadowColor"), AudioMaterialKnobStyle->KnobBarShadowColor);
			DynamicMaterial->SetVectorParameterValue(FName("LEDglowMax"), AudioMaterialKnobStyle->KnobBarFillMaxColor);
			DynamicMaterial->SetVectorParameterValue(FName("Led_Max"), AudioMaterialKnobStyle->KnobBarFillMaxColor);
			DynamicMaterial->SetVectorParameterValue(FName("LEDglowMid"), AudioMaterialKnobStyle->KnobBarFillMidColor);
			DynamicMaterial->SetVectorParameterValue(FName("Led_Mid"), AudioMaterialKnobStyle->KnobBarFillMidColor);
			DynamicMaterial->SetVectorParameterValue(FName("LEDglowMin"), AudioMaterialKnobStyle->KnobBarFillMinColor);
			DynamicMaterial->SetVectorParameterValue(FName("LED_Min"), AudioMaterialKnobStyle->KnobBarFillMinColor);
			DynamicMaterial->SetVectorParameterValue(FName("LineColor"), AudioMaterialKnobStyle->KnobIndicatorColor);
			DynamicMaterial->SetVectorParameterValue(FName("LedTint"), AudioMaterialKnobStyle->KnobBarFillTintColor);

			DynamicMaterial->SetScalarParameterValue(FName("VALUE"), FMath::Clamp(KnobPercent, 0.f, 1.f));
		}

		const bool bEnabled = ShouldBeEnabled(bParentEnabled);
		const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

		const FLinearColor FinalColorAndOpacity(InWidgetStyle.GetColorAndOpacityTint());

		FSlateBrush Brush;
		Brush.SetResourceObject(DynamicMaterial);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId++, AllottedGeometry.ToPaintGeometry(),&Brush, DrawEffects, FinalColorAndOpacity);
	}

	return LayerId;
}

FVector2D SAudioMaterialKnob::ComputeDesiredSize(float) const
{
	FVector2D Vector = FVector2D(SlateWidth, SlateHeight);
	return Vector;
}

FReply SAudioMaterialKnob::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (this->HasMouseCapture())
	{
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
		MouseDownPosition = MouseEvent.GetScreenSpacePosition();
		MouseDownValue = ValueAttribute.Get();

		FReply Reply = FReply::Handled().CaptureMouse(SharedThis(this));
		return Reply;
	}

	return FReply::Unhandled();
}

FReply SAudioMaterialKnob::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && this->HasMouseCapture())
	{
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

void SAudioMaterialKnob::CommitValue(float NewValue)
{
	ValueAttribute.Set(NewValue);
	OnValueChanged.ExecuteIfBound(NewValue);
}
