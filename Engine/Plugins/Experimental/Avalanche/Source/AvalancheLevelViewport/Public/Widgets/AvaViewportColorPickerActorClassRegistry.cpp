// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/AvaViewportColorPickerActorClassRegistry.h"
#include "AvaDefs.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"

namespace UE::AvalancheLevelViewport::Private
{
	FAvaColorChangeData DefaultColorData;
	TMap<UClass*, TSharedRef<FAvaViewportColorPickerActorAdapter>> Adapters;

	TSharedPtr<FAvaViewportColorPickerActorAdapter> FindAdapterForClass(UClass* InClass)
	{
		if (!InClass)
		{
			return nullptr;
		}

		for (UClass* Class = InClass; Class != AActor::StaticClass(); Class = Class->GetSuperClass())
		{
			if (const TSharedRef<FAvaViewportColorPickerActorAdapter>* Adapter = Adapters.Find(Class))
			{
				return *Adapter;
			}
		}

		return nullptr;
	}
}

FAvaColorChangeData FAvaViewportColorPickerActorAdapter::GetColorData(const AActor* InActor) const
{
	using namespace UE::AvalancheLevelViewport::Private;

	return DefaultColorData;
}

void FAvaViewportColorPickerActorAdapter::SetColorData(AActor* InActor, const FAvaColorChangeData& InColorData) const
{
}

void FAvaViewportColorPickerActorClassRegistry::RegisterClassAdapter(UClass* InClass, 
	const TSharedRef<FAvaViewportColorPickerActorAdapter>& InAdapter)
{
	using namespace UE::AvalancheLevelViewport::Private;

	Adapters.Add(InClass, InAdapter);
}

bool FAvaViewportColorPickerActorClassRegistry::ApplyColorDataToActor(AActor* InActor, const FAvaColorChangeData& InColorData)
{
	if (!InActor)
	{
		return false;
	}

	using namespace UE::AvalancheLevelViewport::Private;

	if (TSharedPtr<FAvaViewportColorPickerActorAdapter> Adapter = FindAdapterForClass(InActor->GetClass()))
	{
		Adapter->SetColorData(InActor, InColorData);
		return true;
	}

	return false;
}

bool FAvaViewportColorPickerActorClassRegistry::GetColorDataFromActor(const AActor* InActor, FAvaColorChangeData& OutColorData)
{
	if (!InActor)
	{
		return false;
	}

	using namespace UE::AvalancheLevelViewport::Private;

	if (TSharedPtr<FAvaViewportColorPickerActorAdapter> Adapter = FindAdapterForClass(InActor->GetClass()))
	{
		OutColorData = Adapter->GetColorData(InActor);
		return true;
	}

	return false;
}
