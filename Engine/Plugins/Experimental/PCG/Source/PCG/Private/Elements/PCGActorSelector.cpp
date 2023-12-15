// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGActorSelector.h"

#include "PCGComponent.h"
#include "PCGModule.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"

#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGActorSelector)

namespace PCGActorSelector
{
	// Filter is required if it is not disabled and if we are gathering all world actors or gathering all children.
	bool FilterRequired(const FPCGActorSelectorSettings& InSettings)
	{
		return (InSettings.ActorFilter == EPCGActorFilter::AllWorldActors || InSettings.bIncludeChildren) && !InSettings.bDisableFilter;
	}

	// Need to pass a pointer of pointer to the found actor. The lambda will capture this pointer and modify its value when an actor is found.
	TFunction<bool(AActor*)> GetFilteringFunction(const FPCGActorSelectorSettings& InSettings, const TFunction<bool(const AActor*)>& BoundsCheck, const TFunction<bool(const AActor*)>& SelfIgnoreCheck, TArray<AActor*>& InFoundActors)
	{
		if (!FilterRequired(InSettings))
		{
			return [&InFoundActors, &BoundsCheck, &SelfIgnoreCheck](AActor* Actor) -> bool
			{
				if (BoundsCheck(Actor) && SelfIgnoreCheck(Actor))
				{
					InFoundActors.Add(Actor);
				}
				return true;
			};
		}

		const bool bMultiSelect = InSettings.bSelectMultiple;

		switch (InSettings.ActorSelection)
		{
		case EPCGActorSelection::ByTag:
			return[ActorSelectionTag = InSettings.ActorSelectionTag, &InFoundActors, bMultiSelect, &BoundsCheck, &SelfIgnoreCheck](AActor* Actor) -> bool
			{
				if (Actor && Actor->ActorHasTag(ActorSelectionTag) && BoundsCheck(Actor) && SelfIgnoreCheck(Actor))
				{
					InFoundActors.Add(Actor);
					return bMultiSelect;
				}

				return true;
			};

		case EPCGActorSelection::ByClass:
			return[ActorSelectionClass = InSettings.ActorSelectionClass, &InFoundActors, bMultiSelect, &BoundsCheck, &SelfIgnoreCheck](AActor* Actor) -> bool
			{
				if (Actor && Actor->IsA(ActorSelectionClass) && BoundsCheck(Actor) && SelfIgnoreCheck(Actor))
				{
					InFoundActors.Add(Actor);
					return bMultiSelect;
				}

				return true;
			};

		case EPCGActorSelection::ByName:
			UE_LOG(LogPCG, Error, TEXT("PCGActorSelector::GetFilteringFunction: Unsupported value for EPCGActorSelection - selection by name is no longer supported."));
			break;

		default:
			break;
		}

		return {};
	}

	TArray<AActor*> FindActors(const FPCGActorSelectorSettings& Settings, const UPCGComponent* InComponent, const TFunction<bool(const AActor*)>& BoundsCheck, const TFunction<bool(const AActor*)>& SelfIgnoreCheck)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGActorSelector::FindActor);

		UWorld* World = InComponent ? InComponent->GetWorld() : nullptr;
		AActor* Self = InComponent ? InComponent->GetOwner() : nullptr;

		TArray<AActor*> FoundActors;

		if (!World)
		{
			return FoundActors;
		}

		// Early out if we have not the information necessary.
		const bool bNoTagInfo = Settings.ActorSelection == EPCGActorSelection::ByTag && Settings.ActorSelectionTag == NAME_None;
		const bool bNoClassInfo = Settings.ActorSelection == EPCGActorSelection::ByClass && !Settings.ActorSelectionClass;

		if (FilterRequired(Settings) && (bNoTagInfo || bNoClassInfo))
		{
			return FoundActors;
		}

		// We pass FoundActor ref, that will be captured by the FilteringFunction
		// It will modify the FoundActor pointer to the found actor, if found.
		TFunction<bool(AActor*)> FilteringFunction = PCGActorSelector::GetFilteringFunction(Settings, BoundsCheck, SelfIgnoreCheck, FoundActors);

		if (!FilteringFunction)
		{
			return FoundActors;
		}

		// In case of iterating over all actors in the world, call our filtering function and get out.
		if (Settings.ActorFilter == EPCGActorFilter::AllWorldActors)
		{
			// A potential optimization if we know the sought actors are collide-able could be to obtain overlaps via a collision query.
			UPCGActorHelpers::ForEachActorInWorld<AActor>(World, FilteringFunction);

			// FoundActor is set by the FilteringFunction (captured)
			return FoundActors;
		}

		// Otherwise, gather all the actors we need to check
		TArray<AActor*> ActorsToCheck;
		switch (Settings.ActorFilter)
		{
		case EPCGActorFilter::Self:
			if (Self)
			{
				ActorsToCheck.Add(Self);
			}
			break;

		case EPCGActorFilter::Parent:
			if (Self)
			{
				if (AActor* Parent = Self->GetParentActor())
				{
					ActorsToCheck.Add(Parent);
				}
				else
				{
					// If there is no parent, set the owner as the parent.
					ActorsToCheck.Add(Self);
				}
			}
			break;

		case EPCGActorFilter::Root:
		{
			AActor* Current = Self;
			while (Current != nullptr)
			{
				AActor* Parent = Current->GetParentActor();
				if (Parent == nullptr)
				{
					ActorsToCheck.Add(Current);
					break;
				}
				Current = Parent;
			}

			break;
		}

		case EPCGActorFilter::Original:
		{
			APCGPartitionActor* PartitionActor = Cast<APCGPartitionActor>(Self);
			UPCGComponent* OriginalComponent = (PartitionActor && InComponent) ? PartitionActor->GetOriginalComponent(InComponent) : nullptr;
			AActor* OriginalActor = OriginalComponent ? OriginalComponent->GetOwner() : nullptr;
			if (OriginalActor)
			{
				ActorsToCheck.Add(OriginalActor);
			}
			else if (Self)
			{
				ActorsToCheck.Add(Self);
			}
		}

		default:
			break;
		}

		if (Settings.bIncludeChildren)
		{
			const int32 InitialCount = ActorsToCheck.Num();
			for (int32 i = 0; i < InitialCount; ++i)
			{
				ActorsToCheck[i]->GetAttachedActors(ActorsToCheck, /*bResetArray=*/ false, /*bRecursivelyIncludeAttachedActors=*/ true);
			}
		}

		for (AActor* Actor : ActorsToCheck)
		{
			// FoundActor is set by the FilteringFunction (captured)
			if (!FilteringFunction(Actor))
			{
				break;
			}
		}

		return FoundActors;
	}

	AActor* FindActor(const FPCGActorSelectorSettings& InSettings, UPCGComponent* InComponent, const TFunction<bool(const AActor*)>& BoundsCheck, const TFunction<bool(const AActor*)>& SelfIgnoreCheck)
	{
		// In order to make sure we don't try to select multiple, we'll do a copy of the settings here.
		FPCGActorSelectorSettings Settings = InSettings;
		Settings.bSelectMultiple = false;

		TArray<AActor*> Actors = FindActors(Settings, InComponent, BoundsCheck, SelfIgnoreCheck);
		return Actors.IsEmpty() ? nullptr : Actors[0];
	}
}

FPCGSelectionKey::FPCGSelectionKey(EPCGActorFilter InFilter)
{
	check(InFilter != EPCGActorFilter::AllWorldActors);
	ActorFilter = InFilter;
}

FPCGSelectionKey::FPCGSelectionKey(FName InTag)
{
	Selection = EPCGActorSelection::ByTag;
	Tag = InTag;
	ActorFilter = EPCGActorFilter::AllWorldActors;
}

FPCGSelectionKey::FPCGSelectionKey(TSubclassOf<UObject> InSelectionClass)
{
	Selection = EPCGActorSelection::ByClass;
	SelectionClass = InSelectionClass;
	ActorFilter = EPCGActorFilter::AllWorldActors;
}

FPCGSelectionKey FPCGSelectionKey::CreateFromPath(const FSoftObjectPath& InObjectPath)
{
	FPCGSelectionKey Key{};
	Key.Selection = EPCGActorSelection::ByPath;
	Key.ObjectPath = InObjectPath;
	Key.ActorFilter = EPCGActorFilter::AllWorldActors;

	return Key;
}

void FPCGSelectionKey::SetExtraDependency(const UClass* InExtraDependency)
{
	OptionalExtraDependency = InExtraDependency;
}

bool FPCGSelectionKey::IsMatching(const TSoftObjectPtr<UObject>& InObjectPtr, const UPCGComponent* InComponent) const
{
	if (InObjectPtr.IsNull())
	{
		return false;
	}

	const UObject* InObject = InObjectPtr.Get();
	
	// If we filter something else than all world actors, matching depends on the component.
	// Re-use the same mechanism than Get Actor Data, which should be cheap since we don't look for all actors in the world.
	if (ActorFilter != EPCGActorFilter::AllWorldActors)
	{
		const AActor* InActor = Cast<const AActor>(InObject);

		if (!InActor)
		{
			return false;
		}

		// InKey provide the info for selecting a given actor.
		// We reconstruct the selector settings from this key, and we also force it to SelectMultiple, since
		// we want to gather all the actors that matches this given key, to find if ours matches.
		FPCGActorSelectorSettings SelectorSettings = FPCGActorSelectorSettings::ReconstructFromKey(*this);
		SelectorSettings.bSelectMultiple = true;
		TArray<AActor*> AllActors = PCGActorSelector::FindActors(SelectorSettings, InComponent, [](const AActor*) { return true; }, [](const AActor*) { return true; });
		return AllActors.Contains(InActor);
	}

	switch (Selection)
	{
	case EPCGActorSelection::ByTag:
	{
		const AActor* InActor = Cast<const AActor>(InObject);
		return InActor && InActor->ActorHasTag(Tag);
	}
	case EPCGActorSelection::ByClass:
		return InObject && InObject->GetClass()->IsChildOf(SelectionClass);
	case EPCGActorSelection::ByPath:
		return InObjectPtr.ToSoftObjectPath() == ObjectPath;
	default:
		return false;
	}
}

bool FPCGSelectionKey::operator==(const FPCGSelectionKey& InOther) const
{
	if (ActorFilter != InOther.ActorFilter || Selection != InOther.Selection || OptionalExtraDependency != InOther.OptionalExtraDependency)
	{
		return false;
	}

	switch (Selection)
	{
	case EPCGActorSelection::ByTag:
		return Tag == InOther.Tag;
	case EPCGActorSelection::ByClass:
		return SelectionClass == InOther.SelectionClass;
	case EPCGActorSelection::ByPath:
		return ObjectPath == InOther.ObjectPath;
	case EPCGActorSelection::Unknown: // Fall-through
	case EPCGActorSelection::ByName:
		return true;
	default:
	{
		checkNoEntry();
		return true;
	}
	}
}

uint32 GetTypeHash(const FPCGSelectionKey& In)
{
	uint32 HashResult = HashCombine(GetTypeHash(In.ActorFilter), GetTypeHash(In.Selection));
	HashResult = HashCombine(HashResult, GetTypeHash(In.Tag));
	HashResult = HashCombine(HashResult, GetTypeHash(In.SelectionClass));
	HashResult = HashCombine(HashResult, GetTypeHash(In.OptionalExtraDependency));
	HashResult = HashCombine(HashResult, GetTypeHash(In.ObjectPath));

	return HashResult;
}

#if WITH_EDITOR
FText FPCGActorSelectorSettings::GetTaskNameSuffix() const
{
	if (ActorFilter == EPCGActorFilter::AllWorldActors)
	{
		if (ActorSelection == EPCGActorSelection::ByClass)
		{
			return FText::Format(FText::FromString(TEXT("Class: {0}")), (ActorSelectionClass.Get() ? ActorSelectionClass->GetDisplayNameText() : FText::FromName(NAME_None)));
		}
		else if (ActorSelection == EPCGActorSelection::ByTag)
		{
			return FText::Format(FText::FromString(TEXT("Tag: {0}")), FText::FromName(ActorSelectionTag));
		}
	}
	else if(const UEnum* EnumPtr = StaticEnum<EPCGActorFilter>())
	{
		return EnumPtr->GetDisplayNameTextByValue(static_cast<__underlying_type(EPCGActorFilter)>(ActorFilter));
	}

	return FText();
}

FName FPCGActorSelectorSettings::GetTaskName(const FText& Prefix) const
{
	return FName(FText::Format(NSLOCTEXT("PCGActorSelectorSettings", "NodeTitleFormat", "{0} ({1})"), Prefix, GetTaskNameSuffix()).ToString());
}
#endif // WITH_EDITOR

FPCGSelectionKey FPCGActorSelectorSettings::GetAssociatedKey() const
{
	if (ActorFilter != EPCGActorFilter::AllWorldActors)
	{
		return FPCGSelectionKey(ActorFilter);
	}

	switch (ActorSelection)
	{
	case EPCGActorSelection::ByTag:
		return FPCGSelectionKey(ActorSelectionTag);
	case EPCGActorSelection::ByClass:
		return FPCGSelectionKey(ActorSelectionClass);
	default:
		return FPCGSelectionKey();
	}
}

FPCGActorSelectorSettings FPCGActorSelectorSettings::ReconstructFromKey(const FPCGSelectionKey& InKey)
{
	if (InKey.SelectionClass && !InKey.SelectionClass->IsChildOf<AActor>())
	{
		return FPCGActorSelectorSettings{};
	}

	FPCGActorSelectorSettings Result{};
	Result.ActorFilter = InKey.ActorFilter;
	Result.ActorSelection = InKey.Selection;
	Result.ActorSelectionTag = InKey.Tag;
	Result.ActorSelectionClass = InKey.SelectionClass;

	return Result;
}