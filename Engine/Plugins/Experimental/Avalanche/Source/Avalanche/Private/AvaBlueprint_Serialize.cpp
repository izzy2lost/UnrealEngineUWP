// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaBlueprint_Serialize.h"
#include "AudioDevice.h"
#include "EngineUtils.h"
#include "MovieScene.h"
#include "AI/NavigationSystemConfig.h"
#include "Algo/AllOf.h"
#include "AvaSequence.h"
#include "Archive/AvalancheReader.h"
#include "Archive/AvalancheWriter.h"
#include "Data/AvalancheActorData.h"
#include "Data/AvalancheComponentData.h"
#include "Data/AvalancheDataDefines.h"
#include "Data/AvalancheObjectData.h"
#include "Data/AvalancheSubObjectData.h"
#include "Data/AvalancheWorldData.h"
#include "GameFramework/PhysicsVolume.h"
#include "Engine/BrushBuilder.h"
#include "Misc/ScopedSlowTask.h"
#include "Particles/ParticleEventManager.h"
#include "Templates/NonNullPointer.h"
#include "UObject/ObjectSaveContext.h"

#if WITH_EDITORONLY_DATA
#include "Kismet2/ComponentEditorUtils.h"
#endif

#define LOCTEXT_NAMESPACE "AvaBlueprint_Serialize"

DEFINE_LOG_CATEGORY_STATIC(LogAvaBlueprint_Serialize, Log, All);

FSoftObjectPath FAvaBlueprint_Serialize::SetActorInPath(AActor* NewActor, const FSoftObjectPath& OriginalObjectPath)
{
	const static FString PersistentLevelString("PersistentLevel.");
	const int32 PersistentLevelStringLength = PersistentLevelString.Len();
	const FString& SubPathString            = OriginalObjectPath.GetSubPathString();
	const int32 IndexOfPersistentLevelInfo  = SubPathString.Find(PersistentLevelString, ESearchCase::CaseSensitive);
	if (IndexOfPersistentLevelInfo == INDEX_NONE)
	{
		return {};
	}

	const int32 DotAfterActorNameIndex = SubPathString.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromStart,
		IndexOfPersistentLevelInfo + PersistentLevelStringLength);

	const FSoftObjectPath PathToNewActor(NewActor);
	// PersistentLevel.StaticMeshActor_42.StaticMeshComponent becomes .StaticMeshComponent
	const FString PathAfterOriginalActor = SubPathString.Right(SubPathString.Len() - DotAfterActorNameIndex);
	return FSoftObjectPath(PathToNewActor.GetAssetPath(), PathToNewActor.GetSubPathString() + PathAfterOriginalActor);
}

UObject* FAvaBlueprint_Serialize::ResolveObjectDependency(FAvalancheWorldData& WorldData, FAvaObjectIndex ObjectIndex)
{
	if (!ensure(WorldData.SerializedObjectReferences.IsValidIndex(ObjectIndex.Index)))
	{
		return nullptr;
	}

	const FSoftObjectPath& OriginalObjectPath = WorldData.SerializedObjectReferences[ObjectIndex.Index];

	bool bIsPathToActorSubObject;
	const TOptional<FSoftObjectPath> PathToActor = ExtractActorFromPath(OriginalObjectPath, bIsPathToActorSubObject);

	const bool bIsExternalAssetReference = !PathToActor.IsSet();
	if (bIsExternalAssetReference)
	{
		UObject* ExternalReference      = OriginalObjectPath.ResolveObject();
		const bool bNeedsToLoadFromDisk = ExternalReference == nullptr;
		if (bNeedsToLoadFromDisk)
		{
			ExternalReference = OriginalObjectPath.TryLoad();
		}

		// We're supposed to be dealing with an external (asset) reference, e.g. a UMaterial.
		ensureAlwaysMsgf(ExternalReference == nullptr
			|| !ExternalReference->IsA<AActor>()
			&& !ExternalReference->IsA<UActorComponent>()
			, TEXT("Something is wrong. We just checked that the reference is not a world object but it is an actor or component."));

		return ExternalReference;
	}

	TWeakObjectPtr<AActor> CachedActor;
	{
		const FSoftObjectPath& OriginalActorPath = *PathToActor;

		FAvalancheActorData* const ActorData = WorldData.ActorData.Find(OriginalActorPath);
		if (!ActorData)
		{
			UE_LOG(LogTemp, Warning, TEXT("No save data found for actor %s"), *OriginalActorPath.ToString());
			return nullptr;
		}

		TWeakObjectPtr<AActor>& ActorWeak = WorldData.CachedActors.FindOrAdd(OriginalActorPath);
		if (!ActorWeak.IsValid())
		{
			const FSoftClassPath SoftClassPath(ActorData->ActorClass);
			UClass* TargetClass = SoftClassPath.TryLoadClass<AActor>();
			if (!TargetClass)
			{
				UE_LOG(LogTemp
					, Error
					, TEXT("Unknown class %s. Mostly likely it is referencing a class that was deleted.")
					, *SoftClassPath.ToString());
				return nullptr;
			}

			FActorSpawnParameters SpawnParams;
			SpawnParams.Template                       = Cast<AActor>(TargetClass->GetDefaultObject());
			SpawnParams.NameMode                       = FActorSpawnParameters::ESpawnActorNameMode::Requested;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ensureMsgf(SpawnParams.Template, TEXT("Failed to get class default. This should not happen. Investigate.")))
			{
				UClass* ClassToUse = SpawnParams.Template->GetClass();
				SpawnParams.Name   = *FString("AvalancheObjectInstance_").Append(
					*MakeUniqueObjectName(WorldData.World.Get(), ClassToUse).ToString());
				ActorWeak = WorldData.World->SpawnActor<AActor>(ClassToUse, SpawnParams);
			}
			else
			{
				ActorWeak = WorldData.World->SpawnActor<AActor>(TargetClass, SpawnParams);
			}

			if (ensureMsgf(ActorWeak.IsValid(), TEXT("Failed to spawn actor of class '%s'"), *ActorData->ActorClass.ToString()))
			{
#if WITH_EDITOR
				// Hide this actor so external systems can see that this components should not render, i.e. make USceneComponent::ShouldRender return false
				ActorWeak->SetIsTemporarilyHiddenInEditor(true);
#endif

				for (const TPair<FAvaObjectIndex, FAvalancheComponentData>& Pair : ActorData->ComponentData)
				{
					const FAvaObjectIndex& ReferenceIndex = Pair.Key;

					FAvalancheSubObjectData* SubObjectData = WorldData.SubObjects.Find(ReferenceIndex);
					if (ensure(SubObjectData))
					{
						const FSoftObjectPath& ComponentPath = WorldData.SerializedObjectReferences[ReferenceIndex.Index];
						FindOrAllocateComponent(WorldData
							, ActorWeak.Get()
							, *ActorData
							, ComponentPath
							, *SubObjectData
							, Pair.Value);
					}
				}
			}
		}

		CachedActor = ActorWeak;
	}

	if (!CachedActor.IsValid())
	{
		return nullptr;
	}

	AActor* SnapshotActor     = CachedActor.Get();
	const bool bIsPathToActor = !bIsPathToActorSubObject;
	if (bIsPathToActor)
	{
		return SnapshotActor;
	}

	// Reference to default subobjects, such as components, or an object we already allocated.
	const FSoftObjectPath PathToSubObject = SetActorInPath(SnapshotActor, OriginalObjectPath);
	if (UObject* ExistingSubObject = PathToSubObject.ResolveObject())
	{
		FAvalancheSubObjectData* SubObjectData = WorldData.SubObjects.Find(ObjectIndex.Index);
		if (!SubObjectData || SubObjectData->bWasSkippedClass)
		{
			return ExistingSubObject;
		}

		if (SubObjectData->Class.ResolveClass() != ExistingSubObject->GetClass())
		{
			UE_LOG(LogTemp, Warning
				, TEXT("Skipping serialisation of subobject because classes are different. Object '%s' will not contain values saved.")
				, *ExistingSubObject->GetName());
			return ExistingSubObject;
		}

		TWeakObjectPtr<UObject>& SubobjectCache = WorldData.CachedSubObjects.FindOrAdd(
			WorldData.SerializedObjectReferences[ObjectIndex.Index]);
		if (!SubobjectCache.IsValid())
		{
			SubobjectCache = ExistingSubObject;
			FAvalancheReader Reader(WorldData, *SubObjectData, ExistingSubObject);
			if (Reader.IsError())
			{
				UE_LOG(LogAvaBlueprint_Serialize, Error,
					TEXT("An error occured loading Avalanche SubObject \"%s\" of Class \"%s\" (Path: \"%s\")."),
					*GetNameSafe(ExistingSubObject), *SubObjectData->Class.ToString(),
					*WorldData.SerializedObjectReferences[ObjectIndex.Index].ToString());
			}
		}

		return ExistingSubObject;
	}

	// The sub object is instanced: recreate it recursively
	return CreateSubObject(WorldData, ObjectIndex, PathToSubObject);
}

FString FAvaBlueprint_Serialize::ExtractLastSubObjectName(const FSoftObjectPath& ObjectPath)
{
	const FString& SubPathString = ObjectPath.GetSubPathString();
	const int32 LastDotIndex     = SubPathString.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (LastDotIndex == INDEX_NONE)
	{
		return SubPathString;
	}

	return SubPathString.RightChop(LastDotIndex + 1);
}

UObject* FAvaBlueprint_Serialize::CreateSubObject(FAvalancheWorldData& WorldData, FAvaObjectIndex ObjectIndex,
	const FSoftObjectPath& PathToSubObject)
{
	FAvalancheSubObjectData* SubObjectData = WorldData.SubObjects.Find(ObjectIndex.Index);
	if (!SubObjectData || SubObjectData->bWasSkippedClass)
	{
		return nullptr;
	}

	if (const TWeakObjectPtr<UObject>* SubObjectCache = WorldData.CachedSubObjects.Find(PathToSubObject))
	{
		if (SubObjectCache->IsValid())
		{
			return SubObjectCache->Get();
		}
	}

	const UClass* Class = SubObjectData->Class.TryLoadClass<UObject>();
	if (!Class)
	{
		UE_LOG(LogTemp, Warning, TEXT("Class '%s' not found. Maybe it was removed?"), *SubObjectData->Class.ToString());
		return nullptr;
	}

	const FAvaObjectIndex OuterIndex = SubObjectData->OuterIndex;
	check(OuterIndex.Index != ObjectIndex.Index);
	UObject* SubObjectOuter = ResolveObjectDependency(WorldData, OuterIndex);
	if (!SubObjectOuter)
	{
		UE_LOG(LogTemp, Warning
			, TEXT("Failed to create '%s' because its outer could not be created.")
			, *PathToSubObject.ToString());
		return nullptr;
	}

	const FName SubObjectName        = *ExtractLastSubObjectName(PathToSubObject);
	const bool bSubObjectNameIsTaken = [SubObjectOuter, SubObjectName]
	{
		TArray<UObject*> Subobjects;
		GetObjectsWithOuter(SubObjectOuter, Subobjects, true);
		for (const UObject* Object : Subobjects)
		{
			if (Object->GetFName() == SubObjectName)
			{
				return true;
			}
		}
		return false;
	}();
	if (bSubObjectNameIsTaken)
	{
		UE_LOG(LogTemp
			, Warning, TEXT("Failed to create '%s' because subobject name was already taken.")
			, *PathToSubObject.ToString());
		return nullptr;
	}

	UObject* SubObject = NewObject<UObject>(SubObjectOuter, Class, SubObjectName);

	TWeakObjectPtr<UObject>& CachedSubObject = WorldData.CachedSubObjects.
	                                                     FindOrAdd(WorldData.SerializedObjectReferences[ObjectIndex.Index]);
	CachedSubObject = SubObject;

	FAvalancheReader Reader(WorldData, *SubObjectData, SubObject);
	if (Reader.IsError())
	{
		UE_LOG(LogAvaBlueprint_Serialize, Error,
			TEXT("An error occured loading Avalanche SubObject \"%s\" of Class \"%s\" (Path: \"%s\")."),
			*GetNameSafe(SubObject), *SubObjectData->Class.ToString(),
			*WorldData.SerializedObjectReferences[ObjectIndex.Index].ToString());
	}
	return SubObject;
}

FString FAvaBlueprint_Serialize::ExtractRootToLeafComponentPath(const FSoftObjectPath& ComponentPath)
{
	const static FString PersistentLevelString("PersistentLevel.");
	const int32 PersistentLevelStringLength = PersistentLevelString.Len();

	// /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42.StaticMeshComponent
	// becomes PersistentLevel.StaticMeshActor_42.StaticMeshComponent
	const FString& SubPathString           = ComponentPath.GetSubPathString();
	const int32 IndexOfPersistentLevelInfo = SubPathString.Find(PersistentLevelString, ESearchCase::CaseSensitive);
	if (IndexOfPersistentLevelInfo == INDEX_NONE)
	{
		return {};
	}

	// Example: /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42.StaticMeshComponent
	const int32 DotAfterActorNameIndex = SubPathString.Find(TEXT("."
			), ESearchCase::IgnoreCase
		, ESearchDir::FromStart
		, IndexOfPersistentLevelInfo + PersistentLevelStringLength);

	// PersistentLevel.SomeActor.SomeParentComp.SomeChildComp becomes SomeParentComp.SomeChildComp
	return ensure(DotAfterActorNameIndex != INDEX_NONE)
		       ? ComponentPath.GetSubPathString().RightChop(DotAfterActorNameIndex)
		       : FString();
}

UActorComponent* FAvaBlueprint_Serialize::FindMatchingComponent(AActor* ActorToSearchOn, const FSoftObjectPath& ComponentPath)
{
	const FString RootToLeafPath = ExtractRootToLeafComponentPath(ComponentPath);
	if (!ensure(!RootToLeafPath.IsEmpty()))
	{
		return nullptr;
	}

	for (UActorComponent* Component : ActorToSearchOn->GetComponents())
	{
		const FString OtherRootToLeaf = ExtractRootToLeafComponentPath(Component);
		if (RootToLeafPath.Equals(OtherRootToLeaf))
		{
			return Component;
		}
	}
	return nullptr;
}

void FAvaBlueprint_Serialize::SaveObject(FAvalancheWorldData& WorldData, FAvalancheObjectData& ObjectData, UObject* Object)
{
	FAvalancheWriter Writer(WorldData, ObjectData, Object);

	for (const FCustomVersion& CustomVersion : Writer.GetCustomVersions().GetAllVersions())
	{
		WorldData.VersionInfo.UsedCustomVersions.Add(CustomVersion.Key);
	}
}

void FAvaBlueprint_Serialize::LoadObject(FAvalancheWorldData& WorldData, FAvalancheObjectData& ObjectData, UObject* Object)
{
	FAvalancheReader Reader(WorldData, ObjectData, Object);
	if (Reader.IsError())
	{
		UE_LOG(LogAvaBlueprint_Serialize, Error,
			TEXT("An error occured loading Avalanche SubObject \"%s\"."),
			*GetNameSafe(Object));
	}
}

bool FAvaBlueprint_Serialize::ShouldSaveActor(const UWorld* InWorld, const AActor* InActor)
{
	if (InWorld && InActor)
	{
		return InActor != InWorld->GetDefaultPhysicsVolume()
			&& !InActor->HasAnyFlags(RF_Transient)
#if WITH_EDITOR
			&& InActor->IsEditable()
			&& !InActor->bIsEditorPreviewActor
#endif
			&& InActor != InWorld->GetDefaultBrush()
			&& InActor != InWorld->GetWorldSettings()
			&& InActor != InWorld->MyParticleEventManager;
	}
	return false;
}

bool FAvaBlueprint_Serialize::ShouldSaveComponent(const UActorComponent* InActorComponent)
{
	if (InActorComponent)
	{
		const bool bIsRootComponent = InActorComponent->GetOwner() && InActorComponent->GetOwner()->GetRootComponent() == InActorComponent;
		const bool bAllowedCreationMethod = InActorComponent->CreationMethod != EComponentCreationMethod::UserConstructionScript;
		const bool bNotTransient = !InActorComponent->HasAnyFlags(RF_Transient);

#if WITH_EDITORONLY_DATA
		constexpr bool bAllowUserConstructionScriptComps = false;

		const UActorComponent* ParentComponent = nullptr;
		if (const USceneComponent* SceneComponent = Cast<USceneComponent>(InActorComponent))
		{
			const bool bHasAttachParent = !!SceneComponent->GetAttachParent();
			const bool bHasOwner        = !!SceneComponent->GetOwner();

			//Check that we're Attached to a Component that is owned by our Owner Actor
			if (bHasAttachParent && bHasOwner
				&& SceneComponent->GetAttachParent()->GetOwner() == SceneComponent->GetOwner())
			{
				ParentComponent = SceneComponent->GetAttachParent();
			}
		}

		const bool bCanEditComponentInstance = FComponentEditorUtils::CanEditComponentInstance(InActorComponent
			, ParentComponent
			, bAllowUserConstructionScriptComps);

#else
		const bool bCanEditComponentInstance = true;
#endif

		// Always Save Root Components as this affects Actor's Transform
		return bIsRootComponent || (bAllowedCreationMethod && bNotTransient && bCanEditComponentInstance);
	}
	return false;
}

bool FAvaBlueprint_Serialize::ShouldSaveSubObject(UObject* InSubObject)
{
	check(InSubObject);
	checkf(!InSubObject->IsA<UClass>(), TEXT("Do you have a typo on your code?"));

	if (const UActorComponent* Component = Cast<UActorComponent>(InSubObject))
	{
		return ShouldSaveComponent(Component);
	}

	static const TSet UnsupportedClasses =
		{
			// Does not have state. Referenced by AVolumes, ABrushes, e.g. APostProcessVolume or ALightmassImportanceVolume
			UBrushBuilder::StaticClass(),
			UNavigationSystemConfig::StaticClass()
		};

	const bool bIsSupportedClass = Algo::AllOf(UnsupportedClasses, [InSubObject](UClass* Class)
	{
		return !InSubObject->IsA(Class);
	});

	return bIsSupportedClass && !InSubObject->HasAnyFlags(RF_Transient);
}

FAvaObjectIndex FAvaBlueprint_Serialize::AddOrFindObjectReference(FAvalancheWorldData& WorldData, const FSoftObjectPath& ObjectPath)
{
	if (const FAvaObjectIndex* ExistingIndex = WorldData.ObjectReferenceIndexMap.Find(ObjectPath))
	{
		return *ExistingIndex;
	}
	const FAvaObjectIndex Index(WorldData.SerializedObjectReferences.Add(ObjectPath));
	WorldData.ObjectReferenceIndexMap.Add(ObjectPath, Index);
	return Index;
}

TOptional<FSoftObjectPath> FAvaBlueprint_Serialize::ExtractActorFromPath(const FSoftObjectPath& InObjectPath, bool& bIsPathToActorSubObject)
{
	const static FString PersistentLevelString("PersistentLevel.");
	const int32 PersistentLevelStringLength = PersistentLevelString.Len();
	// /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42.StaticMeshComponent becomes PersistentLevel.StaticMeshActor_42.StaticMeshComponent
	const FString& SubPathString           = InObjectPath.GetSubPathString();
	const int32 IndexOfPersistentLevelInfo = SubPathString.Find(PersistentLevelString, ESearchCase::CaseSensitive);
	if (IndexOfPersistentLevelInfo == INDEX_NONE)
	{
		return {};
	}

	// Example: /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42.StaticMeshComponent
	const int32 DotAfterActorNameIndex = SubPathString.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromStart,
		IndexOfPersistentLevelInfo + PersistentLevelStringLength);
	const bool bPathIsToActor = DotAfterActorNameIndex == INDEX_NONE;

	bIsPathToActorSubObject = !bPathIsToActor;
	return bPathIsToActor
		       ?
		       // Example /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42
		       InObjectPath
		       :
		       // Converts /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42.StaticMeshComponent to /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42
		       FSoftObjectPath(InObjectPath.GetAssetPath(), SubPathString.Left(DotAfterActorNameIndex));
}

UActorComponent* FAvaBlueprint_Serialize::FindOrAllocateComponent(const FAvalancheWorldData& InWorldData
	, AActor* InRecreatedActor
	, const FAvalancheActorData& InActorData
	, const FSoftObjectPath& InComponentPath
	, FAvalancheSubObjectData& SubObjectData
	, const FAvalancheComponentData& ComponentData)
{
	UActorComponent* OutComponent = FindMatchingComponent(InRecreatedActor, InComponentPath);

	if (!OutComponent)
	{
		bool bIsOwnedByComponent;
		const FSoftObjectPath& ComponentOuterPath = InWorldData.SerializedObjectReferences[SubObjectData.OuterIndex.Index];
		TOptional<TNonNullPtr<const FAvalancheActorData>> ComponentOuterData;

		//Find Saved Actor Data Using Object Path
		if (const TOptional<FSoftObjectPath> PathToActor = ExtractActorFromPath(ComponentOuterPath, bIsOwnedByComponent))
		{
			const FAvalancheActorData* const Result = InWorldData.ActorData.Find(*PathToActor);

			UE_CLOG(Result == nullptr
				, LogTemp
				, Warning
				, TEXT(
					"Path %s looks like an actor path but no data was saved for it. Maybe it was a reference to an auto-generated actor, e.g. a brush or volume present in all worlds by default?"
				)
				, *ComponentOuterPath.ToString());

			if (Result)
			{
				ComponentOuterData = TOptional<TNonNullPtr<const FAvalancheActorData>>(Result);
			}
		}

		const bool bIsOwnedByOtherActor = ComponentOuterData.Get(nullptr) != &InActorData;

		if (!ensureAlwaysMsgf(ComponentOuterData
				, TEXT("Failed to recreate component %s because its outer %s did not have any associated actor data. Investigate.")
				, *InComponentPath.ToString(), *ComponentOuterPath.ToString())

			|| !ensureMsgf(!bIsOwnedByOtherActor
				, TEXT(
					"Failed to recreate component %s because saved data indicates it is owned by another actor %s. Components normally are owned by the actor they're attached to. Investigate."
				)
				, *InComponentPath.ToString(), *ComponentOuterPath.ToString()))
		{
			return nullptr;
		}

		UObject* ComponentOuter = InRecreatedActor;

		if (bIsOwnedByComponent)
		{
			bool bFoundPath = false;
			for (const TPair<FAvaObjectIndex, FAvalancheComponentData>& ComponentPair : InActorData.ComponentData)
			{
				const FSoftObjectPath& SavedComponentPath = InWorldData.SerializedObjectReferences[ComponentPair.Key.Index];
				if (SavedComponentPath == ComponentOuterPath)
				{
					bFoundPath = true;

					const FAvaObjectIndex ReferenceIndex                    = ComponentPair.Key;
					const FAvalancheSubObjectData* const FoundSubObjectData = InWorldData.SubObjects.Find(ReferenceIndex);

					if (ensure(FoundSubObjectData))
					{
						ComponentOuter = FindOrAllocateComponent(InWorldData
							, InRecreatedActor
							, InActorData
							, ComponentOuterPath
							, SubObjectData
							, ComponentPair.Value);
					}
				}
			}

			UE_CLOG(bFoundPath
				, LogTemp
				, Error
				, TEXT("Failed to find outer for component %s")
				, *ComponentOuterPath.ToString());
		}

		if (ComponentOuter)
		{
			//Allocate Component
			{
				const UClass* ComponentClass = SubObjectData.Class.TryLoadClass<UActorComponent>();

				if (!ensureMsgf(ComponentClass, TEXT("Component Class is invalid for Outer %s"), *ComponentOuter->GetName()))
				{
					return nullptr;
				}
				
				FName ComponentName;
				{
					const FString& SubPathString = InComponentPath.GetSubPathString();
					const int32 LastDotIndex     = SubPathString.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
					if (LastDotIndex == INDEX_NONE)
					{
						ComponentName = *SubPathString;
					}
					else
					{
						ComponentName = *SubPathString.RightChop(LastDotIndex + 1);
					}
				}

				const EObjectFlags ObjectFlags = SubObjectData.GetObjectFlags();

				// RF_Transactional must be set when the object is created so the creation transacted correctly
				OutComponent = NewObject<UActorComponent>(ComponentOuter
					, ComponentClass
					, ComponentName
					, ObjectFlags | RF_Transactional | RF_WasLoaded);

				OutComponent->SetFlags(ObjectFlags);
			}

			// UActorComponent::PostInitProperties implicitly calls AddOwnedComponent but we have to manually add it to the other arrays
			if (OutComponent->CreationMethod != ComponentData.CreationMethod)
			{
				OutComponent->CreationMethod = ComponentData.CreationMethod;
				switch (OutComponent->CreationMethod)
				{
					case EComponentCreationMethod::Instance: InRecreatedActor->AddInstanceComponent(OutComponent);
						break;

					case EComponentCreationMethod::SimpleConstructionScript: InRecreatedActor->BlueprintCreatedComponents.AddUnique(
							OutComponent);
						break;

					case EComponentCreationMethod::UserConstructionScript:
						checkf(false, TEXT("Component created in construction script currently unsupported"));
						break;

					case EComponentCreationMethod::Native:
					default: break;
				}
			}
		}
	}

	return OutComponent;
}

void FAvaBlueprint_Serialize::AddSubObjectDependency(FAvalancheWorldData& WorldData, UObject* ReferenceFromOriginalObject,
	FAvaObjectIndex Index)
{
	if (!ShouldSaveSubObject(ReferenceFromOriginalObject))
	{
		WorldData.SubObjects.Add(Index, FAvalancheSubObjectData::MakeSkippedSubObjectData());
		return;
	}

	const bool bNeedsToSaveSubObject = !WorldData.SubObjects.Contains(Index);
	if (bNeedsToSaveSubObject)
	{
		// Avoid infinite recursion
		WorldData.SubObjects.Add(Index);

		// Owning actor keeps reference to saved subobjects so they can be recreated faster when snapshot is loaded
		if (!ReferenceFromOriginalObject->IsA<UActorComponent>())
		{
			const AActor* const OwningActor = ReferenceFromOriginalObject->GetTypedOuter<AActor>();
			check(OwningActor);
			WorldData.ActorData.FindOrAdd(OwningActor).OwnedSubobjects.Add(Index);
		}

		// Important: first allocate SubobjectData on stack...
		FAvalancheSubObjectData SubObjectData;
		const UClass* Class      = ReferenceFromOriginalObject->GetClass();
		SubObjectData.Class      = Class;
		SubObjectData.OuterIndex = AddObjectDependency(WorldData, ReferenceFromOriginalObject->GetOuter());

		// ... because serialisation recursively calls this function. FWorldSnapshotData::Subobjects is possibly reallocated
		SaveObject(WorldData, SubObjectData, ReferenceFromOriginalObject);

		WorldData.SubObjects[Index] = MoveTemp(SubObjectData); // MoveTemp not profiled
	}
}

FAvaObjectIndex FAvaBlueprint_Serialize::AddObjectDependency(FAvalancheWorldData& WorldData
	, UObject* ReferenceFromOriginalObject
	, bool bCheckWhetherSubObject)
{
	// Even if ShouldSave later returns false for this object, we want to track it
	const FAvaObjectIndex OutIndex = AddOrFindObjectReference(WorldData, ReferenceFromOriginalObject);
	if (bCheckWhetherSubObject && ReferenceFromOriginalObject && ReferenceFromOriginalObject->GetTypedOuter<AActor>())
	{
		AddSubObjectDependency(WorldData, ReferenceFromOriginalObject, OutIndex);
	}
	return OutIndex;
}

void FAvaBlueprint_Serialize::LoadActor(AActor* InActor, const FSoftObjectPath& InActorPath, FAvalancheWorldData& WorldData)
{
	check(InActor);

	UE_LOG(LogTemp
		, Verbose
		, TEXT("========== Deserialize Actor %s ==========")
		, *InActorPath.ToString());

	FAvalancheActorData& ActorData = WorldData.ActorData[InActorPath];

	LoadObject(WorldData, ActorData, InActor);

#if WITH_EDITOR
	UE_LOG(LogTemp
		, Verbose
		, TEXT("ActorLabel is \"%s\" for \"%s\" (editor object path \"%s\")")
		, *InActor->GetActorLabel()
		, *InActor->GetPathName()
		, *InActorPath.ToString());
#endif

	//Deserialize Components
	for (const TPair<FAvaObjectIndex, FAvalancheComponentData>& ComponentPair : ActorData.ComponentData)
	{
		const EComponentCreationMethod CreationMethod = ComponentPair.Value.CreationMethod;

		// Construction script components are not supported
		if (CreationMethod != EComponentCreationMethod::UserConstructionScript)
		{
			const FAvaObjectIndex SubObjectIndex         = ComponentPair.Key;
			const FSoftObjectPath& OriginalComponentPath = WorldData.SerializedObjectReferences[SubObjectIndex.Index];

			if (UActorComponent* const ComponentToRestore = FindMatchingComponent(InActor, OriginalComponentPath))
			{
				FAvalancheSubObjectData& SubObjectData = WorldData.SubObjects[SubObjectIndex];
				LoadObject(WorldData, SubObjectData, ComponentToRestore);
				if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(ComponentToRestore))
				{
#if WITH_EDITOR
					PrimitiveComponent->UpdateCollisionProfile();
#else
					PrimitiveComponent->BodyInstance.LoadProfileData(false);
#endif
				}
			}
		}
	}

	InActor->PostLoad();
	TArray<UObject*> SubObjects;
	GetObjectsWithOuter(InActor, SubObjects);
	for (UObject* const Object : SubObjects)
	{
		if (Object)
		{
			Object->PostLoad();
		}
	}
}

TOptional<FAvalancheComponentData> FAvaBlueprint_Serialize::SaveComponent(UActorComponent* InComponent)
{
	check(InComponent);

	if (InComponent->CreationMethod == EComponentCreationMethod::UserConstructionScript)
	{
		UE_LOG(LogTemp, Warning
			, TEXT("Components created dynamically in the construction script are not supported (%s). Skipping...")
			, *InComponent->GetPathName());
		return TOptional<FAvalancheComponentData>{};
	}

	FAvalancheComponentData OutComponentData;
	OutComponentData.CreationMethod = InComponent->CreationMethod;
	return OutComponentData;
}

FAvalancheActorData FAvaBlueprint_Serialize::SaveActor(AActor* InActor, FAvalancheWorldData& WorldData)
{
	check(InActor);

	FAvalancheActorData OutActorData;

	const UClass* const ActorClass = InActor->GetClass();
	OutActorData.ActorClass        = ActorClass;

#if WITH_EDITORONLY_DATA
	OutActorData.ActorLabel        = InActor->GetActorLabel();
	OutActorData.bEditorVisibility = InActor->IsTemporarilyHiddenInEditor();
#endif

	SaveObject(WorldData, OutActorData, InActor);

	TInlineComponentArray<UActorComponent*> Components;
	InActor->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (ShouldSaveComponent(Component))
		{
			if (TOptional<FAvalancheComponentData> SerializedComponentData = SaveComponent(Component))
			{
				const FAvaObjectIndex ComponentIndex = AddObjectDependency(WorldData, Component);
				OutActorData.ComponentData.Add(ComponentIndex, *SerializedComponentData);
			}
		}
	}
	return OutActorData;
}

#undef LOCTEXT_NAMESPACE
