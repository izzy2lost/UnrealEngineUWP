// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/MultiUserReplicationSettings.h"

#include "LogMultiUserReplicationEditor.h"
#include "Replication/PropertyChainUtils.h"
#include "Settings/MultiUserDefaultPropertySelection.h"
#include "Settings/MultiUserDefaultSubobjectSelection.h"

#include "Algo/IndexOf.h"
#include "Components/ActorComponent.h"
#include "Internationalization/Regex.h"

namespace UE::MultiUserReplicationEditor::DefaultProperties
{
	static void ApplyDefaultPropertySelection(FReplicatedObjectInfo& Info, const FMultiUserDefaultPropertySelection& Selection, UStruct& Class)
	{
		// Preparse the the FStrings into paths
		TArray<TArray<FName>> Paths;
		Paths.Reserve(Selection.DefaultSelectedProperties.Num());
		for (const FString& StringPath : Selection.DefaultSelectedProperties)
		{
			TArray<FString> Parts;
			StringPath.ParseIntoArray(Parts, TEXT("."));
			TArray<FName> NameArray;
			Algo::Transform(Parts, NameArray, [](const FString& String){ return *String; });
			Paths.Emplace(MoveTemp(NameArray));
		}

		// This walks through the entire property hierarchy once
		using namespace ConcertSyncCore::PropertyChain;
		BulkConstructConcertChainsFromPaths(Class, Paths.Num(), [&Info, &Paths](const FArchiveSerializedPropertyChain& Chain, const FProperty& LeafProperty)
		{
			const int32 IndexOfMatches = Algo::IndexOfByPredicate(Paths, [&Chain, &LeafProperty](const TArray<FName>& Path){ return DoPathAndChainsMatch(Path, Chain, LeafProperty); });
			const bool bMatches = IndexOfMatches != INDEX_NONE;
			if (bMatches)
			{
				Info.PropertySelection.ReplicatedProperties.Emplace(&Chain, LeafProperty);
				Paths.RemoveAtSwap(IndexOfMatches);
			}
			return true;
		});

		// Warn the user of unmatched properties so they can de-clutter their settings of useless entries
		for (const TArray<FName>& Path : Paths)
		{
			UE_LOG(LogMultiUserReplicationEditor, Warning,
				TEXT("Path %s was unmatched. Please fix your settings under MultiUserReplicationEditor > DefaultPropertySelection > %s"),
				*FString::JoinBy(Path, TEXT("."), [](const FName& Name){ return Name.ToString(); }),
				*Class.GetName()
				);
		}
	}
}

void UMultiUserReplicationSettings::AddDefaultPropertiesFromSettings(FReplicatedObjectInfo& Info, UClass& Class)
{
	// Find the most specialized class properties
	UClass* Current = &Class;
	FMultiUserDefaultPropertySelection* DefaultProperties = nullptr;
	for (; Current && !DefaultProperties; Current = Current->GetSuperClass())
	{
		DefaultProperties = DefaultPropertySelection.Find(Current);
		if (!DefaultProperties)
		{
			continue;
		}
			
		UE::MultiUserReplicationEditor::DefaultProperties::ApplyDefaultPropertySelection(Info, *DefaultProperties, *Current);
		// Recurse super structs
		if (UClass* Parent = Current->GetSuperClass()
			; Parent && DefaultProperties->bInheritFromBase)
		{
			AddDefaultPropertiesFromSettings(Info, *Parent);
		}
	}
}

namespace UE::MultiUserReplicationEditor::DefaultSubobjects
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
	
	static void ApplyDefaultSubobjectSelection(UObject& AddedObject, const FMultiUserDefaultSubobjectSelection& Selection, TFunctionRef<void(UObject&)> FurtherObjectsCallback)
	{
		// Do not search recursively so FurtherObjectsCallback can decide to call AddAdditionalObjectsFromSettings again on the newly added objects.
		constexpr bool bIncludeNested = false;
		ForEachObjectWithOuter(&AddedObject, [&Selection, &FurtherObjectsCallback](UObject* Subobject)
		{
			// Has exclusion regex?
			const bool bShouldExclude = MatchesAnyRegex(Subobject->GetName(), Selection.ExcludeSubobjectRegex);
			if (bShouldExclude)
			{
				return;
			}

			// Was told to add all UActorComponents?
			const bool bIncludeAllSubobjects = Selection.IncludeAllOption == EMultiUserIncludeAllSubobjectsType::AllSubobjects;
			const bool bIncludeDueToComponent = Subobject->IsA(UActorComponent::StaticClass())
				&& Selection.IncludeAllOption == EMultiUserIncludeAllSubobjectsType::AllComponents;
			if (bIncludeAllSubobjects || bIncludeDueToComponent)
			{
				FurtherObjectsCallback(*Subobject);
				return;
			}

			// Has configured class?
			UClass* CurrentClass = Subobject->GetClass();
			for (; CurrentClass; CurrentClass = CurrentClass->GetSuperClass())
			{
				if (Selection.IncludeClasses.Contains(Subobject->GetClass()))
				{
					FurtherObjectsCallback(*Subobject);
					return;
				}
			}

			// Has inclusion regex?
			const bool bShouldIncludeByRegex = MatchesAnyRegex(Subobject->GetName(), Selection.IncludeSubobjectRegex);
			if (bShouldIncludeByRegex)
			{
				FurtherObjectsCallback(*Subobject);
			}
		}, bIncludeNested);
	}

	static void InternalAddAdditionalObjectsFromSettings(UClass& StartClass, UMultiUserReplicationSettings& Settings, UObject& AddedObject, TFunctionRef<void(UObject&)> FurtherObjectsCallback)
	{
		// Find the most specialized class properties
		UClass* Current = &StartClass;
		FMultiUserDefaultSubobjectSelection* DefaultSubobjects = nullptr;
		for (; Current && !DefaultSubobjects; Current = Current->GetSuperClass())
		{
			DefaultSubobjects = Settings.DefaultSubobjectSelection.Find(Current);
			if (!DefaultSubobjects)
			{
				continue;
			}
			
			ApplyDefaultSubobjectSelection(AddedObject, *DefaultSubobjects, FurtherObjectsCallback);
			// Recurse super structs
			if (UClass* Parent = Current->GetSuperClass()
				; Parent && DefaultSubobjects->bInheritFromBase)
			{
				InternalAddAdditionalObjectsFromSettings(*Parent, Settings, AddedObject, FurtherObjectsCallback);
			}
		}
	}
}

void UMultiUserReplicationSettings::AddAdditionalObjectsFromSettings(UObject& AddedObject, TFunctionRef<void(UObject&)> FurtherObjectsCallback)
{
	UE::MultiUserReplicationEditor::DefaultSubobjects::InternalAddAdditionalObjectsFromSettings(*AddedObject.GetClass(), *this, AddedObject, FurtherObjectsCallback);
}
