// Copyright Epic Games, Inc. All Rights Reserved.


#include "AudioMaterialSlate/SAudioMaterialMeter.h"
#include "AudioMaterialSlate/AudioMaterialMeter.h"
#include "Components/AudioComponent.h"
#include "SlateOptMacros.h"
#include "Styling/SlateBrush.h"


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAudioMaterialMeter::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	ValueAttribute = InArgs._ValueAttribute;
	OnValueChanged = InArgs._OnValueChanged;
	AudioMaterialMeterStyle = InArgs._AudioMaterialMeterStyle;

	ApplyNewMaterial();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

int32 SAudioMaterialMeter::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (AudioMaterialMeterStyle)
	{
		UMaterialInstanceDynamic* DynamicMaterial = AudioMaterialMeterStyle->GetDynamicMaterial();
		if (IsValid(DynamicMaterial))
		{

			DynamicMaterial->SetVectorParameterValue(FName("A (V3)"), AudioMaterialMeterStyle->MeterFillMinColor);
			DynamicMaterial->SetVectorParameterValue(FName("B (V3)"), AudioMaterialMeterStyle->MeterFillMidColor);
			DynamicMaterial->SetVectorParameterValue(FName("C (V3)"), AudioMaterialMeterStyle->MeterFillMaxColor);
			DynamicMaterial->SetVectorParameterValue(FName("OffColor"), AudioMaterialMeterStyle->MeterOffFillColor);
			DynamicMaterial->SetVectorParameterValue(FName("DotsOffColor"), AudioMaterialMeterStyle->MeterOffFillColor);

			const float Value = ValueAttribute.Get();
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

FVector2D SAudioMaterialMeter::ComputeDesiredSize(float) const
{
	FVector2D Vector = FVector2D(SlateWidth, SlateHeight);
	return Vector;
}

void SAudioMaterialMeter::SetValue(float InValueAttribute)
{
	CommitValue(InValueAttribute);
}

void SAudioMaterialMeter::ApplyNewMaterial()
{
	if (AudioMaterialMeterStyle)
	{
		AudioMaterialMeterStyle->CreateDynamicMaterial(Owner.Get());
	}
}

void SAudioMaterialMeter::CommitValue(float NewValue)
{
	float Val = FMath::Clamp(NewValue, 0.f , 1.f);
	if (!ValueAttribute.IsBound())
	{
		ValueAttribute.Set(Val);
	}

	OnValueChanged.ExecuteIfBound(Val);
}
