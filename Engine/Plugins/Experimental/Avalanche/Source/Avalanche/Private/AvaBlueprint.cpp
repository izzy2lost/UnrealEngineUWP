// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaBlueprint.h"
#include "Algo/ForEach.h"
#include "Async/Async.h"
#include "AudioDevice.h"
#include "AvaActor.h"
#include "AvaBlueprintGeneratedClass.h"
#include "AvaBlueprintVersion.h"
#include "AvaBlueprint_Serialize.h"
#include "AvaRemoteControlRebind.h"
#include "AvaRemoteControlUtils.h"
#include "AvaSceneSubsystem.h"
#include "AvaSequence.h"
#include "AvaSequencePlaybackObject.h"
#include "AvaSequencePlayer.h"
#include "AvaSequenceSubsystem.h"
#include "Camera/CameraActor.h"
#include "Engine/SimpleConstructionScript.h"
#include "EngineUtils.h"
#include "IRemoteControlModule.h"
#include "Misc/ScopedSlowTask.h"
#include "RemoteControlActor.h"
#include "RemoteControlPreset.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/UObjectThreadContext.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Viewport/Interaction/AvaViewportGuide.h"
#endif

#define LOCTEXT_NAMESPACE "AvalancheBlueprint"

DEFINE_LOG_CATEGORY_STATIC(LogAvalancheBlueprint, Log, All);

UAvalancheBlueprint::UAvalancheBlueprint()
{
	BlueprintType = EBlueprintType::BPTYPE_Normal;

	RemoteControlPreset = CreateDefaultSubobject<URemoteControlPreset>("RemoteControlPreset");
}

FTopLevelAssetPath UAvalancheBlueprint::GetWorldContextPath(const FSoftObjectPath& InSourceAssetPath)
{
	constexpr const TCHAR* WorldName     = TEXT("AvalancheWorld"); 
	constexpr const TCHAR* PackagePrefix = TEXT("/Temp/AvaEditor/TransientPackage");

	FName PackageName = *(PackagePrefix / InSourceAssetPath.GetLongPackageName());
	return FTopLevelAssetPath(PackageName, WorldName);
}

void UAvalancheBlueprint::BeginDestroy()
{
	UAvaSequencePlayer::OnSequenceFinished().RemoveAll(this);
	
	UnregisterRemoteControlPreset();

	Super::BeginDestroy();
}

void UAvalancheBlueprint::SetAvalancheWorld(UWorld* InWorld)
{
	World = InWorld;

	if (RemoteControlPreset)
	{
		RemoteControlPreset->RebindUnboundEntities();
	}

	if (UAvaSceneSubsystem* SceneSubsystem = World->GetSubsystem<UAvaSceneSubsystem>())
	{
		SceneSubsystem->RegisterSceneInterface(World->PersistentLevel, this);
	}
}

UWorld* UAvalancheBlueprint::GetAvalancheWorld() const
{
	return World;
}

AAvaActor* UAvalancheBlueprint::GetPlaceholderActor() const
{
#if WITH_EDITOR
	// If Placeholder Actor is Invalid, or Worlds do not match, try to find the Placeholder Actor
	if (ensure(World) && (!IsValid(PlaceholderActor) || PlaceholderActor->GetWorld() != World))
	{
		UAvalancheBlueprint* const MutableThis = const_cast<UAvalancheBlueprint*>(this);
		MutableThis->PlaceholderActor = nullptr;

		for (AAvaActor* const Actor : TActorRange<AAvaActor>(World))
		{
			if (Actor->GetClass()->ClassGeneratedBy == this)
			{
				MutableThis->PlaceholderActor = Actor;
				break;
			}
		}
	}
#endif
	return PlaceholderActor;
}

IAvaSequencePlaybackObject* UAvalancheBlueprint::GetScenePlayback() const
{
	// If the saved scene playback is valid use that
	if (IAvaSequencePlaybackObject* const ScenePlayback = PlaybackObject.GetInterface())
	{
		return ScenePlayback;
	}

	UAvaSequenceSubsystem* const SequenceSubsystem = UAvaSequenceSubsystem::Get(World);
	if (!SequenceSubsystem)
	{
		return nullptr;
	}

	UAvalancheBlueprint& MutableThis = *const_cast<UAvalancheBlueprint*>(this);
	if (IAvaSequencePlaybackObject* const ScenePlayback = SequenceSubsystem->FindOrCreatePlaybackObject(World->PersistentLevel, MutableThis))
	{
		MutableThis.PlaybackObject.SetObject(ScenePlayback->ToUObject());
		MutableThis.PlaybackObject.SetInterface(ScenePlayback);
		return ScenePlayback;
	}

	return nullptr;
}

void UAvalancheBlueprint::DestroyPlaceholderActor()
{
	if (PlaceholderActor && World)
	{
		World->DestroyActor(PlaceholderActor, false, false);
		World->BroadcastLevelsChanged();
	}

#if WITH_EDITOR
	if (SimpleConstructionScript && PlaceholderActor == SimpleConstructionScript->GetComponentEditorActorInstance())
	{
		// Ensure that all editable component references are cleared
		SimpleConstructionScript->ClearEditorComponentReferences();

		// Clear the reference to the actor instance
		SimpleConstructionScript->SetComponentEditorActorInstance(nullptr);
	}
#endif

	PlaceholderActor = nullptr;
}

void UAvalancheBlueprint::UpdatePlaceholderActor(bool bInForceFullUpdate)
{
	AAvaActor* AvaActor = GetPlaceholderActor();

	// if Actor is already valid and we are not doing a full update, skip
	if (AvaActor && !bInForceFullUpdate)
	{
		return;
	}

#if WITH_EDITOR
	// Signal that we're going to be constructing editor components
	if (SimpleConstructionScript)
	{
		SimpleConstructionScript->BeginEditorComponentConstruction();
	}
#endif

	// Spawn Placeholder Actor if current is invalid, or we're forcing full update
	if (!AvaActor || bInForceFullUpdate)
	{
		// Destroy the previous actor instance
		DestroyPlaceholderActor();

		// Spawn a new placeholder actor based on the Blueprint's generated class if it's Actor-based
		if (ensureMsgf(GeneratedClass && GeneratedClass->IsChildOf(AAvaActor::StaticClass())
			, TEXT("Generated Class (%s) is not based on Avalanche Actor."), GeneratedClass ? *GeneratedClass->GetName() : TEXT("invalid")))
		{
			// Spawn an Actor based on the Blueprint's generated class
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnInfo.ObjectFlags = RF_Transient | RF_Transactional;
			SpawnInfo.bNoFail = true;

			check(World);
			{
#if WITH_EDITOR
				FMakeClassSpawnableOnScope TemporarilySpawnable(GeneratedClass);
#endif
				AvaActor   = World->SpawnActor<AAvaActor>(GeneratedClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);
				PlaceholderActor = AvaActor;
			}
			check(AvaActor);

			// Ensure that the actor is invisible as Placeholder
			AvaActor->SetHidden(true);
#if WITH_EDITOR
			AvaActor->SetIsTemporarilyHiddenInEditor(true);
#endif

			// Prevent any audio from playing as a result of spawning
			if (FAudioDeviceHandle AudioDevice = GEngine->GetMainAudioDevice())
			{
				AudioDevice->Flush(World);
			}

#if WITH_EDITOR
			// Set the reference to the preview actor for component editing purposes
			if (SimpleConstructionScript)
			{
				SimpleConstructionScript->SetComponentEditorActorInstance(AvaActor);
			}
#endif
		}
	}
	else
	{
		check(AvaActor);
		AvaActor->ReregisterAllComponents();
#if WITH_EDITOR
		AvaActor->RerunConstructionScripts();
#endif
	}

#if WITH_EDITOR
	// Signal that we're done constructing editor components
	if (SimpleConstructionScript)
	{
		SimpleConstructionScript->EndEditorComponentConstruction();
	}
#endif
}

void UAvalancheBlueprint::PreDuplicate(FObjectDuplicationParameters& InDuplicationParameters)
{
	Super::PreDuplicate(InDuplicationParameters);
	SaveAvalancheWorld();
}

void UAvalancheBlueprint::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FAvalancheBlueprintVersion::GUID);
	
	Super::Serialize(Ar);
}

#if WITH_EDITORONLY_DATA
void UAvalancheBlueprint::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	SaveAvalancheWorld();
	Super::PreSave(ObjectSaveContext);
}

void UAvalancheBlueprint::PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext)
{
	Super::PostSaveRoot(ObjectSaveContext);
	OnAvalanchePostSave.Broadcast();
}
#endif

void UAvalancheBlueprint::SaveAvalancheWorld()
{
#if WITH_EDITOR
	OnAvalanchePreSave.Broadcast();
#endif

	WorldData.World = GetAvalancheWorld();
	if (!WorldData.World.IsValid())
	{
		return;
	}

	TGuardValue SaveGuard(bSavingWorld, true);

#if WITH_EDITOR
	FScopedSlowTask SaveWorldTask(2.f, LOCTEXT("SaveMotionDesignWorld", "Saving Motion Design World"));
	SaveWorldTask.MakeDialogDelayed(1.f);	
#endif
	
	TArray<AActor*> ActorsInWorld;

	//Get all Actors from all Levels
	{
		int32 NumActors = 0;
		for (ULevel* const Level : WorldData.World->GetLevels())
		{
			if (Level)
			{
				NumActors += Level->Actors.Num();
			}
		}
			
		ActorsInWorld.Reserve(NumActors);
			
		for (ULevel* const Level : WorldData.World->GetLevels())
		{
			if (Level)
			{
				ActorsInWorld.Append(Level->Actors);
			}
		}
	}

	FAvaWorldData NewWorldData;
	
#if WITH_EDITOR	
	SaveWorldTask.EnterProgressFrame(1.f);
#endif
	
	//Save Actors
	{
		FScopedSlowTask SaveActorsTask(ActorsInWorld.Num(), LOCTEXT("SavingActors", "Saving Actors"));
		SaveActorsTask.MakeDialogDelayed(1.f);
			
		Algo::ForEach(ActorsInWorld, [&NewWorldData, this, &SaveActorsTask](AActor* Actor)
		{
			SaveActorsTask.EnterProgressFrame();
			if (FAvaBlueprint_Serialize::ShouldSaveActor(WorldData.World.Get(), Actor))
			{
				NewWorldData.ActorData.Add(Actor, FAvaBlueprint_Serialize::SaveActor(Actor, NewWorldData));
			}
		});
	}

	NewWorldData.FinalizeSave();

#if WITH_EDITOR		
	SaveWorldTask.EnterProgressFrame(1.f);
	//Internal::ComputeActorHashes(ActorsInWorld, SnapshotData);
	SetCreateDefaultScene(false);
#endif

	WorldData = MoveTemp(NewWorldData);
}

void UAvalancheBlueprint::LoadAvalancheWorld(UWorld* InOverrideWorld, TOptional<FInitActorFunctionRef> InInitActorFunction)
{
	WorldData.World = InOverrideWorld ? InOverrideWorld : GetAvalancheWorld();
	if (!WorldData.World.IsValid())
	{
		return;
	}

	TGuardValue LoadGuard(bLoadingWorld, true);
	
#if WITH_EDITOR
	FScopedSlowTask LoadWorld(WorldData.ActorData.Num(), LOCTEXT("LoadWorld", "Loading Motion Design World"));
	LoadWorld.MakeDialogDelayed(1.f, false);
#endif
	
	TMap<FSoftObjectPath, AActor*> RecreatedActors;
	RecreatedActors.Reserve(WorldData.ActorData.Num());

	const bool bIsEditorWorld = WorldData.World->IsEditorWorld();
	
	//1st Pass: Allocate/Spawn the Actors. Serialization is done in separate pass so Object References can be resolved correctly.
	for (const TPair<FSoftObjectPath, FAvaActorData>& Pair : WorldData.ActorData)
	{
		const FSoftObjectPath& ActorPath = Pair.Key;
		const FAvaActorData& ActorData = Pair.Value;
		
		UClass* ActorClass = ActorData.ActorClass.TryLoadClass<AActor>();
		if (!ActorClass)
		{
			UE_LOG(LogAvalancheBlueprint
				, Warning
				, TEXT("Failed to resolve class '%s'. Was it removed?")
				, *ActorData.ActorClass.ToString());
			continue;
		}
	
		//Handle Name Clash
		if (UObject* const FoundObject = FindObject<UObject>(WorldData.World.Get(), *ActorPath.ToString()))
		{			
			// If it's not an actor then it's possibly a UObjectRedirector
			AActor* const FoundActor = Cast<AActor>(FoundObject);
			if (IsValid(FoundActor))
			{
#if WITH_EDITOR
				if (bIsEditorWorld)
				{
					GEditor->SelectActor(FoundActor, /*bSelect =*/true, /*bNotifyForActor =*/false, /*bSelectEvenIfHidden =*/true);
					constexpr bool bVerifyDeletionCanHappen = true;
					constexpr bool bWarnAboutReferences = false;
					GEditor->edactDeleteSelected(FoundActor->GetWorld(), bVerifyDeletionCanHappen, bWarnAboutReferences, bWarnAboutReferences);
				}
				else
#endif
				{
					FoundActor->Destroy(true, true);
				}
			}
			else
			{
				const FName NewName = MakeUniqueObjectName(FoundObject->GetOuter(), FoundObject->GetClass());
				FoundObject->Rename(*NewName.ToString(), nullptr, REN_NonTransactional | REN_DontCreateRedirectors);
			}
		}
		const FString& SubObjectPath = ActorPath.GetSubPathString();
		const int32 LastDotIndex = SubObjectPath.Find(TEXT("."));
		// Full string: /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42
		// SubObjectPath: PersistentLevel.StaticMeshActor_42 
		checkf(LastDotIndex != INDEX_NONE, TEXT("There should always be at least one dot after PersistentLevel"));
			
		const int32 NameLength = SubObjectPath.Len() - LastDotIndex - 1;
		const FString ActorName = SubObjectPath.Right(NameLength);
		
		ULevel* const OverrideLevel = WorldData.World->PersistentLevel;
		const FName ActorFName = *ActorName;
		
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = ActorFName;
		SpawnParameters.OverrideLevel = OverrideLevel;
		SpawnParameters.bNoFail = true;
		SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ErrorAndReturnNull;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags = ActorData.GetObjectFlags() | RF_WasLoaded;

		if (AActor* const RecreatedActor = WorldData.World->SpawnActor(ActorClass, nullptr, SpawnParameters))
		{
			//OnPostRecreateActor(RecreatedActor);
			RecreatedActors.Add(ActorPath, RecreatedActor);
			WorldData.CachedActors.Add(ActorPath, RecreatedActor);
		}
	}
	
	//2nd Pass: Serialization 
	for (const TPair<FSoftObjectPath, FAvaActorData>& Pair : WorldData.ActorData)
	{
#if WITH_EDITOR
		LoadWorld.EnterProgressFrame();
#endif

		const FSoftObjectPath& ActorPath = Pair.Key;
		const FAvaActorData& ActorData = Pair.Value;
		
		if (AActor* const * const RecreatedActor = RecreatedActors.Find(ActorPath))
		{
			AActor* const Actor = *RecreatedActor;
			
			UE_LOG(LogAvalancheBlueprint
				, Verbose
				, TEXT("========== Apply recreated %s ==========")
				, *Actor->GetPathName());

			//Allocate Components
			for (const TPair<FAvaObjectIndex, FAvaComponentData>& ComponentPair : ActorData.ComponentData)
			{
				const FAvaObjectIndex& ReferenceIndex = ComponentPair.Key;
				const FAvaComponentData& ComponentData = ComponentPair.Value;
				FAvaSubObjectData* SubObjectData = WorldData.SubObjects.Find(ReferenceIndex);
				
				if (ensure(SubObjectData))
				{
					const FSoftObjectPath& ComponentPath = WorldData.SerializedObjectReferences[ReferenceIndex.Index];
					UActorComponent* const Component = FAvaBlueprint_Serialize::FindOrAllocateComponent(WorldData, Actor, ActorData
						, ComponentPath, *SubObjectData, ComponentData);
					
					if (Component)
					{
						UE_LOG(LogAvalancheBlueprint
							, Verbose
							, TEXT("Found or Allocated Component %s")
							, *Component->GetPathName());
					}
					else
					{
						UE_LOG(LogAvalancheBlueprint
							, Verbose
							, TEXT("Could not Find or Allocate from Component Path %s")
							, *ComponentPath.ToString());
					}
				}
			}


			const AActor* AttachParentBeforeLoad = Actor->GetAttachParentActor();
			FAvaBlueprint_Serialize::LoadActor(Actor, ActorPath, WorldData);
			const AActor* AttachParentAfterLoad = Actor->GetAttachParentActor();

			Actor->UnregisterAllComponents();
			
			if (AttachParentAfterLoad && Actor->GetRootComponent())
			{
				USceneComponent* const AttachComponent = Actor->GetRootComponent()->GetAttachParent();
				const FName AttachSocket = Actor->GetAttachParentSocketName();
				
				Actor->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
				Actor->AttachToComponent(AttachComponent
					, FAttachmentTransformRules::KeepRelativeTransform
					, AttachSocket);
			}

#if WITH_EDITOR
			Actor->RerunConstructionScripts();
#endif
			Actor->RegisterAllComponents();
			Actor->UpdateComponentTransforms();
			
			{
				// GAllowActorScriptExecutionInEditor must be temporarily true so call to UserConstructionScript isn't skipped
				FEditorScriptExecutionGuard AllowConstructionScript;
				Actor->UserConstructionScript();
			}

			if (InInitActorFunction.IsSet())
			{
				(*InInitActorFunction)(*Actor, ActorData);
			}

#if WITH_EDITOR
			// Otherwise actor will show up with internal object name, e.g. actor previously called Cube will be StaticMeshActor1
			Actor->SetActorLabel(ActorData.ActorLabel);
			Actor->SetIsTemporarilyHiddenInEditor(ActorData.bEditorVisibility);
			// Recreated actors have invalid lightning cache... e.g. recreated point lights will show error image (S_LightError)
			Actor->InvalidateLightingCacheDetailed(false);
#endif
		}
		else
		{
			UE_LOG(LogAvalancheBlueprint
				, Error
				, TEXT("Failed to recreate actor %s")
				, *ActorPath.ToString());
		}
	}

	// Post Spawn, handle AvaBlueprint data only if the World was not overridden,
	// as specifying an Override World is treated more as a read-only load, rather than a load & fix
	if (!InOverrideWorld)
	{
		if (RemoteControlPreset)
		{
			RemoteControlPreset->RebindUnboundEntities();
			FAvaRemoteControlRebind::ResolveAllFieldPathInfos(RemoteControlPreset);
		}

		UWorld* const PlaybackContext = WorldData.World.Get();
		ensureAlways(PlaybackContext == World);

		for (UAvaSequence* const Animation : GetSequences())
		{
			if (IsValid(Animation))
			{
				Animation->MigrateLegacyBindings(PlaybackContext);
			}
		}
	}
}

void UAvalancheBlueprint::RegisterRemoteControlPreset()
{
	if (!HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
	{
		FAvaRemoteControlUtils::RegisterRemoteControlPreset(RemoteControlPreset, /*bInEnsureUniqueId*/ true);
	}
}

void UAvalancheBlueprint::UnregisterRemoteControlPreset()
{
	if (!HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
	{
		FAvaRemoteControlUtils::UnregisterRemoteControlPreset(RemoteControlPreset);
	}
}

void UAvalancheBlueprint::PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph)
{
	ScheduleRebuildSequenceTree();
}

ULevel* UAvalancheBlueprint::GetSceneLevel() const
{
	return World ? World->PersistentLevel : nullptr;
}

void UAvalancheBlueprint::SetStartupCameraName(FName InCameraName)
{
	if (InCameraName == StartupCameraName)
	{
		return;
	}

#if WITH_EDITOR
	StartupCameraName = InCameraName;
#endif
}

#if WITH_EDITOR
UClass* UAvalancheBlueprint::GetBlueprintClass() const
{
	return UAvaBlueprintGeneratedClass::StaticClass();
}

void UAvalancheBlueprint::GetReparentingRules(TSet<const UClass*>& AllowedChildrenOfClasses,
	TSet<const UClass*>& DisallowedChildrenOfClasses) const
{
	AllowedChildrenOfClasses.Add(UAvalancheBlueprint::StaticClass());
}
#endif

IAvaSequencePlaybackObject* UAvalancheBlueprint::GetPlaybackObject() const
{
	return GetScenePlayback();
}

UObject* UAvalancheBlueprint::ToUObject()
{
	return this;
}

UWorld* UAvalancheBlueprint::GetContextWorld() const
{
	return GetAvalancheWorld();
}

bool UAvalancheBlueprint::CreateDirectorInstance(UAvaSequence& InSequence
	, IMovieScenePlayer& InPlayer
	, const FMovieSceneSequenceID& InSequenceID
	, UObject*& OutDirectorInstance)
{
	OutDirectorInstance = GetPlaceholderActor();
	return true;
}

void UAvalancheBlueprint::SetDefaultSequence(UAvaSequence* InSequence)
{
	if (IsValid(InSequence))
	{
		AddSequence(InSequence);
		DefaultAnimationIndex = Animations.Find(InSequence);
	}
}

UAvaSequence* UAvalancheBlueprint::GetDefaultSequence() const
{
	if (Animations.IsValidIndex(DefaultAnimationIndex))
	{
		return Animations[DefaultAnimationIndex];
	}
	return nullptr;
}

bool UAvalancheBlueprint::AddSequence(UAvaSequence* InSequence)
{
	if (IsValid(InSequence) && !Animations.Contains(InSequence))
	{
		Animations.Add(InSequence);
		ScheduleRebuildSequenceTree();
		return true;
	}
	
	return false;
}

void UAvalancheBlueprint::RemoveSequence(UAvaSequence* InSequence)
{
	Animations.Remove(InSequence);
	if (InSequence)
	{
		InSequence->OnSequenceRemoved();
	}
	ScheduleRebuildSequenceTree();
}

FName UAvalancheBlueprint::GetSequenceProviderDebugName() const
{
	return GetFName();
}

#if WITH_EDITOR
void UAvalancheBlueprint::OnEditorSequencerCreated(const TSharedPtr<ISequencer>& InSequencer)
{
	EditorSequencer = InSequencer;
	RebuildSequenceTree();
}

TSharedPtr<ISequencer> UAvalancheBlueprint::GetEditorSequencer() const
{
	return EditorSequencer.Pin();
}

bool UAvalancheBlueprint::GetDirectorBlueprint(UAvaSequence& InSequence, UBlueprint*& OutBlueprint)
{
	OutBlueprint = this;
	return true;
}
#endif

void UAvalancheBlueprint::RebuildSequenceTree()
{
	bPendingAnimTreeUpdate = false;
	IAvaSequenceProvider::RebuildSequenceTree();
}

void UAvalancheBlueprint::ScheduleRebuildSequenceTree()
{
	// Bail if the Async Task hasn't executed yet
	if (bPendingAnimTreeUpdate)
	{
		return;
	}

	bPendingAnimTreeUpdate = true;

	TWeakObjectPtr<UAvalancheBlueprint> ThisWeak(this);
	AsyncTask(ENamedThreads::GameThread, [ThisWeak]()->void
	{
		UAvalancheBlueprint* const This = ThisWeak.Get();

		// Check if we already have Rebuilt the Animation Tree while waiting for this Async Task
		if (This && This->bPendingAnimTreeUpdate)
		{
			This->RebuildSequenceTree();
		}
	});
}

UWorld* UAvalancheBlueprint::GetWorld() const
{
	return World.Get();
}

#if WITH_EDITOR
void UAvalancheBlueprint::SetGuideOffset(int32 InGuideIdx, float InGuideOffset)
{
	if (GuideInfos.IsValidIndex(InGuideIdx))
	{
		Modify();
		GuideInfos[InGuideIdx].OffsetFraction = InGuideOffset;
	}
}

void UAvalancheBlueprint::SetGuides(const TArray<FAvaViewportGuideInfo_Deprecated>& InNewGuideInfos)
{
	Modify();
	GuideInfos = InNewGuideInfos;
}
#endif

#undef LOCTEXT_NAMESPACE
