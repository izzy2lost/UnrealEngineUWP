// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaScene.h"
#include "AvaAssetTags.h"
#include "AvaRemoteControlUtils.h"
#include "AvaSceneSettings.h"
#include "AvaSceneState.h"
#include "AvaSceneSubsystem.h"
#include "AvaSequencePlaybackObject.h"
#include "AvaSequenceSubsystem.h"
#include "AvaWorldSubsystemUtils.h"
#include "Containers/Ticker.h"
#include "EngineUtils.h"
#include "RemoteControlPreset.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/UObjectThreadContext.h"

#if WITH_EDITOR
#include "AvaBlueprint.h"
#include "AvaField.h"
#include "Misc/ScopedSlowTask.h"
#include "RemoteControlBinding.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogAvaScene, Log, All);

#define LOCTEXT_NAMESPACE "AvaScene"

AAvaScene* AAvaScene::GetScene(ULevel* InLevel, bool bInCreateSceneIfNotFound)
{
	if (!IsValid(InLevel))
	{
		return nullptr;
	}

	AAvaScene* ExistingScene = nullptr;

	// Return the Existing Scene, or nullptr if not found and not creating a new scene
	if (InLevel->Actors.FindItemByClass<AAvaScene>(&ExistingScene) || !bInCreateSceneIfNotFound)
	{
		return ExistingScene;
	}

	UWorld* const World = InLevel->GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.OverrideLevel = InLevel;
#if WITH_EDITOR
	SpawnParameters.bHideFromSceneOutliner = true;
#endif

	AAvaScene* const NewScene = World->SpawnActor<AAvaScene>(SpawnParameters);
	return NewScene;
}

#if WITH_EDITOR
AAvaScene* AAvaScene::CreateFromBlueprint(ULevel* InNewLevel, UAvalancheBlueprint* InBlueprint)
{
	if (!IsValid(InNewLevel) || !IsValid(InNewLevel->OwningWorld) || !IsValid(InBlueprint))
	{
		return nullptr;
	}

	FScopedSlowTask SlowTask(100, LOCTEXT("ExportingBlueprintToWorld", "Exporting Blueprint to World"));

	// Load Blueprint World Actors into the new Level 
	SlowTask.EnterProgressFrame(80, LOCTEXT("CopyingActors", "Copying Blueprint Actors"));
	{
		auto InitActor = [](AActor& InActor, const FAvaActorData& InActorData)
		{
			InActor.bHiddenEd = InActorData.bEditorVisibility;
		};

		InBlueprint->LoadAvalancheWorld(InNewLevel->OwningWorld, UAvalancheBlueprint::FInitActorFunctionRef(InitActor));
	}

	// Create Scene Actor in Level
	AAvaScene* Scene;
	SlowTask.EnterProgressFrame(5, LOCTEXT("CreatingScene", "Creating Level Scene"));
	{
		Scene = GetScene(InNewLevel, /*bInCreateSceneIfNotFound=*/true);
		check(Scene);

		Scene->SceneTree         = InBlueprint->GetSceneTree();
		Scene->OutlinerData      = InBlueprint->GetEditorData();
		Scene->StartupCameraName = InBlueprint->GetStartupCameraName();
	}

	// Copy Sequences
	SlowTask.EnterProgressFrame(10, LOCTEXT("CopyingSequences", "Copying Blueprint Sequences"));
	{
		// Copy to remove all invalid sequences
		TArray<UAvaSequence*> SourceSequences = InBlueprint->GetSequences();
		SourceSequences.RemoveAll([](UAvaSequence* InSequence) { return !IsValid(InSequence); });

		// Expectation is for this new scene to have no sequences
		ensureAlways(Scene->Animations.IsEmpty());
		Scene->Animations.Empty(SourceSequences.Num());
		Scene->DefaultSequenceIndex = SourceSequences.Find(InBlueprint->GetDefaultSequence());

		const FTopLevelAssetPath OldContext = UAvalancheBlueprint::GetWorldContextPath(InBlueprint);
		const FTopLevelAssetPath NewContext(InNewLevel->OwningWorld);

		for (UAvaSequence* const SourceSequence : SourceSequences)
		{
			UAvaSequence* const NewSequence = DuplicateObject<UAvaSequence>(SourceSequence, Scene);
			ensure(IsValid(NewSequence));

			NewSequence->MigrateLegacyBindings(/*PlaybackContext*/InNewLevel->OwningWorld);

			NewSequence->UpdateBindings(&OldContext, NewContext);

			// Instead of calling AAvaScene::AddSequence which has some check overhead and schedules a Tree Rebuild, handle here manually
			Scene->Animations.Add(NewSequence);
		}

		if (!Scene->Animations.IsValidIndex(Scene->DefaultSequenceIndex))
		{
			Scene->DefaultSequenceIndex = 0;
		}

		check(SourceSequences.Num() == Scene->Animations.Num());

		for (int32 Index = 0; Index < SourceSequences.Num(); ++Index)
		{
			UAvaSequence* const SourceSequence = SourceSequences[Index];
			UAvaSequence* const TargetSequence = Scene->Animations[Index];

			// On Duplicate, the Child Anim property is marked as Duplicate Transient so should've not duplicated over
			ensure(TargetSequence->GetChildren().IsEmpty());

			for (const TWeakObjectPtr<UAvaSequence>& SourceChildren : SourceSequence->GetChildren())
			{
				const int32 ChildIndex = SourceSequences.Find(SourceChildren.Get());
				if (ChildIndex != INDEX_NONE)
				{
					TargetSequence->AddChild(Scene->Animations[ChildIndex]);	
				}
			}
		}

		Scene->RebuildSequenceTree();
	}

	// Copy Remote Control Preset
	SlowTask.EnterProgressFrame(5, LOCTEXT("CopyingRemoteControlPreset", "Copying Blueprint Remote Control Preset"));
	{
		URemoteControlPreset* const SourcePreset = InBlueprint->GetRemoteControlPreset();
		if (IsValid(SourcePreset))
		{
			const EObjectFlags Flags   = IsValid(Scene->RemoteControlPreset) ? Scene->RemoteControlPreset->GetFlags() : SourcePreset->GetFlags();
			Scene->RemoteControlPreset = DuplicateObject<URemoteControlPreset>(SourcePreset, Scene);
			Scene->RemoteControlPreset->SetFlags(Flags);

			FMapProperty* LevelMapProperty  = UE::AvaCore::GetProperty<URemoteControlLevelDependantBinding, FMapProperty>(TEXT("SubLevelSelectionMapByPath"));
			FMapProperty* ObjectMapProperty = UE::AvaCore::GetProperty<URemoteControlLevelDependantBinding, FMapProperty>(TEXT("BoundObjectMapByPath"));
			FProperty* LastLevelProperty    = UE::AvaCore::GetProperty<URemoteControlLevelDependantBinding>(TEXT("LevelWithLastSuccessfulResolve"));
			FProperty* LastObjectProperty   = UE::AvaCore::GetProperty<URemoteControlBinding>(TEXT("LastBoundObjectPath"));

			FSoftObjectPath NullObjectPath;
			FSoftObjectPtr NullObjectPtr;

			auto ResetMap = [](UObject* InObject, FMapProperty* InMapProperty)
			{
				FScriptMapHelper MapHelper(InMapProperty, InMapProperty->ContainerPtrToValuePtr<void*>(InObject));
				MapHelper.EmptyValues();
			};

			for (TArray<TObjectPtr<URemoteControlBinding>>::TIterator BindingIter =  Scene->RemoteControlPreset->Bindings.CreateIterator(); BindingIter; ++BindingIter)
			{
				if (URemoteControlLevelDependantBinding* LevelBinding = Cast<URemoteControlLevelDependantBinding>(*BindingIter))
				{
					ResetMap(LevelBinding, LevelMapProperty);
					ResetMap(LevelBinding, ObjectMapProperty);

					LastLevelProperty->SetValue_InContainer(LevelBinding, &NullObjectPtr);
					LastObjectProperty->SetValue_InContainer(LevelBinding, &NullObjectPath);
				}
			}

			Scene->RemoteControlPreset->RebindUnboundEntities();
		}
	}

	return Scene;
}
#endif

AAvaScene::AAvaScene()
{
	SceneSettings = CreateDefaultSubobject<UAvaSceneSettings>(TEXT("SceneSettings"));

	SceneState = CreateDefaultSubobject<UAvaSceneState>(TEXT("SceneState"));

	RemoteControlPreset = CreateDefaultSubobject<URemoteControlPreset>(TEXT("RemoteControlPreset"));

	StartupCameraName = NAME_None;

#if WITH_EDITOR
	if (!HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
	{
		PreWorldRenameDelegate = FWorldDelegates::OnPreWorldRename.AddUObject(this, &AAvaScene::OnWorldRenamed);
		WorldTagGetterDelegate = UObject::FAssetRegistryTag::OnGetExtraObjectTagsWithContext.AddUObject(this, &AAvaScene::OnGetWorldTags);
	}
#endif
}

IAvaSequencePlaybackObject* AAvaScene::GetScenePlayback() const
{
	// If the saved scene playback is valid use that
	if (IAvaSequencePlaybackObject* const ScenePlayback = PlaybackObject.GetInterface())
	{
		return ScenePlayback;
	}

	UAvaSequenceSubsystem* const SequenceSubsystem = UAvaSequenceSubsystem::Get(GetWorld());
	if (!SequenceSubsystem)
	{
		return nullptr;
	}

	AAvaScene& MutableThis = *const_cast<AAvaScene*>(this);
	if (IAvaSequencePlaybackObject* const ScenePlayback = SequenceSubsystem->FindOrCreatePlaybackObject(GetLevel(), MutableThis))
	{
		MutableThis.PlaybackObject.SetObject(ScenePlayback->ToUObject());
		MutableThis.PlaybackObject.SetInterface(ScenePlayback);
		return ScenePlayback;
	}

	return nullptr;
}

#if WITH_EDITOR
void AAvaScene::OnWorldRenamed(UWorld* InWorld, const TCHAR* InName, UObject* InNewOuter, ERenameFlags InFlags, bool& bOutShouldFailRename)
{
	if (FUObjectThreadContext::Get().IsRoutingPostLoad || InWorld != GetWorld())
	{
		return;
	}

	for (UAvaSequence* Sequence : Animations)
	{
		if (Sequence)
		{
			Sequence->OnOuterWorldRenamed(InName, InNewOuter, InFlags, bOutShouldFailRename);
		}
	}
}

void AAvaScene::OnGetWorldTags(FAssetRegistryTagsContext Context) const
{
	const UObject* InWorld = Context.GetObject();
	if (InWorld != GetTypedOuter<UWorld>())
	{
		return;
	}

	using namespace UE::Ava;
	Context.AddTag(UObject::FAssetRegistryTag(AssetTags::AvalancheScene, AssetTags::Values::Enabled, UObject::FAssetRegistryTag::TT_Alphabetical));
}
#endif

ULevel* AAvaScene::GetSceneLevel() const
{
	return GetLevel();
}

IAvaSequencePlaybackObject* AAvaScene::GetPlaybackObject() const
{
	return GetScenePlayback();
}

UObject* AAvaScene::ToUObject()
{
	return this;
}

UWorld* AAvaScene::GetContextWorld() const
{
	return GetWorld();
}

bool AAvaScene::CreateDirectorInstance(UAvaSequence& InSequence
	, IMovieScenePlayer& InPlayer
	, const FMovieSceneSequenceID& InSequenceID
	, UObject*& OutDirectorInstance)
{
	// Allow ULevelSequence::CreateDirectorInstance to be called
	return false;
}

bool AAvaScene::AddSequence(UAvaSequence* InSequence)
{
	if (IsValid(InSequence) && !Animations.Contains(InSequence))
	{
		Animations.Add(InSequence);
		ScheduleRebuildSequenceTree();
		return true;
	}
	return false;
}

void AAvaScene::RemoveSequence(UAvaSequence* InSequence)
{
	Animations.Remove(InSequence);
	if (InSequence)
	{
		InSequence->OnSequenceRemoved();
	}
	ScheduleRebuildSequenceTree();
}

void AAvaScene::SetDefaultSequence(UAvaSequence* InSequence)
{
	if (IsValid(InSequence))
	{
		AddSequence(InSequence);
		DefaultSequenceIndex = Animations.Find(InSequence);
	}
}

UAvaSequence* AAvaScene::GetDefaultSequence() const
{
	if (Animations.IsValidIndex(DefaultSequenceIndex))
	{
		return Animations[DefaultSequenceIndex];
	}
	return nullptr;
}

FName AAvaScene::GetSequenceProviderDebugName() const
{
	return GetFName();
}

#if WITH_EDITOR
void AAvaScene::OnEditorSequencerCreated(const TSharedPtr<ISequencer>& InSequencer)
{
	EditorSequencer = InSequencer;
	RebuildSequenceTree();
}
#endif

void AAvaScene::ScheduleRebuildSequenceTree()
{
	// Bail if the Deferred Rebuild is pending and hasn't executed yet
	if (bPendingAnimTreeUpdate)
	{
		return;
	}

	bPendingAnimTreeUpdate = true;

	TWeakObjectPtr<AAvaScene> ThisWeak(this);

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[ThisWeak](float InDeltaTime)->bool
		{
			AAvaScene* const This = ThisWeak.Get();

			// Check if we already have Rebuilt the Animation Tree in between when this was added and when it was executed
			if (This && This->bPendingAnimTreeUpdate)
			{
				This->RebuildSequenceTree();
			}

			// Return false for one time execution
			return false;
		}));
}

void AAvaScene::RebuildSequenceTree()
{
	bPendingAnimTreeUpdate = false;
	IAvaSequenceProvider::RebuildSequenceTree();
}

void AAvaScene::BeginPlay()
{
	Super::BeginPlay();

	if (SceneState)
	{
		SceneState->OnBeginPlay(SceneSettings);
	}
}

void AAvaScene::PostActorCreated()
{
	Super::PostActorCreated();

	if (!HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
	{
		FAvaRemoteControlUtils::RegisterRemoteControlPreset(RemoteControlPreset, /*bInEnsureUniqueId*/ false);
	}

	// Register AAvaScenes created past Subsystem Initialization
	if (UAvaSceneSubsystem* SceneSubsystem = FAvaWorldSubsystemUtils::GetWorldSubsystem<UAvaSceneSubsystem>(this))
	{
		SceneSubsystem->RegisterSceneInterface(GetLevel(), this);
	}
}

void AAvaScene::PostLoad()
{
	Super::PostLoad();

	if (!HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
	{
		FAvaRemoteControlUtils::RegisterRemoteControlPreset(RemoteControlPreset, /*bInEnsureUniqueId*/ true);
	}

	// Register AAvaScenes created past Subsystem Initialization
	if (UAvaSceneSubsystem* SceneSubsystem = FAvaWorldSubsystemUtils::GetWorldSubsystem<UAvaSceneSubsystem>(this))
	{
		SceneSubsystem->RegisterSceneInterface(GetLevel(), this);
	}
}

void AAvaScene::PostDuplicate(EDuplicateMode::Type InDuplicateMode)
{
	Super::PostDuplicate(InDuplicateMode);
	if (!HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
	{
		FAvaRemoteControlUtils::RegisterRemoteControlPreset(RemoteControlPreset, /*bInEnsureUniqueId*/ true);
	}
}

void AAvaScene::PostEditImport()
{
	Super::PostEditImport();
	if (!HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
	{
		FAvaRemoteControlUtils::RegisterRemoteControlPreset(RemoteControlPreset, /*bInEnsureUniqueId*/ true);
	}
}

void AAvaScene::BeginDestroy()
{
	Super::BeginDestroy();

	if (!HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
	{
		FAvaRemoteControlUtils::UnregisterRemoteControlPreset(RemoteControlPreset);
	}

#if WITH_EDITOR
	FWorldDelegates::OnPreWorldRename.Remove(PreWorldRenameDelegate);
	PreWorldRenameDelegate.Reset();

	UObject::FAssetRegistryTag::OnGetExtraObjectTagsWithContext.Remove(WorldTagGetterDelegate);
	WorldTagGetterDelegate.Reset();
#endif
}

#if WITH_EDITOR
void AAvaScene::SetStartupCameraName(FName InName)
{
	StartupCameraName = InName;
}

void AAvaScene::SetViewportGuideData(const TConstArrayView<FAvaViewportGuideInfo_Deprecated>& InGuideData)
{
	GuideData = InGuideData;
}
#endif

#undef LOCTEXT_NAMESPACE
