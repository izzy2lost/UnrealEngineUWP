// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/MultiUserReplicationSettings.h"

#include "LogMultiUserReplicationEditor.h"
#include "Replication/PropertyChainUtils.h"
#include "Settings/DefaultPropertySelection.h"

#include "Algo/IndexOf.h"

namespace UE::MultiUserReplicationEditor::Private
{
	static void ApplyDefaultPropertySelection(FReplicatedObjectInfo& Info, const FDefaultPropertySelection& Selection, UStruct& Class)
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
	FDefaultPropertySelection* DefaultProperties = nullptr;
	for (; Current && !DefaultProperties; Current = Current->GetSuperClass())
	{
		DefaultProperties = DefaultPropertySelection.Find(Current);
		if (!DefaultProperties)
		{
			continue;
		}
			
		UE::MultiUserReplicationEditor::Private::ApplyDefaultPropertySelection(Info, *DefaultProperties, *Current);
		// Recurse super structs
		if (UClass* Parent = Current->GetSuperClass()
			; Parent && DefaultProperties->bInheritFromBase)
		{
			AddDefaultPropertiesFromSettings(Info, *Parent);
		}
	}
}
