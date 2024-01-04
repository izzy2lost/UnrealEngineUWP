// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Settings/ConcertPerClassSubobjectMatchingRules.h"

#include "UObject/Class.h"

namespace UE::ConcertClientSharedSlate::DefaultSubobjects
{
	static void InternalAddAdditionalObjectsFromSettings(UClass& StartClass, const FConcertPerClassSubobjectMatchingRules& Settings, const UObject& AddedObject, TFunctionRef<void(UObject&)> OnSubobjectMatched)
	{
		// Find the most specialized class properties
		UClass* Current = &StartClass;
		const FConcertInheritableSubobjectMatchingRules* DefaultSubobjects = nullptr;
		for (; Current && !DefaultSubobjects; Current = Current->GetSuperClass())
		{
			DefaultSubobjects = Settings.SubobjectMatchingRules.Find(Current);
			if (!DefaultSubobjects)
			{
				continue;
			}
			
			DefaultSubobjects->MatchToSubobjectsIn(AddedObject, OnSubobjectMatched);
			// Recurse super structs
			if (UClass* Parent = Current->GetSuperClass()
				; Parent && DefaultSubobjects->bInheritFromBase)
			{
				InternalAddAdditionalObjectsFromSettings(*Parent, Settings, AddedObject, OnSubobjectMatched);
			}
		}
	}
}

void FConcertPerClassSubobjectMatchingRules::MatchSubobjectsRecursivelyFor(const UObject& Object, TFunctionRef<void(UObject&)> OnSubobjectMatched) const
{
	checkf(!Object.IsA<UClass>(), TEXT("Pass in the UObject instanced directly, not its class!"));
	UE::ConcertClientSharedSlate::DefaultSubobjects::InternalAddAdditionalObjectsFromSettings(*Object.GetClass(), *this, Object, OnSubobjectMatched);
}