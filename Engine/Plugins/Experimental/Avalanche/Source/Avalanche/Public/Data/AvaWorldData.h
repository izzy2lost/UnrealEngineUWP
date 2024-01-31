// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaActorData.h"
#include "AvaSubObjectData.h"
#include "AvaVersionInfo.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "UObject/NameTypes.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/WeakObjectPtr.h"
#include "AvaWorldData.generated.h"

class AActor;
class UObject;
class UWorld;

USTRUCT()
struct FAvaWorldData
{
	GENERATED_BODY()
	
	/**
	 *	Called after saving the world to perform final processing.
	 */
	void FinalizeSave();
	
	/**
	 * Resets the Transient Members to zero, since DuplicateTransient only works on UCLASS not USTRUCT.
	 */
	void ResetTransientData();
	
	/**
	* Holds the Version Info this World Data was last serialized with
	*/
	UPROPERTY()
	FAvaVersionInfo VersionInfo;

	/**
	* Transient World used to load the World Data in
	*/
	UPROPERTY(Transient)
	TWeakObjectPtr<UWorld> World;

	/**
	* Holds serialized actor data.
	* Maps the original actor's path to its serialized data.
	*/
	UPROPERTY()
	TMap<FSoftObjectPath, FAvaActorData> ActorData;

	UPROPERTY()
	TArray<FName> SerializedNames;

	/* 
	 * List of all Object References in the World
	 * This is an Untranslated version so we ned to change the World Name Part (when it was serialized)
	 * of the Path to the New World Name
	 */
	UPROPERTY()
	TArray<FSoftObjectPath> SerializedObjectReferences;

	/*
	 * Key: Index to SerializedObjectReferences List
	 * Value: SubObject Information for the Object Reference
	 * (This Container effectively only contains SubObjects so there will be indices not found here)
	 */
	UPROPERTY()
	TMap<FAvaObjectIndex, FAvaSubObjectData> SubObjects;

	/** Binds every entry in SerializedNames to its index to speed up look up */
	UPROPERTY(Transient)
	TMap<FName, FAvaObjectIndex> NameIndexMap;

	/** Binds every entry in SerializedObjectReferences to its index to speed up look up */
	UPROPERTY(Transient)
	TMap<FSoftObjectPath, FAvaObjectIndex> ObjectReferenceIndexMap;

	UPROPERTY(Transient)
	TMap<FSoftObjectPath, TWeakObjectPtr<AActor>> CachedActors;

	UPROPERTY(Transient)
	TMap<FSoftObjectPath, TWeakObjectPtr<UObject>> CachedSubObjects;
};
