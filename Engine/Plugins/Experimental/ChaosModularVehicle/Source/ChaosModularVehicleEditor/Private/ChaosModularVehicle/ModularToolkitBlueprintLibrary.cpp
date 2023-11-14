// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ModularToolkitBlueprintLibrary.h"

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Layers/LayersSubsystem.h"
#include "Subsystems/UnrealEditorSubsystem.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Subsystems/ImportSubsystem.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"

#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionConversion.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "ChaosModularVehicle/ModularVehicleObject.h"
#include "ChaosModularVehicle/ModularVehicleComponent.h"
#include "ChaosModularVehicle/ModularVehiclePawn.h"
#include "ChaosModularVehicle/ModularSimCollection.h"
#include "ChaosModularVehicle/VehicleSimComponentsInclude.h"
#include "SimModule/SimModulesInclude.h"
#include "VehicleUtility.h"

#if VEHICLE_DEBUGGING_ENABLED
UE_DISABLE_OPTIMIZATION
#endif

DEFINE_LOG_CATEGORY(LogModularVehicleToolkit);

UModularToolkitBlueprintLibrary::UModularToolkitBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


void UModularToolkitBlueprintLibrary::AddSingleRootNodeIfRequired(UGeometryCollection* GeometryCollectionObject)
{
	TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
	if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
	{
		if (FGeometryCollectionClusteringUtility::ContainsMultipleRootBones(GeometryCollection))
		{
			FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(GeometryCollection);
		}
	}
}

void UModularToolkitBlueprintLibrary::AddAdditionalAttributesIfRequired(UGeometryCollection* GeometryCollectionObject)
{
	TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
	if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
	{
		if (!GeometryCollection->HasAttribute("Level", FGeometryCollection::TransformGroup))
		{
			FGeometryCollectionClusteringUtility::UpdateHierarchyLevelOfChildren(GeometryCollection, -1);
		}
	}
}

void UModularToolkitBlueprintLibrary::FlattenHierarchy(UGeometryCollection* GeometryCollectionObject)
{
	TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
	if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
	{
		// Make sure we have valid "Level", this is not a default/runtime attribute, it is only used in editor
		AddAdditionalAttributesIfRequired(GeometryCollectionObject);

		if (GeometryCollection->Transform.Num() > 1)
		{
			FGeometryCollectionClusteringUtility::CollapseLevelHierarchy(-1, GeometryCollection);
		}
	}
}

ULevel* UModularToolkitBlueprintLibrary::GetSelectedLevel()
{
	USelection* SelectedActors = GEditor->GetSelectedActors();
	TArray<ULevel*> UniqueLevels;
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (Actor)
		{
			UniqueLevels.AddUnique(Actor->GetLevel());
		}
	}
	check(UniqueLevels.Num() == 1);
	return UniqueLevels[0];
}

AActor* UModularToolkitBlueprintLibrary::AddActor(ULevel* InLevel, UClass* Class)
{
	check(Class);

	UWorld* World = InLevel->OwningWorld;
	ULevel* DesiredLevel = InLevel;

	// Transactionally add the actor.
	AActor* Actor = NULL;
	{
		FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "AddActor", "Add Actor"));

		FActorSpawnParameters SpawnInfo;
		SpawnInfo.OverrideLevel = DesiredLevel;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.ObjectFlags = RF_Transactional;
		const auto Location = FVector(0);
		const auto Rotation = FTransform(FVector(0)).GetRotation().Rotator();
		Actor = World->SpawnActor(Class, &Location, &Rotation, SpawnInfo);

		check(Actor);
		Actor->InvalidateLightingCache();
		Actor->PostEditMove(true);
	}

	// If this actor is part of any layers (set in its default properties), add them into the visible layers list.
	ULayersSubsystem* Layers = GEditor->GetEditorSubsystem<ULayersSubsystem>();
	Layers->SetLayersVisibility(Actor->Layers, true);

	// Clean up.
	Actor->MarkPackageDirty();
	ULevel::LevelDirtiedEvent.Broadcast();

	return Actor;
}


void UModularToolkitBlueprintLibrary::AddActorToExistingModularVehicle(AModularVehiclePawn* ExistingActorInOut, const AActor* Actor /*const TArray<AActor*>& Actors*/)
{
	//	ensure(Actors.Num() > 0);
	const AActor* FirstActor = ExistingActorInOut;
	const FString& Name = FirstActor->GetActorLabel();
	const FVector FirstActorLocation(FirstActor->GetActorLocation());

	FGeometryCollectionEdit GeometryCollectionEdit = ExistingActorInOut->GetModularVehicleComponent()->EditRestCollection(GeometryCollection::EEditUpdate::RestPhysicsDynamic);
	UGeometryCollection* UpdatedGeometryCollection = GeometryCollectionEdit.GetRestCollection();
	check(UpdatedGeometryCollection);

	//for (AActor* Actor : Actors)
	{
		const FTransform ActorTransform(Actor->GetTransform());
		const FVector ActorOffset(Actor->GetActorLocation() - FirstActor->GetActorLocation());

		TArray<UStaticMeshComponent*> StaticMeshComponents;
		Actor->GetComponents<UStaticMeshComponent>(StaticMeshComponents, true);

		// START....
		TArray<UActorComponent*> SimComponents;
		Actor->GetComponents(UVehicleSimBaseComponent::StaticClass(), SimComponents);

		UVehicleSimBaseComponent* SimBaseComponent = Cast<UVehicleSimBaseComponent>(SimComponents[0]);
		bool IsWheel = SimBaseComponent->IsA(UVehicleSimWheelComponent::StaticClass());

		// ....END

		for (int32 ii = 0, ni = StaticMeshComponents.Num(); ii < ni; ++ii)
		{
			check(StaticMeshComponents.Num() == 1); // currently only supports one static mesh - because sim indices to transforms get all mixed up otherwise

			// We're partial to static mesh components, here
			UStaticMeshComponent* StaticMeshComponent = StaticMeshComponents[ii];
			if (StaticMeshComponent != nullptr)
			{
				UStaticMesh* ComponentStaticMesh = StaticMeshComponent->GetStaticMesh();
				if (ComponentStaticMesh != nullptr)
				{
					// If any of the static meshes have Nanite enabled, also enable on the new geometry collection asset for convenience.
					UpdatedGeometryCollection->EnableNanite |= ComponentStaticMesh->IsNaniteEnabled();

					FTransform ComponentTransform(StaticMeshComponent->GetComponentTransform());
					ComponentTransform.SetTranslation((ComponentTransform.GetTranslation() - ActorTransform.GetTranslation()) + ActorOffset);

					// Record the contributing source on the asset.
					FSoftObjectPath SourceSoftObjectPath(ComponentStaticMesh);
					decltype(FGeometryCollectionSource::SourceMaterial) SourceMaterials(StaticMeshComponent->GetMaterials());
					UpdatedGeometryCollection->GeometrySource.Emplace(SourceSoftObjectPath, ComponentTransform, SourceMaterials);

					//if (IsWheel)
					//{
					//	UpdatedGeometryCollection->GetGeometryCollection()->SimulationType[] = FGeometryCollection::ESimulationTypes::FST_None;
					//}
					FGeometryCollectionConversion::AppendStaticMesh(ComponentStaticMesh, SourceMaterials, ComponentTransform, UpdatedGeometryCollection, true);
				}
			}
		}

		TArray<UModularVehicleComponent*> ModularVehicleComponents;
		Actor->GetComponents<UModularVehicleComponent>(ModularVehicleComponents, true);
		for (int32 ii = 0, ni = ModularVehicleComponents.Num(); ii < ni; ++ii)
		{
			UModularVehicleComponent* ModularVehicleComponent = ModularVehicleComponents[ii];
			if (ModularVehicleComponent != nullptr)
			{
				const UGeometryCollection* RestCollection = ModularVehicleComponent->GetRestCollection();
				if (RestCollection != nullptr)
				{
					// If any of the static meshes have Nanite enabled, also enable on the new geometry collection asset for convenience.
					UpdatedGeometryCollection->EnableNanite |= RestCollection->EnableNanite;
				}

				FTransform ComponentTransform(ModularVehicleComponent->GetComponentTransform());
				ComponentTransform.SetTranslation((ComponentTransform.GetTranslation() - ActorTransform.GetTranslation()) + ActorOffset);

				// Record the contributing source on the asset.
				FSoftObjectPath SourceSoftObjectPath(RestCollection);

				// We're not interested in recording the final material of the collection since it's inevitably the Selection material.
				int32 NumMaterials = ModularVehicleComponent->GetNumMaterials() - 1;
				TArray<TObjectPtr<UMaterialInterface>> SourceMaterials;
				SourceMaterials.SetNum(NumMaterials);
				for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
				{
					SourceMaterials[MaterialIndex] = ModularVehicleComponent->GetMaterial(MaterialIndex);
				}
				UpdatedGeometryCollection->GeometrySource.Emplace(SourceSoftObjectPath, ComponentTransform, SourceMaterials);

				FGeometryCollectionConversion::AppendGeometryCollection(RestCollection, ModularVehicleComponent, ComponentTransform, UpdatedGeometryCollection, false);

			}
		}

	}

	UpdatedGeometryCollection->InitializeMaterials();

	// TODO: best if we add the actors in a flat manner in the first place, then we can skip this step
	// different behavior from fracture tools as it creates a new GC whereas we are adding to an existing one
	AddSingleRootNodeIfRequired(UpdatedGeometryCollection);
	FlattenHierarchy(UpdatedGeometryCollection);

	if (UpdatedGeometryCollection->EnableNanite)
	{
		UpdatedGeometryCollection->InvalidateCollection();
		UpdatedGeometryCollection->RebuildRenderData();
	}

	ExistingActorInOut->GetModularVehicleComponent()->MarkRenderStateDirty();

	// Add and initialize guids
	::GeometryCollection::GenerateTemporaryGuids(UpdatedGeometryCollection->GetGeometryCollection().Get(), 0, true);
}

void UModularToolkitBlueprintLibrary::AddActorsToExistingModularVehicle(AModularVehiclePawn* ExistingActorInOut, const TArray<AActor*>& Actors)
{
	ensure(Actors.Num() > 0);
	const AActor* FirstActor = Actors[0];
	const FString& Name = FirstActor->GetActorLabel();
	const FVector FirstActorLocation(FirstActor->GetActorLocation());

	FGeometryCollectionEdit GeometryCollectionEdit = ExistingActorInOut->GetModularVehicleComponent()->EditRestCollection(GeometryCollection::EEditUpdate::RestPhysicsDynamic);
	UGeometryCollection* UpdatedGeometryCollection = GeometryCollectionEdit.GetRestCollection();
	check(UpdatedGeometryCollection);

	for (AActor* Actor : Actors)
	{
		UE_LOG(LogModularVehicleToolkit, Log, TEXT("ModularVehicleConvert Adding Actor %s"), *Actor->GetName());

		const FTransform ActorTransform(Actor->GetTransform());
		const FVector ActorOffset(Actor->GetActorLocation() - FirstActor->GetActorLocation());

		TArray<UStaticMeshComponent*> StaticMeshComponents;
		Actor->GetComponents<UStaticMeshComponent>(StaticMeshComponents, true);

		for (int32 ii = 0, ni = StaticMeshComponents.Num(); ii < ni; ++ii)
		{
			check(StaticMeshComponents.Num() == 1); // currently only supports one static mesh - because sim indices to transforms get all mixed up otherwise

			UStaticMeshComponent* StaticMeshComponent = StaticMeshComponents[ii];
			if (StaticMeshComponent != nullptr)
			{
				UStaticMesh* ComponentStaticMesh = StaticMeshComponent->GetStaticMesh();
				if (ComponentStaticMesh != nullptr)
				{
					// If any of the static meshes have Nanite enabled, also enable on the new geometry collection asset for convenience.
					UpdatedGeometryCollection->EnableNanite |= ComponentStaticMesh->IsNaniteEnabled();

					FTransform ComponentTransform(StaticMeshComponent->GetComponentTransform());
					ComponentTransform.SetTranslation((ComponentTransform.GetTranslation() - ActorTransform.GetTranslation()) + ActorOffset);

					// Record the contributing source on the asset.
					FSoftObjectPath SourceSoftObjectPath(ComponentStaticMesh);
					decltype(FGeometryCollectionSource::SourceMaterial) SourceMaterials(StaticMeshComponent->GetMaterials());
					UpdatedGeometryCollection->GeometrySource.Emplace(SourceSoftObjectPath, ComponentTransform, SourceMaterials);

					FGeometryCollectionConversion::AppendStaticMesh(ComponentStaticMesh, SourceMaterials, ComponentTransform, UpdatedGeometryCollection, true);

					int LastIndex = UpdatedGeometryCollection->GetGeometryCollection()->Transform.Num() - 1;
					UE_LOG(LogModularVehicleToolkit, Log, TEXT("FGeometryCollectionConversion:: SM_Name %s, added transform @ Index %d, BoneName %s")
						, *ComponentStaticMesh->GetName()
						, LastIndex
						, *UpdatedGeometryCollection->GetGeometryCollection()->BoneName[LastIndex]);

				}			
			}
		}

		TArray<UModularVehicleComponent*> ModularVehicleComponents;
		Actor->GetComponents<UModularVehicleComponent>(ModularVehicleComponents, true);
		for (int32 ii = 0, ni = ModularVehicleComponents.Num(); ii < ni; ++ii)
		{
			UModularVehicleComponent* ModularVehicleComponent = ModularVehicleComponents[ii];
			if (ModularVehicleComponent != nullptr)
			{
				const UGeometryCollection* RestCollection = ModularVehicleComponent->GetRestCollection();
				if (RestCollection != nullptr)
				{
					// If any of the static meshes have Nanite enabled, also enable on the new geometry collection asset for convenience.
					UpdatedGeometryCollection->EnableNanite |= RestCollection->EnableNanite;
				}

				// this isn't right - only works when not rotated or scaled
				FTransform ComponentTransform(ModularVehicleComponent->GetComponentTransform());
				ComponentTransform.SetTranslation((ComponentTransform.GetTranslation() - ActorTransform.GetTranslation()) + ActorOffset);

				// Record the contributing source on the asset.
				FSoftObjectPath SourceSoftObjectPath(RestCollection);

				// We're not interested in recording the final material of the collection since it's inevitably the Selection material.
				int32 NumMaterials = ModularVehicleComponent->GetNumMaterials() - 1;
				TArray<TObjectPtr<UMaterialInterface>> SourceMaterials;
				SourceMaterials.SetNum(NumMaterials);
				for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
				{
					SourceMaterials[MaterialIndex] = ModularVehicleComponent->GetMaterial(MaterialIndex);
				}
				UpdatedGeometryCollection->GeometrySource.Emplace(SourceSoftObjectPath, ComponentTransform, SourceMaterials);

				FGeometryCollectionConversion::AppendGeometryCollection(RestCollection, ModularVehicleComponent, ComponentTransform, UpdatedGeometryCollection, false);
			}
		}

	}

	UpdatedGeometryCollection->InitializeMaterials();

	// TODO: best if we add the actors in a flat manner in the first place if this is possible, then we can skip this step
	// different behavior from fracture tools as it creates a new GC whereas we are adding to an existing one
	AddSingleRootNodeIfRequired(UpdatedGeometryCollection);
	FlattenHierarchy(UpdatedGeometryCollection);

	if (UpdatedGeometryCollection->EnableNanite)
	{
		UpdatedGeometryCollection->InvalidateCollection();
		UpdatedGeometryCollection->RebuildRenderData();
	}

	ExistingActorInOut->GetModularVehicleComponent()->MarkRenderStateDirty();

	// Add and initialize guids
	::GeometryCollection::GenerateTemporaryGuids(UpdatedGeometryCollection->GetGeometryCollection().Get(), 0, true);

}


AModularVehiclePawn* UModularToolkitBlueprintLibrary::ConvertActorsToModularVehicle(const FString& InAssetPath, const TArray<AActor*>& Actors)
{
	ensure(Actors.Num() > 0);

	// create a new GC based actor
	AModularVehiclePawn* NewActor = CreateNewModularVehicleActor(InAssetPath, FTransform());

	// now append geometry to this new actor
	AddActorsToExistingModularVehicle(NewActor, Actors);

	// fix up the simulation hierarchy
	int Index = 0;
	for (AActor* Actor : Actors)
	{
		int32 TreeIndex = UModularToolkitBlueprintLibrary::UpdateSimulationTree(NewActor, EModuleOperation::Add, Index++, Actor);
	}

	// Automatically introduce expected vehicle simulation hiearchy - a future vehicle editor would do this for us
	if (UModularVehicleComponent* ModularVehicleComponent = NewActor->GetModularVehicleComponent())
	{
		AttachNearest(ModularVehicleComponent, UVehicleSimSuspensionComponent::StaticClass(), UVehicleSimWheelComponent::StaticClass());

		if (!Attach(ModularVehicleComponent, UVehicleSimWheelComponent::StaticClass(), UVehicleSimTransmissionComponent::StaticClass()))
		{
			if (!Attach(ModularVehicleComponent, UVehicleSimWheelComponent::StaticClass(), UVehicleSimClutchComponent::StaticClass()))
			{
				Attach(ModularVehicleComponent, UVehicleSimWheelComponent::StaticClass(), UVehicleSimEngineComponent::StaticClass());
			}
		}

		Attach(ModularVehicleComponent, UVehicleSimTransmissionComponent::StaticClass(), UVehicleSimClutchComponent::StaticClass());
		Attach(ModularVehicleComponent, UVehicleSimClutchComponent::StaticClass(), UVehicleSimEngineComponent::StaticClass());
	}


	// Create a BP Actor asset using this new Actor
	FKismetEditorUtilities::FCreateBlueprintFromActorParams Params;
	Params.bReplaceActor = true;
	Params.ParentClassOverride = AModularVehiclePawn::StaticClass();
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromActor(InAssetPath + FString("_BP"), NewActor, Params);

	// Select the newly created blueprint in the content browser, but don't activate the browser
	TArray<UObject*> Objects;
	Objects.Add(Blueprint);
	GEditor->SyncBrowserToObjects(Objects, false);

	return NewActor;
}

class AModularVehiclePawn* UModularToolkitBlueprintLibrary::CreateNewModularVehicleActor(const FString& InAssetPath, const FTransform& Transform)
{
	FString UniquePackageName = InAssetPath + FString("_GC");
	FString UniqueAssetName = FPackageName::GetLongPackageAssetName(InAssetPath + FString("_GC"));

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(UniquePackageName, TEXT(""), UniquePackageName, UniqueAssetName);

	UPackage* Package = CreatePackage(*UniquePackageName);
	UModularVehicle* InModularVehicle = static_cast<UModularVehicle*>(NewObject<UModularVehicle>(Package, UModularVehicle::StaticClass(), FName(*UniqueAssetName), RF_Transactional | RF_Public | RF_Standalone));
	if (!InModularVehicle->SizeSpecificData.Num()) InModularVehicle->SizeSpecificData.Add(FGeometryCollectionSizeSpecificData());

	// Create the new actor
	AModularVehiclePawn* NewActor = Cast<AModularVehiclePawn>(AddActor(GetSelectedLevel(), AModularVehiclePawn::StaticClass()));

	// Set the Geometry Collection asset in the new actor
	NewActor->GetModularVehicleComponent()->SetRestCollection(InModularVehicle);

	// copy transform of original static mesh actor to this new actor
	NewActor->SetActorLabel(UniqueAssetName);
	NewActor->SetActorTransform(Transform);

	// Mark relevant stuff dirty
	FAssetRegistryModule::AssetCreated(InModularVehicle);
	InModularVehicle->MarkPackageDirty();
	Package->SetDirtyFlag(true);

	return NewActor;
}


class AModularVehiclePawn* UModularToolkitBlueprintLibrary::SetupRestCollectionOnActor(const FString& InAssetPath, AModularVehiclePawn* ModularVehicleActor, const FTransform& Transform)
{
	check(ModularVehicleActor);

	FString UniquePackageName = InAssetPath;
	FString UniqueAssetName = FPackageName::GetLongPackageAssetName(InAssetPath);

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(UniquePackageName, TEXT(""), UniquePackageName, UniqueAssetName);

	UPackage* Package = CreatePackage(*UniquePackageName);

	UModularVehicle* ModularVehicleAsset = static_cast<UModularVehicle*>(NewObject<UModularVehicle>(Package, UModularVehicle::StaticClass(), FName(*UniqueAssetName), RF_Transactional | RF_Public | RF_Standalone));
	if (!ModularVehicleAsset->SizeSpecificData.Num()) ModularVehicleAsset->SizeSpecificData.Add(FGeometryCollectionSizeSpecificData());

	check(ModularVehicleActor->GetModularVehicleComponent());

	// Set the Geometry Collection asset in the new actor
	ModularVehicleActor->GetModularVehicleComponent()->SetRestCollection(ModularVehicleAsset);

	// copy transform of original static mesh actor to this new actor
	ModularVehicleActor->SetActorLabel(UniqueAssetName);
	ModularVehicleActor->SetActorTransform(Transform);

	// Mark relevant stuff dirty
	FAssetRegistryModule::AssetCreated(ModularVehicleAsset);
	ModularVehicleAsset->MarkPackageDirty();
	Package->SetDirtyFlag(true);

	return ModularVehicleActor;
}


int32 UModularToolkitBlueprintLibrary::UpdateSimulationTree(AModularVehiclePawn* ExistingVehicle, EModuleOperation Operation, int32 Index, const AActor* Actor)
{
	check(ExistingVehicle);
	if (UModularVehicleComponent* ModularVehicleComponent = ExistingVehicle->GetModularVehicleComponent())
	{
		if (const UGeometryCollection* Rest = ModularVehicleComponent->GetRestCollection())
		{
			USceneComponent* BaseComponent = ModularVehicleComponent;

			TArray<UActorComponent*> Components;
			Actor->GetComponents(UVehicleSimBaseComponent::StaticClass(), Components);

			for (int32 ii = 0, ni = Components.Num(); ii < ni; ++ii)
			{
				UVehicleSimBaseComponent* SimBaseComponent = Cast<UVehicleSimBaseComponent>(Components[ii]);

				UE_LOG(LogModularVehicleToolkit, Log, TEXT("UpdateSimulationTree Actor %s, Comp %s, Index %d?"), *Actor->GetName(), *SimBaseComponent->GetName(), Index);

				UVehicleSimBaseComponent* DuplicateComponent = DuplicateObject(SimBaseComponent, ExistingVehicle);

				if (const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GC = Rest->GetGeometryCollection())
				{
					UE_LOG(LogModularVehicleToolkit, Log, TEXT("Index %d < GC->Transform.Num() %d"), Index, GC->Transform.Num());
					if (Index >= GC->Transform.Num())
					{
						UE_LOG(LogModularVehicleToolkit, Warning, TEXT("Modular Vehicle Index out of range"));
						return Chaos::ISimulationModuleBase::INVALID_IDX;
					}

					// Will ignore all the chassis parts as they don't have a function anyway
					if (SimBaseComponent->GetClass() != UVehicleSimChassisComponent::StaticClass())
					{
						FTransform NewComponentTransform = FTransform(GC->Transform[Index]);
						DuplicateComponent->TransformIndex = Index;
						DuplicateComponent->SetWorldTransform(NewComponentTransform);

						// exists to provide data setup information for the vehicle simulation, component itself does no work/is not visible
						DuplicateComponent->SetIsReplicated(false);
						DuplicateComponent->SetActive(false);
						DuplicateComponent->SetComponentTickEnabled(false);
						DuplicateComponent->SetAutoActivate(false);
						DuplicateComponent->SetHiddenInGame(true);

						// taking some clues as what to do here from USubobjectDataSubsystem::AddNewSubobject
						DuplicateComponent->AttachToComponent(BaseComponent, FAttachmentTransformRules::KeepWorldTransform);
						ExistingVehicle->AddInstanceComponent(DuplicateComponent);
						DuplicateComponent->OnComponentCreated();
						DuplicateComponent->RegisterComponent();
					}

				}
			}

			ExistingVehicle->RerunConstructionScripts();
		}
	}

	return Chaos::ISimulationModuleBase::INVALID_IDX;
}

bool UModularToolkitBlueprintLibrary::Attach(UModularVehicleComponent* ModularVehicleComponent, UClass* AttachClass, UClass* ToClass)
{
	bool bMatchFound = false;
	TArray<TObjectPtr<USceneComponent>> Children = ModularVehicleComponent->GetAttachChildren(); // deliberate copy

	TObjectPtr<USceneComponent> To = nullptr;
	for (auto& Child : Children)
	{
		if (Child.GetClass() == ToClass)
		{
			To = Child;
		}
	}

	if (To != nullptr)
	{
		for (auto& Child : Children)
		{
			if (Child.GetClass() == AttachClass)
			{
				bMatchFound = true;
				Child->AttachToComponent(To, FAttachmentTransformRules::KeepWorldTransform);
			}
		}
	}

	return bMatchFound;
}

bool UModularToolkitBlueprintLibrary::AttachNearest(UModularVehicleComponent* ModularVehicleComponent, UClass* AttachClass, UClass* ToClass)
{
	bool bMatchFound = false;
	TArray<TObjectPtr<USceneComponent>> Children = ModularVehicleComponent->GetAttachChildren(); // deliberate copy

	for (auto& Child : Children)
	{
		if (Child.GetClass() == AttachClass)
		{
			TObjectPtr<USceneComponent> Me = Child;
			TObjectPtr<USceneComponent> To = nullptr;
			float ClosestDistSqr = MAX_FLT;

			for (auto& ChildAlt : Children)
			{
				if (ChildAlt.GetClass() == ToClass)
				{
					bMatchFound = true;
					float DistanceSqr = (Me->GetRelativeLocation() - ChildAlt->GetRelativeLocation()).SizeSquared();
					if (DistanceSqr < ClosestDistSqr)
					{
						ClosestDistSqr = DistanceSqr;
						To = ChildAlt;
					}
				}
			}

			if (Me && To)
			{
				Me->AttachToComponent(To, FAttachmentTransformRules::KeepWorldTransform);
			}
		}
	}

	return bMatchFound;
}


int32 UModularToolkitBlueprintLibrary::AddChild(AModularVehiclePawn* ExistingVehicleActor, int32 ParentIndex, const AActor* Actor)
{
	// if GC Rest collection does not exist create it
	if (ExistingVehicleActor->GetModularVehicleComponent()->GetRestCollection() == nullptr)
	{
		FString OriginalName = ExistingVehicleActor->GetName();
		const FString InAssetPath = FString("/Game/Developers/billhenderson/" + OriginalName); // #TODO: fix
		SetupRestCollectionOnActor(InAssetPath, ExistingVehicleActor, FTransform());
	}

	// append geometry to this actor - flat hierarchy of nodes in Geometry Collection
	UModularToolkitBlueprintLibrary::AddActorToExistingModularVehicle(ExistingVehicleActor, Actor);

	// fix up the simulation hierarchy
	int32 TreeIndex = UModularToolkitBlueprintLibrary::UpdateSimulationTree(ExistingVehicleActor, EModuleOperation::Add, ParentIndex, Actor);

	return TreeIndex;
}

int32 UModularToolkitBlueprintLibrary::InsertAbove(AModularVehiclePawn* ExistingVehicleActor, int32 ChildIndex, const AActor* Actor)
{
	// if GC Rest collection does not exist create it
	//if (ExistingVehicleActor->GetModularVehicleComponent()->GetRestCollection() == nullptr)
	//{
	//	FTransform Transform;
	//	SetupRestCollectionOnActor(ExistingVehicleActor, Transform);
	//}

	// append geometry to this actor - flat hierarchy of nodes in Geometry Collection
	UModularToolkitBlueprintLibrary::AddActorToExistingModularVehicle(ExistingVehicleActor, Actor);

	// fix up the simulation hierarchy
	int32 TreeIndex = UModularToolkitBlueprintLibrary::UpdateSimulationTree(ExistingVehicleActor, EModuleOperation::Insert, ChildIndex, Actor);

	return TreeIndex;
}

int32 UModularToolkitBlueprintLibrary::RemoveAt(AModularVehiclePawn* ExistingVehicleActor, int Index)
{
	check(false); // TODO: implement
	return -1;
}

#if VEHICLE_DEBUGGING_ENABLED
UE_ENABLE_OPTIMIZATION
#endif
