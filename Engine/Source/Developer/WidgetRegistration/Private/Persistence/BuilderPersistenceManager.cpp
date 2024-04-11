// Copyright Epic Games, Inc. All Rights Reserved.

#include "Persistence/BuilderPersistenceManager.h"

TObjectPtr<UBuilderPersistenceManager> UBuilderPersistenceManager::Instance = nullptr;

FName UBuilderPersistenceManager::FavoritesSuffix = "BuilderFavorites";

void UBuilderPersistenceManager::Initialize()
{
	if(!Instance)
	{
		Instance = NewObject<UBuilderPersistenceManager>(); 
		Instance->AddToRoot();
		Instance->LoadEditorConfig();
	}
}

void UBuilderPersistenceManager::ShutDown()
{
	Instance->RemoveFromRoot();
	Instance = nullptr;
}

TArray<FName> UBuilderPersistenceManager::GetPersistedFavoritesNamesArray(const UE::DisplayBuilders::FBuilderKey& Key)
{
	return GetPersistedArrayOfNames(Key, FavoritesSuffix);
}

void UBuilderPersistenceManager::SetPersistedFavoritesNamesArray(const UE::DisplayBuilders::FBuilderKey& Key, TArray<FName>& Favorites)
{
	return PersistArrayOfNames(Key, FavoritesSuffix, Favorites);
}

TArray<FName> UBuilderPersistenceManager::GetPersistedArrayOfNames( const UE::DisplayBuilders::FBuilderKey& Key, FName PersistenceKeySuffix )
{
	if ( !Key.IsNone()  && !PersistenceKeySuffix.IsNone() )
	{
		if ( const FPersistedNameArray* Settings =  PersistenceSettings.Find( Key.GetKeyWithSuffix( PersistenceKeySuffix ) ) )
		{
			return Settings->ArrayOfNamesToPersist;
		}
	}

	return {};
}

void UBuilderPersistenceManager::PersistArrayOfNames( const UE::DisplayBuilders::FBuilderKey& Key, FName PersistenceKeySuffix, TArray<FName>& ArrayOfNamesToPersist )
{
	if (PersistenceKeySuffix.IsNone())
	{
		return;
	}

	FPersistedNameArray& Settings =  PersistenceSettings.Add( Key.GetKeyWithSuffix( PersistenceKeySuffix ) );

	Settings.ArrayOfNamesToPersist = ArrayOfNamesToPersist;;
	SaveEditorConfig();
}