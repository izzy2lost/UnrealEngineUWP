// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EditorSubsystem.h"
#include "Engine/EngineTypes.h"

#include "ModularToolkitBlueprintLibrary.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogModularVehicleToolkit, Log, All);

class AModularVehiclePawn;
class UModularVehicle;
class UGeometryCollection;
class UModularVehicleComponent;

UCLASS(ClassGroup = (Physics))
class CHAOSMODULARVEHICLEEDITOR_API UModularToolkitBlueprintLibrary : public UObject
{
	
	GENERATED_UCLASS_BODY()

	enum class EModuleOperation : uint8
	{
		Add,
		Insert,
		Remove
	};

	/**
		* BP Function, Searches for static mesh and geometry collection components in source actors and adds them into an existing GeometryCollectionActor
		*/
//	UFUNCTION(BlueprintCallable, Category = "ModularVehicle")
	static void AddActorToExistingModularVehicle(AModularVehiclePawn* ExistingActorInOut, const AActor* Actor);

	static void AddActorsToExistingModularVehicle(AModularVehiclePawn* ExistingActorInOut, const TArray<AActor*>& Actors);



	/**
		* Searches for static mesh and geometry collection components in source actors and combines them into one new GeometryCollectionActor
		*/
	static AModularVehiclePawn* ConvertActorsToModularVehicle(const FString& InAssetPath, const TArray<AActor*>& Actors);

	/**
		* Creates a new geometry collection actor from scratch
		*/
	static class AModularVehiclePawn* CreateNewModularVehicleActor(const FString& InAssetPath, const FTransform& Transform);
	
	static class AModularVehiclePawn* SetupRestCollectionOnActor(const FString& InAssetPath, AModularVehiclePawn* ModularVehicleActor, const FTransform& Transform);

	static bool Attach(UModularVehicleComponent* ModularVehicleComponent, UClass* AttachClass, UClass* ToClass);

	static bool AttachNearest(UModularVehicleComponent* ModularVehicleComponent, UClass* AttachClass, UClass* ToClass);

	// Add new vehicle piece as child of specified parent piece index, returns new piece's index
	static int AddChild(AModularVehiclePawn* ExistingVehicleActor, int32 ParentIndex, const AActor* Actor);

	// inserts vehicle piece between existing parent and child at the specified child index, returns new piece's index
	static int InsertAbove(AModularVehiclePawn* ExistingVehicleActor, int32 ChildIndex, const AActor* Actor);

	// remove existing vehicle piece, moving the deleted pieces children up to the deleted pieces parent
	static int RemoveAt(AModularVehiclePawn* ExistingVehicleActor, int Index);


private:
	static void AddSingleRootNodeIfRequired(UGeometryCollection* GeometryCollectionObject);
	static void AddAdditionalAttributesIfRequired(UGeometryCollection* GeometryCollectionObject);
	static void FlattenHierarchy(UGeometryCollection* GeometryCollectionObject);

	static ULevel* GetSelectedLevel();
	static AActor* AddActor(ULevel* InLevel, UClass* Class);
	static int UpdateSimulationTree(AModularVehiclePawn* ExistingVehicle, EModuleOperation Operation, int Index, const AActor* Actor);

};
