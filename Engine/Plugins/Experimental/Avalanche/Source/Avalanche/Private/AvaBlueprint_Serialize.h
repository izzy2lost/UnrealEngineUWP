// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/AvalancheDataDefines.h"

class AActor;
class FString;
class UActorComponent;
class UObject;
class UWorld;
struct FAvalancheActorData;
struct FAvalancheComponentData;
struct FAvalancheObjectData;
struct FAvalancheSubObjectData;
struct FAvalancheWorldData;
struct FSoftObjectPath;
template<typename OptionalType> struct TOptional;

struct FAvaBlueprint_Serialize
{
	/**
	 * Takes an existing path to an actor's subobjects and replaces the actor bit with the path to another actor.
	 * E.g. /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42.StaticMeshComponent could become /Game/MapName.MapName:PersistentLevel.SomeOtherActor.StaticMeshComponent
	 */
	static FSoftObjectPath SetActorInPath(AActor* NewActor, const FSoftObjectPath& OriginalObjectPath);

	static UObject* ResolveObjectDependency(FAvalancheWorldData& WorldData, FAvaObjectIndex ObjectIndex);

	static FString ExtractLastSubObjectName(const FSoftObjectPath& ObjectPath);

	static UObject* CreateSubObject(FAvalancheWorldData& WorldData, FAvaObjectIndex ObjectIndex, const FSoftObjectPath& PathToSubObject);

	static FString ExtractRootToLeafComponentPath(const FSoftObjectPath& ComponentPath);
	static UActorComponent* FindMatchingComponent(AActor* ActorToSearchOn, const FSoftObjectPath& ComponentPath);

	static void SaveObject(FAvalancheWorldData& WorldData, FAvalancheObjectData& ObjectData, UObject* Object);
	static void LoadObject(FAvalancheWorldData& WorldData, FAvalancheObjectData& ObjectData, UObject* Object);

	static bool ShouldSaveActor(const UWorld* InWorld, const AActor* InActor);
	static bool ShouldSaveComponent(const UActorComponent* InActorComponent);
	static bool ShouldSaveSubObject(UObject* InSubObject);

	static UActorComponent* FindOrAllocateComponent(const FAvalancheWorldData& InWorldData
		, AActor* InRecreatedActor
		, const FAvalancheActorData& InActorData
		, const FSoftObjectPath& InComponentPath
		, FAvalancheSubObjectData& SubObjectData
		, const FAvalancheComponentData& ComponentData);

	static FAvaObjectIndex AddOrFindObjectReference(FAvalancheWorldData& WorldData, const FSoftObjectPath& ObjectPath);
	static TOptional<FSoftObjectPath> ExtractActorFromPath(const FSoftObjectPath& InObjectPath, bool& bIsPathToActorSubObject);
	static void AddSubObjectDependency(FAvalancheWorldData& WorldData, UObject* ReferenceFromOriginalObject, FAvaObjectIndex Index);
	static FAvaObjectIndex AddObjectDependency(FAvalancheWorldData& WorldData
		, UObject* ReferenceFromOriginalObject
		, bool bCheckWhetherSubObject = true);

	static void LoadActor(AActor* InActor, const FSoftObjectPath& InActorPath, FAvalancheWorldData& WorldData);
	static TOptional<FAvalancheComponentData> SaveComponent(UActorComponent* InComponent);
	static FAvalancheActorData SaveActor(AActor* InActor, FAvalancheWorldData& WorldData);
};
