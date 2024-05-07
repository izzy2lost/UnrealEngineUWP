// Copyright Epic Games, Inc. All Rights Reserved.

#include "Persistence/BuilderPersistenceManager.h"

TObjectPtr<UBuilderPersistenceManager> UBuilderPersistenceManager::Instance = nullptr;

namespace UE::DisplayBuilders::BuilderPersistenceManager
{
	FName FavoritesSuffix = "BuilderFavorites";
	FName ShowButtonLabelsSuffix = "ShowButtonLabels";
}

void UBuilderPersistenceManager::Initialize()
{
	if(!Instance)
	{
		Instance = NewObject<UBuilderPersistenceManager>(); 
		Instance->AddToRoot();
	}
}

void UBuilderPersistenceManager::ShutDown()
{
	if ( UObjectInitialized() )
	{
		Instance->RemoveFromRoot();
	}
	Instance = nullptr;
}

TArray<FName> UBuilderPersistenceManager::GetPersistedFavoritesNamesArray(const UE::DisplayBuilders::FBuilderKey& Key)
{
	return GetPersistedArrayOfNames( Key, UE::DisplayBuilders::BuilderPersistenceManager::FavoritesSuffix );
}

void UBuilderPersistenceManager::SetPersistedFavoritesNamesArray(const UE::DisplayBuilders::FBuilderKey& Key, TArray<FName>& Favorites)
{
	return PersistArrayOfNames(Key, UE::DisplayBuilders::BuilderPersistenceManager::FavoritesSuffix, Favorites);
}

TArray<FName> UBuilderPersistenceManager::GetPersistedArrayOfNames( const UE::DisplayBuilders::FBuilderKey& Key, FName PersistenceKeySuffix )
{
	LoadEditorConfig();
	if ( !Key.IsNone()  && !PersistenceKeySuffix.IsNone() )
	{
		if ( const FPersistedNameArray* Settings =  SavedNameToPersistedFNameArrayMap.Find( Key.GetKeyWithSuffix( PersistenceKeySuffix ) ) )
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

	FPersistedNameArray& Settings =  SavedNameToPersistedFNameArrayMap.Add( Key.GetKeyWithSuffix( PersistenceKeySuffix ) );

	Settings.ArrayOfNamesToPersist = ArrayOfNamesToPersist;;
	SaveEditorConfig();
}

bool UBuilderPersistenceManager::GetPersistedBool( const UE::DisplayBuilders::FBuilderKey& Key, FName PersistenceKeySuffix, bool PersistedBoolIfNoneFound )
{
	LoadEditorConfig();
	if ( !Key.IsNone()  && !PersistenceKeySuffix.IsNone() )
	{
		if ( const FPersistedBool* BoolSettings =  SavedNameToPersistedBoolMap.Find( Key.GetKeyWithSuffix( PersistenceKeySuffix ) ) )
		{
			return BoolSettings->PersistedBool;
		}
		else
		{
			PersistBool( Key, PersistenceKeySuffix, PersistedBoolIfNoneFound );
		}
	}

	return PersistedBoolIfNoneFound;
}

void UBuilderPersistenceManager::SetPersistedButtonLabelBool( const UE::DisplayBuilders::FBuilderKey& Key, bool InPersistedBool )
{
	return PersistBool( Key, UE::DisplayBuilders::BuilderPersistenceManager::ShowButtonLabelsSuffix, InPersistedBool );
}

bool UBuilderPersistenceManager::GetPersistedButtonLabelBool( const UE::DisplayBuilders::FBuilderKey& Key, bool PersistedBoolIfNoneFound )
{
	return GetPersistedBool( Key, UE::DisplayBuilders::BuilderPersistenceManager::ShowButtonLabelsSuffix, PersistedBoolIfNoneFound );
}

void UBuilderPersistenceManager::PersistBool( const UE::DisplayBuilders::FBuilderKey& Key, FName PersistenceKeySuffix, bool InPersistedBool )
{
	if (PersistenceKeySuffix.IsNone())
	{
		return;
	}

	FPersistedBool& Settings =  SavedNameToPersistedBoolMap.Add( Key.GetKeyWithSuffix( PersistenceKeySuffix ) );
	Settings.PersistedBool = InPersistedBool;;
	SaveEditorConfig();
}
