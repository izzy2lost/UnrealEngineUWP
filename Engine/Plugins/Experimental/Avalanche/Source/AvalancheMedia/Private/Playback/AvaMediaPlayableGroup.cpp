// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/AvaMediaPlayableGroup.h"

#include "Engine/ViewportStatsSubsystem.h"
#include "Framework/AvalancheGameInstance.h"
#include "Playback/AvaMediaPlayableGroupManager.h"
#include "Playback/AvaMediaPlaybackUtils.h"
#include "Playback/AvalanchePlayable.h"
#include "Playback/Transition/AvalanchePlayableTransition.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "AvaMediaPlayableGroup"

namespace UE::AvalancheMedia::PlayableGroup::Private
{
	template<typename InElementType>
	void CleanStaleKeys(TSet<TObjectKey<InElementType>>& InOutSetToClean)
	{
		for (typename TSet<TObjectKey<InElementType>>::TIterator It(InOutSetToClean); It; ++It)
		{
			if (!It->ResolveObjectPtr())
			{
				It.RemoveCurrent();
			}
		}
	}
}

UPackage* UAvaMediaPlayableGroup::MakeGameInstancePackage(const FSoftObjectPath& InSourceAssetPath, const FName& InChannelName)
{
	// Remote Control Preset will be registered with the package name.
	// We want a package name unique to the game instance and that will be
	// human readable since it will show up in the Web Remote Control page.
	
	FString InstancePackageName = TEXT("/Temp/");	// Keep it short for web page.

	// Using channel name, since we should have one instance per channel.
	InstancePackageName += InChannelName.ToString();

	// In order to keep things short, we remove "/Game" since we added the channel name instead.
	FString InstanceSubPath;
	if (!InSourceAssetPath.GetLongPackageName().Split(TEXT("/Game"), nullptr, &InstanceSubPath))
	{
		InstanceSubPath = InSourceAssetPath.GetLongPackageName();
	}

	// This may happen if the original asset path is not specified.
	if (InstanceSubPath.IsEmpty())
	{
		// Add something to get a valid path at least.
		InstanceSubPath = TEXT("InvalidAssetName");
	}
	
	InstancePackageName += InstanceSubPath;

	return MakeInstancePackage(InstancePackageName);
}

UPackage* UAvaMediaPlayableGroup::MakeSharedInstancePackage(const FName& InChannelName)
{
	// Remote Control Preset will be registered with the package name.
	// We want a package name unique to the game instance and that will be
	// human readable since it will show up in the Web Remote Control page.
	
	FString SharedPackageName = TEXT("/Temp/");	// Keep it short for web page.

	// Using channel name, since we should have one instance per channel.
	SharedPackageName += InChannelName.ToString();

	// Shared for all levels.
	SharedPackageName += TEXT("/SharedLevels");
	
	return MakeInstancePackage(SharedPackageName);
}

UPackage* UAvaMediaPlayableGroup::MakeInstancePackage(const FString& InInstancePackageName)
{
	UPackage* InstancePackage = CreatePackage(*InInstancePackageName);
	if (InstancePackage)
	{
		InstancePackage->SetFlags(RF_Transient);
	}
	else
	{
		// Note: The outer will fallback to GEngine in that case.
		UE_LOG(LogAvalanchePlayable, Error, TEXT("Unable to create package \"%s\" for Motion Design Game Instance."), *InInstancePackageName);
	}
	return InstancePackage;
}

UAvaMediaPlayableGroup* UAvaMediaPlayableGroup::MakePlayableGroup(UObject* InOuter, const FPlayableGroupCreationInfo& InPlayableGroupInfo)
{
	UObject* Outer = InOuter ? InOuter : GetTransientPackage();
	
	UAvaMediaPlayableGroup* GameInstanceGroup;
	if (InPlayableGroupInfo.bIsRemoteProxy)
	{
		// Remote Proxy group doesn't have a game instance.
		GameInstanceGroup = NewObject<UAvaMediaRemoteProxyPlayableGroup>(Outer);
		GameInstanceGroup->ParentPlayableGroupManagerWeak = InPlayableGroupInfo.PlayableGroupManager;
	}
	else
	{
		GameInstanceGroup = NewObject<UAvaMediaPlayableGroup>(Outer);
		GameInstanceGroup->ParentPlayableGroupManagerWeak = InPlayableGroupInfo.PlayableGroupManager;
		
		if (InPlayableGroupInfo.bIsSharedGroup)
		{
			GameInstanceGroup->GameInstancePackage = MakeSharedInstancePackage(InPlayableGroupInfo.ChannelName);
		}
		else
		{
			// We can create the package even if the name is null, it will have a generic name. But that will be considered an error.
			if (!InPlayableGroupInfo.SourceAssetPath.IsNull())
			{
				UE_LOG(LogAvalanchePlayable, Error, TEXT("Creating game instance package for asset with unspecified name."));
			}
			GameInstanceGroup->GameInstancePackage = MakeGameInstancePackage(InPlayableGroupInfo.SourceAssetPath, InPlayableGroupInfo.ChannelName);
		}
		
		GameInstanceGroup->GameInstance = UAvalancheGameInstance::Create(GameInstanceGroup->GameInstancePackage);
	}
	return GameInstanceGroup;
}

void UAvaMediaPlayableGroup::RegisterPlayable(UAvalanchePlayable* InPlayable)
{
	// Prevent accumulation of stale keys.
	UE::AvalancheMedia::PlayableGroup::Private::CleanStaleKeys(Playables);

	if (InPlayable)
	{
		InPlayable->OnPlayableStatusChanged().AddUObject(this, &UAvaMediaPlayableGroup::OnPlayableStatusChanged);
		Playables.Add(InPlayable);
	}
}

/** Unregister a playable when it is about to be deleted. */
void UAvaMediaPlayableGroup::UnregisterPlayable(UAvalanchePlayable* InPlayable)
{
	if (InPlayable)
	{
		InPlayable->OnPlayableStatusChanged().RemoveAll(this);
		Playables.Remove(InPlayable);
	}
}

bool UAvaMediaPlayableGroup::HasPlayables() const
{
	for (const TObjectKey<UAvalanchePlayable>& PlayableKey : Playables)
	{
		if (PlayableKey.ResolveObjectPtr())
		{
			return true;
		}
	}
	return false;
}

bool UAvaMediaPlayableGroup::HasPlayingPlayables() const
{
	for (const TObjectKey<UAvalanchePlayable>& PlayableKey : Playables)
	{
		const UAvalanchePlayable* Playable = PlayableKey.ResolveObjectPtr();
		if (Playable && Playable->IsPlaying())
		{
			return true;
		}
	}
	return false;
}

UAvalanchePlayable* UAvaMediaPlayableGroup::FindFirstPlayableBySourceAssetPath(const FSoftObjectPath& InSourceAssetPath) const
{
	for (const TObjectKey<UAvalanchePlayable>& PlayableKey : Playables)
	{
		UAvalanchePlayable* Playable = PlayableKey.ResolveObjectPtr();
		if (Playable && Playable->GetSourceAssetPath() == InSourceAssetPath)
		{
			return Playable;
		}
	}
	return nullptr;
}

void UAvaMediaPlayableGroup::RegisterPlayableTransition(UAvalanchePlayableTransition* InPlayableTransition)
{
	if (InPlayableTransition)
	{
		// Protect PlayableTransitions iterator.
		if (bIsTickingTransitions)
		{
			PlayableTransitionsToRemove.Remove(InPlayableTransition);
			PlayableTransitionsToAdd.Add(InPlayableTransition);
			return;
		}

		// Prevent accumulation of stale keys.
		UE::AvalancheMedia::PlayableGroup::Private::CleanStaleKeys(PlayableTransitions);

		PlayableTransitions.Add(InPlayableTransition);
		
		if (UAvaMediaPlayableGroupManager* PlayableGroupManager = GetPlayableGroupManager())
		{
			PlayableGroupManager->RegisterForTransitionTicking(this);
		}
	}
}
	
void UAvaMediaPlayableGroup::UnregisterPlayableTransition(UAvalanchePlayableTransition* InPlayableTransition)
{
	if (InPlayableTransition)
	{
		// Protect PlayableTransitions iterator.
		if (bIsTickingTransitions)
		{
			PlayableTransitionsToAdd.Remove(InPlayableTransition);
			PlayableTransitionsToRemove.Add(InPlayableTransition);
			return;
		}
		
		PlayableTransitions.Remove(InPlayableTransition);

		UE::AvalancheMedia::PlayableGroup::Private::CleanStaleKeys(PlayableTransitions);
		
		if (PlayableTransitions.IsEmpty())
		{
			if (UAvaMediaPlayableGroupManager* PlayableGroupManager = GetPlayableGroupManager())
			{
				PlayableGroupManager->UnregisterFromTransitionTicking(this);
			}
		}
	}
}

void UAvaMediaPlayableGroup::TickTransitions(double InDeltaSeconds)
{
	// Ticking the transitions will lead to some transitions being removed.
	// So we need to protect the iterator with a scope and do the operations when iteration is done.
	{
		TGuardValue TickGuard(bIsTickingTransitions, true);
	
		for (TSet<TObjectKey<UAvalanchePlayableTransition>>::TIterator TickIt(PlayableTransitions); TickIt; ++TickIt)
		{
			if (UAvalanchePlayableTransition* TransitionToTick = TickIt->ResolveObjectPtr())
			{
				TransitionToTick->Tick(InDeltaSeconds);
			}
			else
			{
				// Prevent accumulation of stale keys.
				// Remark: if there is a stale key, it means the transition was not stopped properly.
				TickIt.RemoveCurrent();
			}
		}
	}
	
	for (const TObjectKey<UAvalanchePlayableTransition>& ToRemove : PlayableTransitionsToRemove)
	{
		UnregisterPlayableTransition(ToRemove.ResolveObjectPtr());
	}
	PlayableTransitionsToRemove.Reset();
	
	for (const TObjectKey<UAvalanchePlayableTransition>& ToAdd : PlayableTransitionsToAdd)
	{
		RegisterPlayableTransition(ToAdd.ResolveObjectPtr());
	}
	PlayableTransitionsToAdd.Reset();
}

bool UAvaMediaPlayableGroup::HasTransitions() const
{
	return !PlayableTransitions.IsEmpty();
}

bool UAvaMediaPlayableGroup::ConditionalCreateWorld()
{
	if (!GameInstance)
	{
		return false;
	}

	bool bWorldWasCreated = false;

	if (!GameInstance->IsWorldCreated())
	{
		bWorldWasCreated = GameInstance->CreateWorld();
	}
	
	// Make sure we register our delegates to this world.
	if (GameInstance->GetPlayWorld())
	{
		ConditionalRegisterWorldDelegates(GameInstance->GetPlayWorld());
	}
	
	return bWorldWasCreated;
}

bool UAvaMediaPlayableGroup::ConditionalBeginPlay(const FAvalancheInstancePlaySettings& InWorldPlaySettings)
{
	bool bHasBegunPlay = false;
	if (!GameInstance)
	{
		return bHasBegunPlay;
	}
	
	// Make sure we don't have pending unload or stop requests left over in the game instance.
	GameInstance->CancelWorldRequests();

	if (!GameInstance->IsWorldPlaying())
	{
		bHasBegunPlay = GameInstance->BeginPlayWorld(InWorldPlaySettings);
	}
	else
	{
		GameInstance->UpdateRenderTarget(InWorldPlaySettings.RenderTarget);
		GameInstance->UpdateSceneViewportSize(InWorldPlaySettings.ViewportSize);
	}
	return bHasBegunPlay;
}

void UAvaMediaPlayableGroup::RequestEndPlayWorld(bool bInForceImmediate)
{
	if (GameInstance)
	{
		GameInstance->RequestEndPlayWorld(bInForceImmediate);
	}
}

void UAvaMediaPlayableGroup::SetLastAppliedCameraPlayable(UAvalanchePlayable* InPlayable)
{
	LastAppliedCameraPlayableWeak = InPlayable;
}

bool UAvaMediaPlayableGroup::UpdateCameraSetup()
{
	// With rigs in sub-playables, it may still be valid and won't need updating.
	if (const UAvalanchePlayable* LastAppliedCameraPlayable = LastAppliedCameraPlayableWeak.Get())
	{
		if (LastAppliedCameraPlayable->GetPlayableStatus() == EAvalanchePlayableStatus::Visible
			&& LastAppliedCameraPlayable->GetShouldBeVisible())
		{
			return true;	// Camera setup is still valid.
		}
	}

	// This is called when a playable from the group is stopped. We need to
	// select a playable in the group that is still playing and will use it's camera.
	for (const TObjectKey<UAvalanchePlayable>& PlayableKey : Playables )
	{
		UAvalanchePlayable* Playable = PlayableKey.ResolveObjectPtr();
		
		if (Playable && Playable->IsPlaying())
		{
			if (Playable->ApplyCamera())
			{
				return true;
			}
		}
	}
	return false;
}

bool UAvaMediaPlayableGroup::ConditionalRequestUnloadWorld(bool bForceImmediate)
{
	if (!GameInstance)
	{
		return false;
	}
	
	if (!HasPlayables())
	{
		UnregisterWorldDelegates(GameInstance->GetPlayWorld());
		GameInstance->RequestUnloadWorld(bForceImmediate);
		return true;
	}
	return false;
}

void UAvaMediaPlayableGroup::QueueCameraCut()
{
	if (GameInstance)
	{
		if (UAvalancheGameViewportClient* GameViewportClient = GameInstance->GetAvalancheGameViewportClient())
		{
			GameViewportClient->SetCameraCutThisFrame();
		}
	}
}

void UAvaMediaPlayableGroup::NotifyLevelStreaming(UAvalanchePlayable* InPlayable)
{
	// If the world is not playing, we need to make sure the level streaming still updates.
	if (!IsWorldPlaying())
	{
		if (UAvaMediaPlayableGroupManager* PlayableGroupManager = GetPlayableGroupManager())
		{
			PlayableGroupManager->RegisterForLevelStreamingUpdate(this);
		}
	}
}

void UAvaMediaPlayableGroup::FVisibilityRequest::Execute(const UAvaMediaPlayableGroup* InPlayableGroup) const
{
	UAvalanchePlayable* Playable = PlayableWeak.Get();
	
	if (!Playable)
	{
		using namespace UE::AvaMediaPlayback::Utils;
		UE_LOG(LogAvalanchePlayable, Error,
			TEXT("%s Failed to Set Visibility to \"%s\" because the playable has become stale. Playable Group: \"%s\"."),
			*GetBriefFrameInfo(), bShouldBeVisible ? TEXT("true") : TEXT("false"), *InPlayableGroup->GetFullName());
	}

	Playable->SetShouldBeVisible(bShouldBeVisible);
}

void UAvaMediaPlayableGroup::RegisterVisibilityConstraint(const TWeakInterfacePtr<IAvaPlayableVisibilityConstraint>& InVisibilityConstraint)
{
	if (!VisibilityConstraints.Contains(InVisibilityConstraint))
	{
		VisibilityConstraints.RemoveAll([](const TWeakInterfacePtr<IAvaPlayableVisibilityConstraint>& InConstraint) { return InConstraint.IsStale();});
		VisibilityConstraints.Add(InVisibilityConstraint);
	}
}

void UAvaMediaPlayableGroup::UnregisterVisibilityConstraint(const IAvaPlayableVisibilityConstraint* InVisibilityConstraint)
{
	VisibilityConstraints.RemoveAll([InVisibilityConstraint](const TWeakInterfacePtr<IAvaPlayableVisibilityConstraint>& InConstraint)
	{
		// also remove stale pointers.
		return InConstraint.IsStale() || InConstraint.Get() == InVisibilityConstraint;
	});
}

void UAvaMediaPlayableGroup::RequestSetVisibility(UAvalanchePlayable* InPlayable, bool bInShouldBeVisible)
{
	FVisibilityRequest Request = { InPlayable, bInShouldBeVisible };

	if (IsVisibilityConstrained(InPlayable))
	{
		VisibilityRequests.Add(MoveTemp(Request));
	}
	else
	{
		Request.Execute(this);
	}
}

bool UAvaMediaPlayableGroup::IsVisibilityConstrained(const UAvalanchePlayable* InPlayable) const
{
	for (const TWeakInterfacePtr<IAvaPlayableVisibilityConstraint>& ConstraintWeak : VisibilityConstraints)
	{
		if (const IAvaPlayableVisibilityConstraint* Constraint = ConstraintWeak.Get())
		{
			if (Constraint->IsVisibilityConstrained(InPlayable))
			{
				return true;
			}
		}
	}
	return false;
}

void UAvaMediaPlayableGroup::OnPlayableStatusChanged(UAvalanchePlayable* InPlayable)
{
	// Evaluate the playable visibility requests
	for (TArray<FVisibilityRequest>::TIterator RequestIt(VisibilityRequests); RequestIt; ++RequestIt)
	{
		const UAvalanchePlayable* Playable = RequestIt->PlayableWeak.Get();
		if (Playable && IsVisibilityConstrained(Playable))
		{
			continue;
		}
		
		RequestIt->Execute(this);
		RequestIt.RemoveCurrent();
	}
}

void UAvaMediaPlayableGroup::ConditionalRegisterWorldDelegates(UWorld* InWorld)
{
	if (!DisplayDelegateIndices.IsEmpty() && LastWorldBoundToDisplayDelegates.Get() == InWorld)
	{
		return;
	}

	if (!DisplayDelegateIndices.IsEmpty() && LastWorldBoundToDisplayDelegates.IsValid())
	{
		UnregisterWorldDelegates(LastWorldBoundToDisplayDelegates.Get());
	}

	if (UViewportStatsSubsystem* ViewportSubsystem = InWorld->GetSubsystem<UViewportStatsSubsystem>())
	{
		DisplayDelegateIndices.Add(ViewportSubsystem->AddDisplayDelegate([this](FText& OutText, FLinearColor& OutColor)
		{
			return DisplayLoadedAssets(OutText, OutColor);
		}));
		DisplayDelegateIndices.Add(ViewportSubsystem->AddDisplayDelegate([this](FText& OutText, FLinearColor& OutColor)
		{
			return DisplayPlayingAssets(OutText, OutColor);
		}));
		DisplayDelegateIndices.Add(ViewportSubsystem->AddDisplayDelegate([this](FText& OutText, FLinearColor& OutColor)
		{
			return DisplayTransitions(OutText, OutColor);
		}));

		LastWorldBoundToDisplayDelegates = InWorld;
	}
}

void UAvaMediaPlayableGroup::UnregisterWorldDelegates(UWorld* InWorld)
{
	if (!DisplayDelegateIndices.IsEmpty())
	{
		// Check that we are indeed registered to the world
		check(LastWorldBoundToDisplayDelegates.Get() == InWorld);
	
		if (UViewportStatsSubsystem* ViewportSubsystem = InWorld->GetSubsystem<UViewportStatsSubsystem>())
		{
			// Remark: removing them in the reverse order they where added. It is using RemoveAtSwap()
			// so removing the first one would change the index of the next one. Removing a delegate in the
			// middle of the array will invalidate the indices above.
			for (int32 Index = DisplayDelegateIndices.Num()-1; Index >= 0; --Index)
			{
				ViewportSubsystem->RemoveDisplayDelegate(DisplayDelegateIndices[Index]);
			}
		}

		DisplayDelegateIndices.Reset();
		LastWorldBoundToDisplayDelegates.Reset();
	}

	check(LastWorldBoundToDisplayDelegates.Get() == nullptr);
}

bool UAvaMediaPlayableGroup::DisplayLoadedAssets(FText& OutText, FLinearColor& OutColor)
{
	FString AssetList;
	for (const TObjectKey<UAvalanchePlayable>& PlayableKey : Playables)
	{
		if (const UAvalanchePlayable* Playable = PlayableKey.ResolveObjectPtr())
		{
			AssetList += AssetList.IsEmpty() ? TEXT("") : TEXT(", ");
			AssetList += Playable->GetSourceAssetPath().GetAssetName();
		}
	}

	if (!AssetList.IsEmpty())
	{
		OutText = FText::Format(LOCTEXT("DisplayLoadedGraphics", "Loaded Graphic(s): {0}"), FText::FromString(AssetList));
		OutColor = FLinearColor::Red;
		return true;
	}
	return false;
}

bool UAvaMediaPlayableGroup::DisplayPlayingAssets(FText& OutText, FLinearColor& OutColor)
{
	FString AssetList;
	for (const TObjectKey<UAvalanchePlayable>& PlayableKey : Playables)
	{
		const UAvalanchePlayable* Playable = PlayableKey.ResolveObjectPtr();
		if (Playable && Playable->IsPlaying())
		{
			AssetList += AssetList.IsEmpty() ? TEXT("") : TEXT(", ");
			AssetList += Playable->GetSourceAssetPath().GetAssetName();

			if (!Playable->GetUserData().IsEmpty())
			{
				AssetList += FString::Printf(TEXT(" (%s)"), *Playable->GetUserData());
			}
		}
	}

	if (!AssetList.IsEmpty())
	{
		OutText = FText::Format(LOCTEXT("DisplayPlayingGraphics", "Playing Graphic(s): {0}"), FText::FromString(AssetList));
		OutColor = FLinearColor::Green;
		return true;
	}
	return false;
}

bool UAvaMediaPlayableGroup::DisplayTransitions(FText& OutText, FLinearColor& OutColor)
{
	FString TransitionList;
	for (const TObjectKey<UAvalanchePlayableTransition>& TransitionKey : PlayableTransitions)
	{
		const UAvalanchePlayableTransition* Transition = TransitionKey.ResolveObjectPtr();
		if (Transition && Transition->IsRunning())
		{
			TransitionList += TransitionList.IsEmpty() ? TEXT("") : TEXT(", ");
			TransitionList += Transition->GetPrettyInfo();
		}
	}

	if (!TransitionList.IsEmpty())
	{
		OutText = FText::Format(LOCTEXT("DisplayTransitions", "Transition(s): {0}"), FText::FromString(TransitionList));
		OutColor = FLinearColor::Green;
		return true;
	}
	return false;
}

bool UAvaMediaRemoteProxyPlayableGroup::ConditionalBeginPlay(const FAvalancheInstancePlaySettings& InWorldPlaySettings)
{
	if (!bIsPlaying)
	{
		bIsPlaying = true;
		return true;
	}
	return false;
}

void UAvaMediaRemoteProxyPlayableGroup::RequestEndPlayWorld(bool bInForceImmediate)
{
	bIsPlaying = false;
}

#undef LOCTEXT_NAMESPACE