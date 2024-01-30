// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaDefs.h"
#include "GameFramework/Actor.h"
#include "Templates/SharedPointerFwd.h"

class AActor;
class UClass;

struct AVALANCHELEVELVIEWPORT_API FAvaViewportColorPickerActorAdapter
{
	virtual ~FAvaViewportColorPickerActorAdapter() = default;

	virtual FAvaColorChangeData GetColorData(const AActor* InActor) const;
	virtual void SetColorData(AActor* InActor, const FAvaColorChangeData& InColorData) const;
};

template<typename InType>
struct TAvaViewportColorPickerActorAdapter : public FAvaViewportColorPickerActorAdapter
{
	virtual ~TAvaViewportColorPickerActorAdapter() = default;

	virtual FAvaColorChangeData GetColorData(const AActor* InActor) const override
	{
		if (const InType* CastActor = Cast<InType>(InActor))
		{
			return CastActor->GetColorData();
		}

		return FAvaColorChangeData();
	}

	virtual void SetColorData(AActor* InActor, const FAvaColorChangeData& InColorData) const override
	{
		if (InType* CastActor = Cast<InType>(InActor))
		{
			CastActor->SetColorData(InColorData);
		}
	}
};

template<typename InComponentType>
struct TAvaViewportColorPickerActorComponentAdapter : public FAvaViewportColorPickerActorAdapter
{
	virtual ~TAvaViewportColorPickerActorComponentAdapter() = default;

	virtual FAvaColorChangeData GetColorData(const AActor* InActor) const override
	{
		if (IsValid(InActor))
		{
			if (InComponentType* Component = InActor->FindComponentByClass<InComponentType>())
			{
				return Component->GetColorData();
			}
		}

		return FAvaColorChangeData();
	}

	virtual void SetColorData(AActor* InActor, const FAvaColorChangeData& InColorData) const override
	{
		if (IsValid(InActor))
		{
			if (InComponentType* Component = InActor->FindComponentByClass<InComponentType>())
			{
				Component->SetColorData(InColorData);
			}
		}
	}
};

struct AVALANCHELEVELVIEWPORT_API FAvaViewportColorPickerActorClassRegistry
{
	static void RegisterClassAdapter(UClass* InClass, const TSharedRef<FAvaViewportColorPickerActorAdapter>& InAdapter);

	static bool ApplyColorDataToActor(AActor* InActor, const FAvaColorChangeData& InColorData);

	static bool GetColorDataFromActor(const AActor* InActor, FAvaColorChangeData& OutColorData);
};
