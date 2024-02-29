// Copyright Epic Games, Inc. All Rights Reserved.


#include "AudioMaterialSlate/AudioMaterialSlider.h"
#include "AudioMaterialSlate/AudioMaterialSlateTypes.h"
#include "AudioMaterialSlate/SAudioMaterialSlider.h"
#include "AudioWidgetsStyle.h"
#include "Widgets/SWeakWidget.h"

#define LOCTEXT_NAMESPACE "AudioWidgets"

UAudioMaterialSlider::UAudioMaterialSlider()
{
	//get default style
	WidgetStyle = FAudioWidgetsStyle::Get().GetWidgetStyle<FAudioMaterialSliderStyle>("AudioMaterialSlider.Style");
}

#if WITH_EDITOR
const FText UAudioMaterialSlider::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "AudioMaterial");
}
#endif

void UAudioMaterialSlider::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (!Slider.IsValid())
	{
		return;
	}

	Slider->SetValue(Value);
	Slider->SetOrientation(Orientation);
	Slider->ApplyNewMaterial();
}

void UAudioMaterialSlider::ReleaseSlateResources(bool bReleaseChildren)
{
	Slider.Reset();
}

float UAudioMaterialSlider::GetValue() const
{
	return Value;
}

void UAudioMaterialSlider::SetValue(float InValue)
{
	InValue = FMath::Clamp(InValue, 0.f, 1.f);

	if (Slider.IsValid())
	{
		Slider->SetValue(InValue);
	}

	if (Value != InValue)
	{
		Value = InValue;
		HandleOnValueChanged(InValue);
	}
}

TSharedRef<SWidget> UAudioMaterialSlider::RebuildWidget()
{
	Slider = SNew(SAudioMaterialSlider)
		.Owner(this)
		.Orientation(Orientation)
		.AudioMaterialSliderStyle(&WidgetStyle)
		.OnValueChanged(BIND_UOBJECT_DELEGATE(FOnFloatValueChanged, HandleOnValueChanged));

	return Slider.ToSharedRef();
}

void UAudioMaterialSlider::HandleOnValueChanged(float InValue)
{
	OnValueChanged.Broadcast(InValue);
}

#undef LOCTEXT_NAMESPACE
