// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "EditorConfigBase.h"
#include "BuilderKey.h"
#include "UObject/ObjectPtr.h"

#include "BuilderPersistenceManager.generated.h"

/**
 * Manages an array of FNames to persist
 */
USTRUCT()
struct WIDGETREGISTRATION_API FPersistedNameArray
{
	GENERATED_BODY()
public:
	
	UPROPERTY()
	TArray<FName> ArrayOfNamesToPersist;
};


/**
 * The Builder Persistence Manager handles persistence for Builders through use of FBuilderKeys
 */
UCLASS(EditorConfig="BuilderPersistenceManager")
class WIDGETREGISTRATION_API  UBuilderPersistenceManager : public UEditorConfigBase
{
	GENERATED_BODY()

public:
	/**
	 * @return the default named favorites array for the Builder with FBuilderKey Key, if one exists, else an empty array is returned
	 * 
	 * @param Key the FBuilderKey to retrieve the favorites for
	 */
	TArray<FName> GetPersistedFavoritesNamesArray( const UE::DisplayBuilders::FBuilderKey& Key  );

	/**
	 * Sets the default named favorites array for the Builder with FBuilderKey Key
	 * 
	 * @param Key the FBuilderKey to retrieve the favorites for
	 * @param Favorites the array of FNames for the favorites to persist
	 */
	void SetPersistedFavoritesNamesArray( const UE::DisplayBuilders::FBuilderKey& Key, TArray<FName>& Favorites );

	/**
	 * @return the an array for the Builder with FBuilderKey Key and the suffix that the array was persisted with, if one exists, else an empty array is returned
	 * 
	 * @param Key the FBuilderKey to retrieve the FNames array for
	 * @param PersistenceKeySuffix the suffix to add to the FBuilderKey to persist the array of FNames with
	 */
	TArray<FName> GetPersistedArrayOfNames( const UE::DisplayBuilders::FBuilderKey& Key, FName PersistenceKeySuffix );

	/**
	 * Persists the array of FNames the Builder with FBuilderKey Key and suffix PersistenceKeySuffix
	 * 
	 * @param Key the FBuilderKey to retrieve the favorites for
	 * @param PersistenceKeySuffix the suffix added to the FBuilderKey to persist the array of FNames with
	 */
	void PersistArrayOfNames( const UE::DisplayBuilders::FBuilderKey& Key, FName PersistenceKeySuffix, TArray<FName>& ArrayOfNamesToPersist );

	/**
	 * Initialize the Persistence manager
	 */
	static void Initialize();
	
	/**
	* Shuts down the Persistence manager
	*/
	static void ShutDown();

	/**
	 * Gets the singleton for the Builder Persistence Manager
	 */
	static UBuilderPersistenceManager* Get()
	{
		return Instance;
	}


private:

	UPROPERTY(meta=(EditorConfig))
	TMap<FString, FPersistedNameArray> PersistenceSettings;

	static FName FavoritesSuffix;
	
	static TObjectPtr<UBuilderPersistenceManager> Instance;
};