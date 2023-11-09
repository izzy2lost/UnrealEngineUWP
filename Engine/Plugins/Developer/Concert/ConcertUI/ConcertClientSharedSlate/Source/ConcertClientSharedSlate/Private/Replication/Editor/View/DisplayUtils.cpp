// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/View/DisplayUtils.h"

#include "Replication/Editor/Model/IReplicationStreamModel.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "ConcertLogGlobal.h"
#include "GameFramework/Actor.h"
#include "Styling/SlateIconFinder.h"

namespace UE::ConcertClientSharedSlate::DisplayUtils
{
	FText GetObjectDisplayText(const FSoftObjectPath& Object)
	{
		return FText::FromString(GetObjectDisplayString(Object));
	}

	FString GetObjectDisplayString(const FSoftObjectPath& Object)
	{
		// Important! The object may not be loaded, yet. This could be if the asset is using a level that was not opened. 
		if (const UObject* LoadedObject = Object.ResolveObject())
		{
			return GetObjectDisplayString(*LoadedObject);
		}

		// Subpath looks like this PersistentLevel.Actor.Component
		const FString& Subpath = Object.GetSubPathString();
		const int32 LastDotIndex = Subpath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (LastDotIndex == INDEX_NONE)
		{
			return {};
		}
		return Subpath.RightChop(LastDotIndex + 1);
	}

	FString GetObjectDisplayString(const UObject& Object)
	{
		if (const AActor* AsActor = Cast<AActor>(&Object))
		{
			return AsActor->GetActorLabel();
		}
		return Object.GetName();
	}

	FText GetObjectTypeText(const IReplicationStreamModel& Model, const FSoftObjectPath& Object)
	{
		if (const FSoftClassPath ClassPath = Model.GetObjectClass(Object); ClassPath.IsValid())
		{
			// For C++ classes or loaded Blueprints
			if (UClass* Class = ClassPath.ResolveClass())
			{
				return Class->GetDisplayNameText();
			}

			// Assuming it is a Blueprint, try to get its display name from the registry tags
			const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
			
			const FAssetData BlueprintData = AssetRegistry.GetAssetByObjectPath(ClassPath);
			if (BlueprintData.IsValid())
			{
				FString DisplayName;
				return BlueprintData.GetTagValue(FBlueprintTags::BlueprintDisplayName, DisplayName)
					? FText::FromString(DisplayName)
					: FText::GetEmpty();
			}

			// TODO: Check redirectors
			// It was not a Blueprint - the class was most likely removed
			UE_LOG(LogConcert, Error, TEXT("Failed to get display name for unknown class \"%s\""), *ClassPath.ToString());
		}
		return FText::GetEmpty();
	}

	FSlateIcon GetObjectIcon(const IReplicationStreamModel& Model, const FSoftObjectPath& Object)
	{
		if (const FSoftClassPath ClassPath = Model.GetObjectClass(Object); ClassPath.IsValid())
		{
			UClass* Class = ClassPath.ResolveClass();
			return Class
				? FSlateIconFinder::FindIconForClass(Class)
				: FSlateIcon();
		}
		return FSlateIcon();
	}

	FSlateIcon GetObjectIcon(UObject& Object)
	{
		return FSlateIconFinder::FindIconForClass(Object.GetClass());
	}
	
	FText GetPropertyDisplayText(const FConcertPropertyChain& Property)
	{
		return FText::FromString(GetPropertyDisplayString(Property));
    }
	
	FString GetPropertyDisplayString(const FConcertPropertyChain& Property)
	{
		return Property.ToString(FConcertPropertyChain::EToStringMethod::LeafProperty);
	}
}
