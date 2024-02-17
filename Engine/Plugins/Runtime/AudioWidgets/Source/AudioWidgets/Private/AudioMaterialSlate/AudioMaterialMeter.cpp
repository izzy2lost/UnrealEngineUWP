// Copyright Epic Games, Inc. All Rights Reserved.


#include "AudioMaterialSlate/AudioMaterialMeter.h"
#include "AudioMaterialSlate/AudioMaterialSlateStyles.h"
#include "AudioMaterialSlate/SAudioMaterialMeter.h"
#include "Widgets/SWeakWidget.h"
#include "Components/AudioComponent.h"

#define LOCTEXT_NAMESPACE "AudioWidgets"

#if WITH_EDITOR
const FText UAudioMaterialMeter::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "AudioMaterial");
}
#endif

void UAudioMaterialMeter::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (!Meter.IsValid())
	{
		return;
	}

	Meter->ApplyNewMaterial();
	Meter->SetValue(MeterValue);
}

void UAudioMaterialMeter::ReleaseSlateResources(bool bReleaseChildren)
{
	Meter.Reset();
}

float UAudioMaterialMeter::GetValue() const
{
	return MeterValue;
}

void UAudioMaterialMeter::SetValue(float InValue)
{
	if (Meter.IsValid())
	{
		Meter->SetValue(InValue);
	}

	if (MeterValue != InValue)
	{
		MeterValue = InValue;
		HandleOnValueChanged(InValue);
	}
}

TSharedRef<SWidget> UAudioMaterialMeter::RebuildWidget()
{
	Meter = SNew(SAudioMaterialMeter)
		.Owner(this)
		.AudioMaterialMeterStyle(&WidgetStyle)
		.ValueAttribute(MeterValue)
		.OnValueChanged(BIND_UOBJECT_DELEGATE(FOnFloatValueChanged, HandleOnValueChanged));

	return Meter.ToSharedRef();
}

void UAudioMaterialMeter::HandleOnValueChanged(float InValue)
{
	OnValueChanged.Broadcast(InValue);
}

#undef LOCTEXT_NAMESPACE
