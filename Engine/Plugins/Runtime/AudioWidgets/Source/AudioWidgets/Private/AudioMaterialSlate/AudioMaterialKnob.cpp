// Copyright Epic Games, Inc. All Rights Reserved.


#include "AudioMaterialSlate/AudioMaterialKnob.h"
#include "AudioMaterialSlate/SAudioMaterialKnob.h"

#define LOCTEXT_NAMESPACE "AudioWidgets"
#if WITH_EDITOR
const FText UAudioMaterialKnob::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "AudioMaterial");
}
#endif // WITH_EDITOR

void UAudioMaterialKnob::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (!Knob.IsValid())
	{
		return;
	}

	Knob->SetValue(Value);
	Knob->ApplyNewMaterial();
}

void UAudioMaterialKnob::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Knob.Reset();
}

float UAudioMaterialKnob::GetValue()
{
	return Value;
}

void UAudioMaterialKnob::SetValue(float InValue)
{
	InValue = FMath::Clamp(InValue, 0.f, 1.f);

	if (Knob.IsValid())
	{
		Knob->SetValue(InValue);
	}

	if (Value != InValue)
	{
		Value = InValue;
		HandleOnKnobValueChanged(InValue);
	}
}

TSharedRef<SWidget> UAudioMaterialKnob::RebuildWidget()
{
	Knob = SNew(SAudioMaterialKnob)
		.Owner(this)
		.AudioMaterialKnobStyle(&WidgetStyle)
		.Value(Value)
		.OnFloatValueChanged(BIND_UOBJECT_DELEGATE(FOnFloatValueChanged, HandleOnKnobValueChanged));
		
	return Knob.ToSharedRef();
}

void UAudioMaterialKnob::HandleOnKnobValueChanged(float InValue)
{
	Value = InValue;
	OnKnobValueChanged.Broadcast(InValue);	
}

#undef LOCTEXT_NAMESPACE
