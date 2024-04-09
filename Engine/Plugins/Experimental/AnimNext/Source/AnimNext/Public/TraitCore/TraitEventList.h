// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TraitCore/TraitEvent.h"

namespace UE::AnimNext
{
	/**
	 * Trait Event List
	 * 
	 * Encapsulates a list of trait events.
	 */
	struct FTraitEventList
	{
		using IteratorType = TArray<FAnimNextTraitEventPtr>::RangedForIteratorType;
		using ConstIteratorType = TArray<FAnimNextTraitEventPtr>::RangedForConstIteratorType;

		FTraitEventList() = default;

		void Push(FAnimNextTraitEventPtr Event) { Events.Add(MoveTemp(Event)); }
		void Reset() { Events.Reset(); }
		int32 Num() const { return Events.Num(); }

		FAnimNextTraitEventPtr& operator[](int32 EventIndex) { return Events[EventIndex]; }
		const FAnimNextTraitEventPtr& operator[](int32 EventIndex) const { return Events[EventIndex]; }

		IteratorType begin() { return Events.begin(); }
		ConstIteratorType begin() const { return Events.begin(); }
		IteratorType end() { return Events.end(); }
		ConstIteratorType end() const { return Events.end(); }

		// Decrements the lifetime of every event within the list
		void DecrementLifetime()
		{
			for (FAnimNextTraitEventPtr& Event : Events)
			{
				Event->DecrementLifetime();
			}
		}

	private:
		// A list of events
		TArray<FAnimNextTraitEventPtr> Events;
	};
}
