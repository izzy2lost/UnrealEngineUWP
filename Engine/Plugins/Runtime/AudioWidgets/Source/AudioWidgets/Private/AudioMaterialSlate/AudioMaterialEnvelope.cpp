// Copyright Epic Games, Inc. All Rights Reserved.


#include "AudioMaterialSlate/AudioMaterialEnvelope.h"
#include "AudioMaterialSlate/AudioMaterialSlateStyles.h"
#include "AudioMaterialSlate/SAudioMaterialEnvelope.h"
#include "Widgets/SWeakWidget.h"
#include "Components/AudioComponent.h"

#define LOCTEXT_NAMESPACE "AudioWidgets"

#if WITH_EDITOR
const FText UAudioMaterialEnvelope::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "AudioMaterial");
}
#endif

void UAudioMaterialEnvelope::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (!EnvelopeCurve.IsValid())
	{
		return;
	}

	EnvelopeCurve->ApplyNewMaterial();
}

void UAudioMaterialEnvelope::ReleaseSlateResources(bool bReleaseChildren)
{
	EnvelopeCurve.Reset();
}

TSharedRef<SWidget> UAudioMaterialEnvelope::RebuildWidget()
{
	EnvelopeCurve = SNew(SAudioMaterialEnvelope)
		.Owner(this)
		.AudioMaterialEnvelopeStyle(&WidgetStyle)
		.EnvelopeSettings(&EnvelopeSettings);

	return EnvelopeCurve.ToSharedRef();
}

#undef LOCTEXT_NAMESPACE
