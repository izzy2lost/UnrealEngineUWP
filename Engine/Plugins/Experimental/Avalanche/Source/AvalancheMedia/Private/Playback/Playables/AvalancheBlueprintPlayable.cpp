// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Playables/AvalancheBlueprintPlayable.h"

#include "AvaBlueprint.h"
#include "AvaRemoteControlRebind.h"
#include "AvaSequencePlaybackObject.h"
#include "Framework/AvaSoftAssetPtr.h"
#include "Playback/AvaMediaPlayableGroup.h"
#include "Playback/AvaMediaPlayableGroupManager.h"
#include "Viewport/AvaCameraManager.h"

#define LOCTEXT_NAMESPACE "AvalancheBlueprintPlayable"

bool UAvalancheBlueprintPlayable::LoadAsset(const FAvaSoftAssetPtr& InAvalancheSourceAsset, bool bInInitiallyVisible)
{
	if (!PlayableGroup)
	{
		return false;
	}

	// Ensure world is created. Does nothing if already created.
	PlayableGroup->ConditionalCreateWorld(); 

	check(InAvalancheSourceAsset.GetAssetType() == EAvalancheAssetType::Blueprint);
	return LoadAvalancheBlueprintInternal(TSoftObjectPtr<UAvalancheBlueprint>(InAvalancheSourceAsset.ToSoftObjectPath()));
}

bool UAvalancheBlueprintPlayable::UnloadAsset()
{
	if (IsValid(ManagedAvalancheBlueprint.Get()))
	{
		ManagedAvalancheBlueprint->UnregisterRemoteControlPreset();
	}
	SourceAvalancheBlueprint.Reset();
	ManagedAvalancheBlueprint = nullptr;
	return true;
}

const FSoftObjectPath& UAvalancheBlueprintPlayable::GetSourceAssetPath() const
{
	return SourceAvalancheBlueprint.ToSoftObjectPath();
}

EAvalanchePlayableStatus UAvalancheBlueprintPlayable::GetPlayableStatus() const
{
	// The blueprint assets don't stream. We can't even make them visible/invisible.
	return ManagedAvalancheBlueprint ? EAvalanchePlayableStatus::Visible : EAvalanchePlayableStatus::Unloaded;
}

IAvaSceneInterface* UAvalancheBlueprintPlayable::GetSceneInterface() const
{
	if (ManagedAvalancheBlueprint)
	{
		return static_cast<IAvaSceneInterface*>(ManagedAvalancheBlueprint);
	}
	return nullptr;
}

EAvalanchePlayableCommandResult UAvalancheBlueprintPlayable::ExecuteAnimationCommand(EAvaMediaAnimAction InAnimAction, const FAnimPlaySettings& InAnimPlaySettings)
{
	if (InAnimAction ==  EAvaMediaAnimAction::CameraCut)
	{
		if (GetPlayableGroup())
		{
			GetPlayableGroup()->QueueCameraCut();
		}
		return EAvalanchePlayableCommandResult::Executed;
	}
	
	return Super::ExecuteAnimationCommand(InAnimAction, InAnimPlaySettings);
}

bool UAvalancheBlueprintPlayable::InitPlayable(const FPlayableCreationInfo& InPlayableInfo)
{
	// Avalanche blueprint requires a separate playable group for each asset.
	
	UAvaMediaPlayableGroup::FPlayableGroupCreationInfo PlayableGroupCreationInfo;
	PlayableGroupCreationInfo.PlayableGroupManager = InPlayableInfo.PlayableGroupManager;
	PlayableGroupCreationInfo.SourceAssetPath = InPlayableInfo.SourceAsset.ToSoftObjectPath();
	PlayableGroupCreationInfo.ChannelName = InPlayableInfo.ChannelName;
	PlayableGroupCreationInfo.bIsRemoteProxy = false;
	PlayableGroupCreationInfo.bIsSharedGroup = false;

	PlayableGroup = InPlayableInfo.PlayableGroup ?
		InPlayableInfo.PlayableGroup : UAvaMediaPlayableGroup::MakePlayableGroup(InPlayableInfo.PlayableGroupManager, PlayableGroupCreationInfo);
	
	return Super::InitPlayable(InPlayableInfo);
}

void UAvalancheBlueprintPlayable::OnPlay()
{
	if (!GetPlayableGroup())
	{
		return;
	}

	const UAvalancheGameInstance* AvalancheGameInstance = Cast<UAvalancheGameInstance>(GetPlayableGroup()->GetGameInstance());
	if (!AvalancheGameInstance)
	{
		return;
	}
	
	UAvalancheGameViewportClient* ViewportClient = AvalancheGameInstance->GetAvalancheGameViewportClient();

	// Verify if this can be changed on the fly.
	ManagedAvalancheBlueprint->GetViewportQualitySettings().Apply(ViewportClient->EngineShowFlags);
	
	// Old code using ava camera manager. To retire.
	constexpr bool bIsCanvasController = false;
	ViewportClient->GetCameraManager()->Init(ManagedAvalancheBlueprint->GetPlaybackObject(), bIsCanvasController);
#if WITH_EDITOR
	ViewportClient->GetCameraManager()->SetDefaultViewTarget(GetPlayWorld(), ManagedAvalancheBlueprint->GetStartupCameraName());
#endif
}

void UAvalancheBlueprintPlayable::OnEndPlay()
{
	if (ManagedAvalancheBlueprint)
	{
		if (IAvaSequencePlaybackObject* PlaybackObject = ManagedAvalancheBlueprint->GetPlaybackObject())
		{
			PlaybackObject->CleanupPlayers();
		}
	}
}

bool UAvalancheBlueprintPlayable::LoadAvalancheBlueprintInternal(const TSoftObjectPtr<UAvalancheBlueprint>& InSourceAvalancheBlueprint)
{
	if (!GetPlayableGroup())
	{
		return false;
	}

	UAvalancheGameInstance* AvalancheGameInstance = Cast<UAvalancheGameInstance>(GetPlayableGroup()->GetGameInstance());
	if (!AvalancheGameInstance)
	{
		return false;
	}
	
	UAvalancheBlueprint* Source;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UAvalancheBlueprintPlayable::LoadAsset::LoadSourceAvaBp);
		Source = InSourceAvalancheBlueprint.LoadSynchronous();
		check(Source);
	}

#if WITH_EDITORONLY_DATA
	FGuardValue_Bitfield(Source->bDuplicatingReadOnly, true);
#endif

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UAvalancheBlueprintPlayable::LoadAsset::DupAvaBp);
		const FName AvaInstanceName = MakeUniqueObjectName(AvalancheGameInstance, UAvalancheBlueprint::StaticClass(), Source->GetFName());
		ManagedAvalancheBlueprint = Cast<UAvalancheBlueprint>(StaticDuplicateObject(Source, AvalancheGameInstance, AvaInstanceName, RF_Transient));
	}

	if (!ManagedAvalancheBlueprint)
	{
		return false;
	}
	
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UAvalancheBlueprintPlayable::LoadAsset::LoadAvaBpWorld);
		ManagedAvalancheBlueprint->SetAvalancheWorld(AvalancheGameInstance->GetPlayWorld());
		ManagedAvalancheBlueprint->LoadAvalancheWorld();
	}

	ManagedAvalancheBlueprint->UpdatePlaceholderActor();
	
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UAvalancheBlueprintPlayable::LoadAsset::RebindRCP);
		ManagedAvalancheBlueprint->RegisterRemoteControlPreset();
		FAvaRemoteControlRebind::RebindUnboundEntities(ManagedAvalancheBlueprint->GetRemoteControlPreset());
	}

	AvalancheGameInstance->MarkSynchronousAssetLoadingThisFrame();
	
	SourceAvalancheBlueprint = InSourceAvalancheBlueprint;
	return true;
}

#undef LOCTEXT_NAMESPACE
