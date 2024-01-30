// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Playables/AvalancheLevelStreamingPlayable.h"

#include "AvaRemoteControlRebind.h"
#include "AvaRemoteControlUtils.h"
#include "AvaScene.h"
#include "Camera/CameraActor.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/LocalPlayer.h"
#include "Framework/AvaSoftAssetPtr.h"
#include "Framework/AvalancheInstanceSettings.h"
#include "IAvaMediaModule.h"
#include "Playback/AvaMediaPlayableGroup.h"
#include "Playback/AvaMediaPlayableGroupManager.h"
#include "Playback/AvaMediaPlaybackUtils.h"
#include "Streaming/LevelStreamingDelegates.h"
#include "UObject/Package.h"
#include "Viewport/AvaCameraManager.h"

#define LOCTEXT_NAMESPACE "AvalancheLevelStreamingPlayable"

namespace UE::AvaMedia::LevelStreamingPlayable::Private
{
	AAvaScene* FindAvaScene(const ULevel* InLevel)
	{
		AAvaScene* AvaScene = nullptr;
		InLevel->Actors.FindItemByClass(&AvaScene);
		return AvaScene;
	}

	AActor* FindActorByName(const ULevel* InLevel, const FName& InActorName)
	{
		for (AActor* Actor : InLevel->Actors)
		{
			if (Actor && Actor->GetFName() == InActorName)
			{
				return Actor;
			}
		}
		return nullptr;
	}

	bool IsCameraActor(const AActor* InActor)
	{
		return InActor->IsA<ACameraActor>();
	}
	
	AActor* FindFirstCameraActor(const ULevel* InLevel)
	{
		for (AActor* Actor : InLevel->Actors)
		{
			if (Actor && IsCameraActor(Actor))
			{
				return Actor;
			}
		}
		return nullptr;
	}

	FName GetStartupCameraName(const AAvaScene* InAvaScene)
	{
		return InAvaScene ? InAvaScene->GetStartupCameraName() : NAME_None;
	}

	AActor* FindCameraActor(const ULevel* InLevel, FName InStartupCameraName)
	{
		if (InStartupCameraName != NAME_None)
		{
			AActor* CameraActor = FindActorByName(InLevel, InStartupCameraName);
			if (CameraActor && IsCameraActor(CameraActor))
			{
				return CameraActor;
			}
		}
		return nullptr;
	}

	AActor* FindStartupCameraActor(const ULevel* InLevel, FName InStartupCameraName, const AAvaScene* InAvaScene)
	{
		if (!IsValid(InLevel))
		{
			return nullptr;
		}

		// 1- Find by name from the parent scene.
		if (AActor* FoundCameraActor = FindCameraActor(InLevel, InStartupCameraName))
		{
			return FoundCameraActor;
		}

		// 2- Find by name from dependent scene (if any). Dependent levels may not have an ava scene.
		if (AActor* FoundCameraActor = FindCameraActor(InLevel, GetStartupCameraName(InAvaScene)))
		{
			return FoundCameraActor;
		}

		// 3- For now we fallback to looking for the first camera actor in the level.
		return FindFirstCameraActor(InLevel);
	}

	EAvalanchePlayableStatus GetPlayableStatusFromLevelStreaming(const ULevelStreaming* InLevelStreaming)
	{
		if (!InLevelStreaming)
		{
			return EAvalanchePlayableStatus::Unloaded;
		}

		switch (InLevelStreaming->GetLevelStreamingState())
		{
			case ELevelStreamingState::Removed:
				return EAvalanchePlayableStatus::Unloaded;
			
			case ELevelStreamingState::Unloaded:
				// If the LevelStreaming was not loaded and has just been made to be loading, the status will still be "unloaded" but
				// we consider it is loading.
				return InLevelStreaming->ShouldBeLoaded() ? EAvalanchePlayableStatus::Loading : EAvalanchePlayableStatus::Unloaded;
			
			case ELevelStreamingState::FailedToLoad:
				return EAvalanchePlayableStatus::Error;
			
			case ELevelStreamingState::Loading:
				return EAvalanchePlayableStatus::Loading;
			
			case ELevelStreamingState::LoadedNotVisible:
			case ELevelStreamingState::MakingVisible:
			case ELevelStreamingState::MakingInvisible:
				return EAvalanchePlayableStatus::Loaded;
			
			case ELevelStreamingState::LoadedVisible:
				return EAvalanchePlayableStatus::Visible;
			
			default:
				return EAvalanchePlayableStatus::Error;
		}
	}
	
	static int32 GetPlayableStatusRelevanceShouldBeUnloaded(EAvalanchePlayableStatus InStatus)
	{
		switch (InStatus)
		{
			case EAvalanchePlayableStatus::Unknown:
				return 0;
			case EAvalanchePlayableStatus::Error:
				return 1;
			case EAvalanchePlayableStatus::Unloaded:
				return 2; // Weakest (desired)
			case EAvalanchePlayableStatus::Loading:
				return 3;
			case EAvalanchePlayableStatus::Loaded:
				return 4;
			case EAvalanchePlayableStatus::Visible:
				return 5;
			default:
				return 0;
		}
	}

	static int32 GetPlayableStatusRelevanceShouldBeLoadedOrVisible(EAvalanchePlayableStatus InStatus)
	{
		switch (InStatus)
		{
			case EAvalanchePlayableStatus::Unknown:
				return 0;
			case EAvalanchePlayableStatus::Error:
				return 1;
			case EAvalanchePlayableStatus::Unloaded:
				return 5;
			case EAvalanchePlayableStatus::Loading:
				return 4;
			case EAvalanchePlayableStatus::Loaded:
				return 3;
			case EAvalanchePlayableStatus::Visible:
				return 2; // Weakest (desired)
			default:
				return 0;
		}
	}
	
	/**
	 * Returns the relevance of the status for comparison with another status
	 * in order to determine the combined status of a playable according to what it should be.
	 */
	static int32 GetPlayableStatusRelevance(EAvalanchePlayableStatus InStatus, bool bInShouldBeLoaded, bool bInShouldBeVisible)
	{
		// The "desired" status is the weakest of the valid statues.
		if (bInShouldBeLoaded || bInShouldBeVisible)
		{
			return GetPlayableStatusRelevanceShouldBeLoadedOrVisible(InStatus);
		}
		
		return GetPlayableStatusRelevanceShouldBeUnloaded(InStatus);
	}
}

bool UAvalancheLevelStreamingPlayable::LoadAsset(const FAvaSoftAssetPtr& InAvalancheSourceAsset, bool bInInitiallyVisible)
{
	if (!PlayableGroup)
	{
		return false;
	}

	// Ensure world is created. Does nothing if already created.
	PlayableGroup->ConditionalCreateWorld();

	const FAvalancheInstanceSettings& PlaybackInstanceSettings = IAvaMediaModule::Get().GetAvalancheInstanceSettings();
	bLoadSubPlayables = PlaybackInstanceSettings.bEnableLoadSubPlayables;
	
	check(InAvalancheSourceAsset.GetAssetType() == EAvalancheAssetType::World);
	const bool bAssetLoading = LoadAvalancheLevel(TSoftObjectPtr<UWorld>(InAvalancheSourceAsset.ToSoftObjectPath()), bInInitiallyVisible); 
	if (bAssetLoading)
	{
		PlayableGroup->NotifyLevelStreaming(this);
	}
	return bAssetLoading;
}

bool UAvalancheLevelStreamingPlayable::UnloadAsset()
{
	if (AvalancheScene)
	{
		FAvaRemoteControlUtils::UnregisterRemoteControlPreset(AvalancheScene->GetRemoteControlPreset());
	}

	UnloadSubPlayables();
	
	// Ripped from UPocketLevelInstance::BeginDestroy()
	if (LevelStreaming)
	{
		LevelStreaming->bShouldBlockOnUnload = false;
		LevelStreaming->SetShouldBeVisible(false);
		LevelStreaming->SetShouldBeLoaded(false);
		LevelStreaming->SetIsRequestingUnloadAndRemoval(true);
		LevelStreaming->OnLevelShown.RemoveAll(this);
		LevelStreaming->OnLevelLoaded.RemoveAll(this);

		const ULevel* const Level = LevelStreaming->GetLoadedLevel();
		if (IsValid(Level) && Level->GetPackage())
		{
			// Hack so that FLevelStreamingGCHelper::PrepareStreamedOutLevelForGC unloads the level.
			Level->GetPackage()->SetPackageFlags(PKG_PlayInEditor);
		}
	}
	LevelStreaming = nullptr;
	AvalancheScene = nullptr;
	SourceAvalancheLevel.Reset();
	return true;
}

EAvalanchePlayableStatus UAvalancheLevelStreamingPlayable::GetPlayableStatus() const
{
	using namespace UE::AvaMedia::LevelStreamingPlayable::Private;

	bool bShouldBeLoaded = LevelStreaming ? LevelStreaming->ShouldBeLoaded() : false;
	bool bShouldBeVisible = LevelStreaming ? LevelStreaming->ShouldBeVisible() : false;

	EAvalanchePlayableStatus MostRelevantStatus = EAvalanchePlayableStatus::Unknown;
	int32 HighestRelevance = -1;

	auto CompareAndSetMostRelevantStatus = [&MostRelevantStatus, &HighestRelevance, bShouldBeLoaded, bShouldBeVisible](EAvalanchePlayableStatus InStatus)
	{
		const int32 CurrentRelevance = GetPlayableStatusRelevance(InStatus, bShouldBeLoaded, bShouldBeVisible);
		if (CurrentRelevance > HighestRelevance)
		{
			MostRelevantStatus = InStatus;
			HighestRelevance = CurrentRelevance;
		}
	};
	
	// Check sub playables.
	for (const UAvalancheLevelStreamingPlayable* SubPlayable : SubPlayables)
	{
		CompareAndSetMostRelevantStatus(SubPlayable->GetPlayableStatus());
	}

	CompareAndSetMostRelevantStatus(GetPlayableStatusFromLevelStreaming(LevelStreaming));

	return MostRelevantStatus;
}

IAvaSceneInterface* UAvalancheLevelStreamingPlayable::GetSceneInterface() const
{
	if (AvalancheScene)
	{
		return static_cast<IAvaSceneInterface*>(AvalancheScene);
	}
	return nullptr;
}

bool UAvalancheLevelStreamingPlayable::ApplyCamera()
{
	using namespace UE::AvaMedia::LevelStreamingPlayable;

	if (!AvalancheScene)
	{
		return false;
	}

	bool bSetupDone = false;
	UAvalanchePlayable* PlayableOwningCamera = nullptr;

	// Camera setup.
	if (AActor* CameraActor = FindStartupCameraActor(NAME_None, &PlayableOwningCamera))
	{
		// New code using player controller.
		// Note: doesn't work yet because there is no player controller in avalanche game instance, but it works in PIE.
		if (APlayerController* PlayerController = GetPlayableGroup()->GetPlayWorld()->GetFirstPlayerController())
		{
			PlayerController->SetViewTargetWithBlend(CameraActor);
			GetPlayableGroup()->SetLastAppliedCameraPlayable(PlayableOwningCamera);
			bSetupDone = true;
		}
		else
		{
			// If there is no player controller, fallback to avalanche camera manager, if the game instance has it.
			const UAvalancheGameInstance* AvalancheGameInstance = Cast<UAvalancheGameInstance>(GetPlayableGroup()->GetGameInstance());			
			if (const UAvalancheGameViewportClient* ViewportClient = AvalancheGameInstance ? AvalancheGameInstance->GetAvalancheGameViewportClient() : nullptr)
			{
				// Old code using ava camera manager. To retire.
				constexpr bool bIsCanvasController = false;
				ViewportClient->GetCameraManager()->Init(AvalancheScene->GetPlaybackObject(), bIsCanvasController);
#if WITH_EDITOR
				ViewportClient->GetCameraManager()->SetViewTarget(CameraActor);
#endif
				GetPlayableGroup()->SetLastAppliedCameraPlayable(PlayableOwningCamera);
				bSetupDone = true;
				UE_LOG(LogAvalanchePlayable, Log, TEXT("No player controller found in \"%s\" - Using Ava Camera Manager instead."), *LevelStreaming->PackageNameToLoad.ToString())
			}
			else
			{
				UE_LOG(LogAvalanchePlayable, Warning, TEXT("No player controller found in \"%s\" - Ava Camera Manager not available either."), *LevelStreaming->PackageNameToLoad.ToString());				
			}
		}
	}
	else
	{
		UE_LOG(LogAvalanchePlayable, Error, TEXT("Failed to find camera \"%s\", or any camera actor, in loaded level."),
			*Private::GetStartupCameraName(AvalancheScene).ToString());
	}
	return bSetupDone;
}

bool UAvalancheLevelStreamingPlayable::GetShouldBeVisible() const
{
	return LevelStreaming ? LevelStreaming->GetShouldBeVisibleFlag() : false;
}

void UAvalancheLevelStreamingPlayable::SetShouldBeVisible(bool bInShouldBeVisible)
{
	if (LevelStreaming)
	{
		LevelStreaming->SetShouldBeVisible(bInShouldBeVisible);
	}

	for (UAvalancheLevelStreamingPlayable* SubPlayable : SubPlayables)
	{
		SubPlayable->UpdateVisibilityFromParents();
	}
}

bool UAvalancheLevelStreamingPlayable::LoadAvalancheLevel(const TSoftObjectPtr<UWorld>& InSourceAvalancheLevel, bool bInInitiallyVisible)
{
	if (SourceAvalancheLevel == InSourceAvalancheLevel)
	{
		// Already started loading.
		return false;
	}

	if (!PlayableGroup)
	{
		return false;
	}
	
	bool bSuccess = false;
	const FVector Origin(0,0,0);

	ULevelStreamingDynamic::FLoadLevelInstanceParams Params(PlayableGroup->GetPlayWorld()
			, InSourceAvalancheLevel.GetLongPackageName()
			, FTransform(FRotator::ZeroRotator, Origin));
	Params.bLoadAsTempPackage = true;
	Params.bInitiallyVisible  = bInInitiallyVisible;

	LevelStreaming = ULevelStreamingDynamic::LoadLevelInstance(Params, bSuccess);

	if (!bSuccess || !LevelStreaming)
	{
		UE_LOG(LogAvalanchePlayable, Error, TEXT("[%s]: Failed to load level instance `%s`."), *GetPathNameSafe(this), *InSourceAvalancheLevel.ToString());
		return false;
	}

	SourceAvalancheLevel = InSourceAvalancheLevel;
	LevelStreaming->SetShouldBeLoaded(true);
	LevelStreaming->SetShouldBeVisible(bInInitiallyVisible);
	return true;
}

void UAvalancheLevelStreamingPlayable::OnLevelStreamingStateChanged(UWorld* InWorld
	, const ULevelStreaming* InLevelStreaming
	, ULevel* InLevelIfLoaded
	, ELevelStreamingState InPreviousState
	, ELevelStreamingState InNewState)
{
	// Filter out levels we don't care about.
	if (InLevelStreaming != LevelStreaming)
	{
		return;
	}
	
	if (InNewState == ELevelStreamingState::FailedToLoad)
	{
		UE_LOG(LogAvalanchePlayable, Error, TEXT("Level \"%s\" failed to load."), *InLevelStreaming->PackageNameToLoad.ToString());		
	}
	else if (InNewState == ELevelStreamingState::LoadedNotVisible || InNewState == ELevelStreamingState::LoadedVisible)
	{
		const ULevel* const Level = LevelStreaming->GetLoadedLevel();
		if (!IsValid(Level))
		{
			return;
		}

		if (UWorld* OuterWorld = Level->GetTypedOuter<UWorld>())
		{
			// Workaround to avoid UEditorEngine::CheckForWorldGCLeaks killing the editor.
			// Change the sub-level's world to be a "persistent" world type.
			OuterWorld->WorldType = EWorldType::GamePreview;

			if (bLoadSubPlayables)
			{
				LoadSubPlayables(OuterWorld);
			}
		}

		// Workaround to destroy the Linker Load so that it does not keep the underlying File Opened
		if (Level->GetPackage())
		{
			FAvaMediaPlaybackUtils::FlushPackageLoading(Level->GetPackage());
		}
		
		// Resolve the ava scene for the other operations.
		ResolveAvalancheScene(Level);

		OnPlayableStatusChanged().Broadcast(this);

		for (const TObjectKey<UAvalancheLevelStreamingPlayable>& ParentPlayableKey : ParentPlayables)
		{
			if (UAvalancheLevelStreamingPlayable* ParentPlayable = ParentPlayableKey.ResolveObjectPtr())
			{
				ParentPlayable->OnLevelStreamingPlayableStatusChanged(this);
			}
		}

		OnLevelStreamingPlayableStatusChanged(this);
	}
}

void UAvalancheLevelStreamingPlayable::OnLevelStreamingPlayableStatusChanged(UAvalancheLevelStreamingPlayable* InPlayable)
{
	// OnPlay (camera setup, animations, etc) can only be done when the level is visible (components must be active).
	// With camera rig, we also need to make sure the rig level is loaded and visible.

	if (GetPlayableStatus() == EAvalanchePlayableStatus::Visible)
	{
		if (bOnPlayQueued)
		{
			bOnPlayQueued = false;
			OnPlay();
		}
	}
}

void UAvalancheLevelStreamingPlayable::BindDelegates()
{
	if (!FLevelStreamingDelegates::OnLevelStreamingStateChanged.IsBoundToObject(this))
	{
		FLevelStreamingDelegates::OnLevelStreamingStateChanged.AddUObject(this, &UAvalancheLevelStreamingPlayable::OnLevelStreamingStateChanged);
	}
}

void UAvalancheLevelStreamingPlayable::UnbindDelegates()
{
	FLevelStreamingDelegates::OnLevelStreamingStateChanged.RemoveAll(this);
}

ULevel* UAvalancheLevelStreamingPlayable::GetLoadedLevel() const
{
	if (!LevelStreaming)
	{
		return nullptr;	
	}
	
	ULevel* Level = LevelStreaming->GetLoadedLevel();
	return IsValid(Level) ? Level : nullptr;
}

void UAvalancheLevelStreamingPlayable::ResolveAvalancheScene(const ULevel* InLevel)
{
	if (!IsValid(AvalancheScene))
	{
		AvalancheScene = UE::AvaMedia::LevelStreamingPlayable::Private::FindAvaScene((InLevel));
		if (AvalancheScene)
		{
			FAvaRemoteControlUtils::RegisterRemoteControlPreset(AvalancheScene->GetRemoteControlPreset(), /*bInEnsureUniqueId*/ true);
			FAvaRemoteControlRebind::RebindUnboundEntities(AvalancheScene->GetRemoteControlPreset(), InLevel);
		}
		else
		{
			UE_LOG(LogAvalanchePlayable, Error, TEXT("Loaded level \"%s\" is not an Avalanche level."), *LevelStreaming->PackageNameToLoad.ToString());
		}
	}
}

AActor* UAvalancheLevelStreamingPlayable::FindStartupCameraActor(FName InStartupCameraName, UAvalanchePlayable** OutParentPlayable)
{
	using namespace UE::AvaMedia::LevelStreamingPlayable;

	// TODO: Configurable priorities for camera selection.
	
	if (InStartupCameraName == NAME_None && AvalancheScene)
	{
		InStartupCameraName = AvalancheScene->GetStartupCameraName();
	}
	
	if (AActor* FoundCameraActor = Private::FindStartupCameraActor(GetLoadedLevel(), InStartupCameraName, AvalancheScene))
	{
		if (OutParentPlayable)
		{
			*OutParentPlayable = this;
		}
		return FoundCameraActor;
	}

	for (UAvalancheLevelStreamingPlayable* SubPlayable : SubPlayables)
	{
		if (AActor* FoundCameraActor = SubPlayable->FindStartupCameraActor(InStartupCameraName, OutParentPlayable))
		{
			return FoundCameraActor;
		}
	}
	
	return nullptr;
}

bool UAvalancheLevelStreamingPlayable::InitPlayable(const FPlayableCreationInfo& InPlayableInfo)
{
	// For now, we share all the levels in the same instance group. We may do sub-grouping later.
	PlayableGroup = InPlayableInfo.PlayableGroup ?
		InPlayableInfo.PlayableGroup : InPlayableInfo.PlayableGroupManager->GetOrCreateSharedLevelGroup(InPlayableInfo.ChannelName, false);

	const bool bInitSuccess = Super::InitPlayable(InPlayableInfo);
	
	if (bInitSuccess)
	{
		BindDelegates();
	}
	return bInitSuccess;
}

void UAvalancheLevelStreamingPlayable::OnPlay()
{
	if (!LevelStreaming || !GetPlayableGroup())
	{
		return;
	}
	
	if (!LevelStreaming->GetShouldBeVisibleFlag())
	{
		// Can't make visible immediately if part of a transition with other playables and the others are not ready.
		GetPlayableGroup()->RequestSetVisibility(this, true);
	}

	const ULevel* const Level = LevelStreaming->GetLoadedLevel();
	if (!IsValid(Level))
	{
		// Level is not yet loaded, queue the action when it gets loaded.
		// Remark: we could either do this when the event is received or poll on the next tick.
		const ELevelStreamingState LevelStreamingState = LevelStreaming->GetLevelStreamingState();
		ensure(LevelStreaming->ShouldBeLoaded());
		if (LevelStreamingState != ELevelStreamingState::FailedToLoad)
		{
			bOnPlayQueued = true;
		}
		else
		{
			UE_LOG(LogAvalanchePlayable, Error, TEXT("Level \"%s\" is not loading. Current Streaming State: \"%s\"."),
				*LevelStreaming->PackageNameToLoad.ToString(), EnumToString(LevelStreamingState));			
		}
		return;
	}

	// Check if the level is visible. We can't do the actual camera setup, or animation
	// if the level is not yet visible as the components are inactive.
	const ELevelStreamingState LevelStreamingState = LevelStreaming->GetLevelStreamingState();
	if (LevelStreamingState != ELevelStreamingState::LoadedVisible)
	{
		bOnPlayQueued = true;
		return;
	}

	// Ensure scene is resolved.
	ResolveAvalancheScene(Level);

	if (!AvalancheScene)
	{
		return;
	}

	ApplyCamera();
}

void UAvalancheLevelStreamingPlayable::OnEndPlay()
{
	// Ensure the level is hidden and clear dirty flag because those are transient and shouldn't be saved.
	if (LevelStreaming)
	{
		const ULevel* const Level = LevelStreaming->GetLoadedLevel();
		if (IsValid(Level))
		{
			if (UPackage* LevelPackage = Level->GetPackage())
			{
				LevelPackage->ClearDirtyFlag();
			}
		}

		LevelStreaming->SetShouldBeVisible(false);
	}
}

void UAvalancheLevelStreamingPlayable::BeginDestroy()
{
	UnbindDelegates();
	Super::BeginDestroy();
}

void UAvalancheLevelStreamingPlayable::LoadSubPlayables(const UWorld* InLevelInstanceWorld)
{
	check(InLevelInstanceWorld);
	
	for (const ULevelStreaming* SubLevelStreaming : InLevelInstanceWorld->GetStreamingLevels())
	{
		if (SubLevelStreaming)
		{
			GetOrLoadSubPlayable(SubLevelStreaming);
		}
	}
}

void UAvalancheLevelStreamingPlayable::UnloadSubPlayables()
{
	for (UAvalancheLevelStreamingPlayable* SubPlayable : SubPlayables)
	{
		if (SubPlayable)
		{
			SubPlayable->ParentPlayables.Remove(this);

			// Shared Sub-Playables will be unloaded if they no longer have any parent
			// playables to keep them alive.
			if (!SubPlayable->HasParentPlayables())
			{
				SubPlayable->UnloadAsset();
				if (UAvaMediaPlayableGroup* ParentPlayableGroup = SubPlayable->GetPlayableGroup())
				{
					ParentPlayableGroup->UnregisterPlayable(SubPlayable);
				}
			}
		}
	}
	
	SubPlayables.Reset();
}

void UAvalancheLevelStreamingPlayable::GetOrLoadSubPlayable(const ULevelStreaming* InLevelStreaming)
{
	if (!PlayableGroup)
	{
		return;
	}

	const FSoftObjectPath SourceAssetPath = InLevelStreaming->GetWorldAsset().ToSoftObjectPath();
	
	// Assume sub playables are shared, i.e. unique instance per group. For now.
	// Need to see with Brad how to determine instancing scope (i.e. global vs local).	
	if (UAvalancheLevelStreamingPlayable* ExistingPlayable = Cast<UAvalancheLevelStreamingPlayable>(PlayableGroup->FindFirstPlayableBySourceAssetPath(SourceAssetPath)))
	{
		AddSubPlayable(ExistingPlayable);
		return;
	}

	if (UAvalancheLevelStreamingPlayable* NewPlayable = CreateSubPlayable(PlayableGroup, SourceAssetPath))
	{
		// TODO: Propagate more stuff from InLevelStreaming. Needs to reach LoadAvalancheLevel.
		const FAvaSoftAssetPtr AssetPtr = { UWorld::StaticClass(), TSoftObjectPtr(SourceAssetPath)};
		if (NewPlayable->LoadAsset(AssetPtr, GetShouldBeVisible()))
		{
			AddSubPlayable(NewPlayable);
		}
		else
		{
			PlayableGroup->UnregisterPlayable(NewPlayable);
		}
	}
}

UAvalancheLevelStreamingPlayable* UAvalancheLevelStreamingPlayable::CreateSubPlayable(UAvaMediaPlayableGroup* InPlayableGroup, const FSoftObjectPath& InSourceAssetPath)
{
	if (!InPlayableGroup)
	{
		return nullptr;
	}

	UAvalancheLevelStreamingPlayable* NewPlayable = NewObject<UAvalancheLevelStreamingPlayable>(GEngine);
	
	const FPlayableCreationInfo PlayableCreationInfo =
		{
			InPlayableGroup->GetPlayableGroupManager(),
			{ UWorld::StaticClass(), TSoftObjectPtr(InSourceAssetPath) },
			FName(),
			InPlayableGroup
		};
	
	if (NewPlayable && !NewPlayable->InitPlayable(PlayableCreationInfo))
	{
		// final setup may fail, in this case the playable is discarded.
		return nullptr;
	}

	return NewPlayable;
}

void UAvalancheLevelStreamingPlayable::AddSubPlayable(UAvalancheLevelStreamingPlayable* InSubPlayable)
{
	if (InSubPlayable)
	{
		SubPlayables.AddUnique(InSubPlayable);
		InSubPlayable->ParentPlayables.Add(this);
	}
}

void UAvalancheLevelStreamingPlayable::RemoveSubPlayable(UAvalancheLevelStreamingPlayable* InSubPlayable)
{
	if (InSubPlayable)
	{
		SubPlayables.Remove(InSubPlayable);
		InSubPlayable->ParentPlayables.Remove(this);
	}
}

bool UAvalancheLevelStreamingPlayable::HasParentPlayables() const
{
	for (const TObjectKey<UAvalancheLevelStreamingPlayable>& ParentPlayableKey : ParentPlayables)
	{
		if (ParentPlayableKey.ResolveObjectPtr())
		{
			return true;
		}
	}
	return false;
}

void UAvalancheLevelStreamingPlayable::UpdateVisibilityFromParents()
{
	bool bShouldBeVisible = false;
	
	for (const TObjectKey<UAvalancheLevelStreamingPlayable>& ParentPlayableKey : ParentPlayables)
	{
		if (const UAvalancheLevelStreamingPlayable* ParentPlayable = ParentPlayableKey.ResolveObjectPtr())
		{
			if (ParentPlayable->GetShouldBeVisible())
			{
				bShouldBeVisible = true;
				break;
			}
		}
	}

	SetShouldBeVisible(bShouldBeVisible);
}


#undef LOCTEXT_NAMESPACE
