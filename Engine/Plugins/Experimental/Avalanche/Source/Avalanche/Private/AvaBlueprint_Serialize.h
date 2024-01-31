// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/AvaDataDefines.h"

class AActor;
class FString;
class UActorComponent;
class UObject;
class UWorld;
struct FAvaActorData;
struct FAvaComponentData;
struct FAvaObjectData;
struct FAvaSubObjectData;
struct FAvaWorldData;
struct FSoftObjectPath;
template<typename OptionalType> struct TOptional;

struct FAvaBlueprint_Serialize
{
	/**
	 * Takes an existing path to an actor's subobjects and replaces the actor bit with the path to another actor.
	 * E.g. /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42.StaticMeshComponent could become /Game/MapName.MapName:PersistentLevel.SomeOtherActor.StaticMeshComponent
	 */
	static FSoftObjectPath SetActorInPath(AActor* NewActor, const FSoftObjectPath& OriginalObjectPath);

	static UObject* ResolveObjectDependency(FAvaWorldData& WorldData, FAvaObjectIndex ObjectIndex);

	static FString ExtractLastSubObjectName(const FSoftObjectPath& ObjectPath);

	static UObject* CreateSubObject(FAvaWorldData& WorldData, FAvaObjectIndex ObjectIndex, const FSoftObjectPath& PathToSubObject);

	static FString ExtractRootToLeafComponentPath(const FSoftObjectPath& ComponentPath);
	static UActorComponent* FindMatchingComponent(AActor* ActorToSearchOn, const FSoftObjectPath& ComponentPath);

	static void SaveObject(FAvaWorldData& WorldData, FAvaObjectData& ObjectData, UObject* Object);
	static void LoadObject(FAvaWorldData& WorldData, FAvaObjectData& ObjectData, UObject* Object);

	static bool ShouldSaveActor(const UWorld* InWorld, const AActor* InActor);
	static bool ShouldSaveComponent(const UActorComponent* InActorComponent);
	static bool ShouldSaveSubObject(UObject* InSubObject);

	static UActorComponent* FindOrAllocateComponent(const FAvaWorldData& InWorldData
		, AActor* InRecreatedActor
		, const FAvaActorData& InActorData
		, const FSoftObjectPath& InComponentPath
		, FAvaSubObjectData& SubObjectData
		, const FAvaComponentData& ComponentData);

	static FAvaObjectIndex AddOrFindObjectReference(FAvaWorldData& WorldData, const FSoftObjectPath& ObjectPath);
	static TOptional<FSoftObjectPath> ExtractActorFromPath(const FSoftObjectPath& InObjectPath, bool& bIsPathToActorSubObject);
	static void AddSubObjectDependency(FAvaWorldData& WorldData, UObject* ReferenceFromOriginalObject, FAvaObjectIndex Index);
	static FAvaObjectIndex AddObjectDependency(FAvaWorldData& WorldData
		, UObject* ReferenceFromOriginalObject
		, bool bCheckWhetherSubObject = true);

	static void LoadActor(AActor* InActor, const FSoftObjectPath& InActorPath, FAvaWorldData& WorldData);
	static TOptional<FAvaComponentData> SaveComponent(UActorComponent* InComponent);
	static FAvaActorData SaveActor(AActor* InActor, FAvaWorldData& WorldData);
};
