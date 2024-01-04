// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Settings/ConcertSubobjectMatchingRules.h"

#include "Components/ActorComponent.h"
#include "Internationalization/Regex.h"

namespace UE::ConcertClientSharedSlate
{
	static bool MatchesAnyRegex(const FString& Input, const TSet<FString>& AllRegex)
	{
		for (const FString& RegexString : AllRegex)
		{
			const FRegexPattern Pattern(RegexString);
			FRegexMatcher Matcher(Pattern, Input);
			if (Matcher.FindNext())
			{
				return true;
			}
		}

		return false;
	}
}

void FConcertSubobjectMatchingRules::MatchToSubobjectsIn(const UObject& AddedObject, TFunctionRef<void(UObject&)> OnSubobjectMatched) const
{
	// Do not search recursively so FurtherObjectsCallback can decide to call AddAdditionalObjectsFromSettings again on the newly added objects.
	constexpr bool bIncludeNested = false;
	ForEachObjectWithOuter(&AddedObject, [this, &OnSubobjectMatched](UObject* Subobject)
	{
		// Has exclusion regex?
		const bool bShouldExclude = UE::ConcertClientSharedSlate::MatchesAnyRegex(Subobject->GetName(), ExcludeSubobjectRegex);
		if (bShouldExclude)
		{
			return;
		}

		// Was told to add all UActorComponents?
		const bool bIncludeAllSubobjects = IncludeAllOption == EConcertIncludeAllSubobjectsType::AllSubobjects;
		const bool bIncludeDueToComponent = Subobject->IsA(UActorComponent::StaticClass())
			&& IncludeAllOption == EConcertIncludeAllSubobjectsType::AllComponents;
		if (bIncludeAllSubobjects || bIncludeDueToComponent)
		{
			OnSubobjectMatched(*Subobject);
			return;
		}

		// Has configured class?
		UClass* CurrentClass = Subobject->GetClass();
		for (; CurrentClass; CurrentClass = CurrentClass->GetSuperClass())
		{
			if (IncludeClasses.Contains(Subobject->GetClass()))
			{
				OnSubobjectMatched(*Subobject);
				return;
			}
		}

		// Has inclusion regex?
		const bool bShouldIncludeByRegex = UE::ConcertClientSharedSlate::MatchesAnyRegex(Subobject->GetName(), IncludeSubobjectRegex);
		if (bShouldIncludeByRegex)
		{
			OnSubobjectMatched(*Subobject);
		}
	}, bIncludeNested);
}