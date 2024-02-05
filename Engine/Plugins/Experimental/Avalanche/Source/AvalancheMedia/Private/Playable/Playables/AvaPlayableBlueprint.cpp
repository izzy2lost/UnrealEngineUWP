// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playable/Playables/AvaPlayableBlueprint.h"

#include "AvaBlueprint.h"
#include "AvaRemoteControlRebind.h"
#include "AvaSequencePlaybackObject.h"
#include "Framework/AvaSoftAssetPtr.h"
#include "Playable/AvaPlayableGroup.h"
#include "Playable/AvaPlayableGroupManager.h"
#include "Viewport/AvaCameraManager.h"

#define LOCTEXT_NAMESPACE "AvaPlayableBlueprint"

bool UAvaPlayableBlueprint::LoadAsset(const FAvaSoftAssetPtr& InSourceAsset, bool bInInitiallyVisible)
{
	if (!PlayableGroup)
	{
		return false;
	}

	// Ensure world is created. Does nothing if already created.
	PlayableGroup->ConditionalCreateWorld(); 

	check(InSourceAsset.GetAssetType() == EAvalancheAssetType::Blueprint);
	return LoadBlueprintInternal(TSoftObjectPtr<UAvalancheBlueprint>(InSourceAsset.ToSoftObjectPath()));
}

bool UAvaPlayableBlueprint::UnloadAsset()
{
	if (IsValid(ManagedBlueprint.Get()))
	{
		ManagedBlueprint->UnregisterRemoteControlPreset();
	}
	SourceBlueprint.Reset();
	ManagedBlueprint = nullptr;
	return true;
}

const FSoftObjectPath& UAvaPlayableBlueprint::GetSourceAssetPath() const
{
	return SourceBlueprint.ToSoftObjectPath();
}

EAvaPlayableStatus UAvaPlayableBlueprint::GetPlayableStatus() const
{
	// The blueprint assets don't stream. We can't even make them visible/invisible.
	return ManagedBlueprint ? EAvaPlayableStatus::Visible : EAvaPlayableStatus::Unloaded;
}

IAvaSceneInterface* UAvaPlayableBlueprint::GetSceneInterface() const
{
	if (ManagedBlueprint)
	{
		return static_cast<IAvaSceneInterface*>(ManagedBlueprint);
	}
	return nullptr;
}

EAvaPlayableCommandResult UAvaPlayableBlueprint::ExecuteAnimationCommand(EAvaPlaybackAnimAction InAnimAction, const FAvaPlaybackAnimPlaySettings& InAnimPlaySettings)
{
	if (InAnimAction ==  EAvaPlaybackAnimAction::CameraCut)
	{
		if (GetPlayableGroup())
		{
			GetPlayableGroup()->QueueCameraCut();
		}
		return EAvaPlayableCommandResult::Executed;
	}
	
	return Super::ExecuteAnimationCommand(InAnimAction, InAnimPlaySettings);
}

bool UAvaPlayableBlueprint::InitPlayable(const FPlayableCreationInfo& InPlayableInfo)
{
	// Motion Design blueprint requires a separate playable group for each asset.
	
	UAvaPlayableGroup::FPlayableGroupCreationInfo PlayableGroupCreationInfo;
	PlayableGroupCreationInfo.PlayableGroupManager = InPlayableInfo.PlayableGroupManager;
	PlayableGroupCreationInfo.SourceAssetPath = InPlayableInfo.SourceAsset.ToSoftObjectPath();
	PlayableGroupCreationInfo.ChannelName = InPlayableInfo.ChannelName;
	PlayableGroupCreationInfo.bIsRemoteProxy = false;
	PlayableGroupCreationInfo.bIsSharedGroup = false;

	PlayableGroup = InPlayableInfo.PlayableGroup ?
		InPlayableInfo.PlayableGroup : UAvaPlayableGroup::MakePlayableGroup(InPlayableInfo.PlayableGroupManager, PlayableGroupCreationInfo);
	
	return Super::InitPlayable(InPlayableInfo);
}

void UAvaPlayableBlueprint::OnPlay()
{
	if (!GetPlayableGroup())
	{
		return;
	}

	const UAvaGameInstance* AvaGameInstance = Cast<UAvaGameInstance>(GetPlayableGroup()->GetGameInstance());
	if (!AvaGameInstance)
	{
		return;
	}
	
	UAvaGameViewportClient* ViewportClient = AvaGameInstance->GetAvaGameViewportClient();

	// Verify if this can be changed on the fly.
	ManagedBlueprint->GetViewportQualitySettings().Apply(ViewportClient->EngineShowFlags);
	
	// Old code using ava camera manager. To retire.
	constexpr bool bIsCanvasController = false;
	ViewportClient->GetCameraManager()->Init(ManagedBlueprint->GetPlaybackObject(), bIsCanvasController);
#if WITH_EDITOR
	ViewportClient->GetCameraManager()->SetDefaultViewTarget(GetPlayWorld(), ManagedBlueprint->GetStartupCameraName());
#endif
}

void UAvaPlayableBlueprint::OnEndPlay()
{
	if (ManagedBlueprint)
	{
		if (IAvaSequencePlaybackObject* PlaybackObject = ManagedBlueprint->GetPlaybackObject())
		{
			PlaybackObject->CleanupPlayers();
		}
	}
}

bool UAvaPlayableBlueprint::LoadBlueprintInternal(const TSoftObjectPtr<UAvalancheBlueprint>& InSourceBlueprint)
{
	if (!GetPlayableGroup())
	{
		return false;
	}

	UAvaGameInstance* AvaGameInstance = Cast<UAvaGameInstance>(GetPlayableGroup()->GetGameInstance());
	if (!AvaGameInstance)
	{
		return false;
	}
	
	UAvalancheBlueprint* Source;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UAvaPlayableBlueprint::LoadAsset::LoadSourceAvaBp);
		Source = InSourceBlueprint.LoadSynchronous();
		check(Source);
	}

#if WITH_EDITORONLY_DATA
	FGuardValue_Bitfield(Source->bDuplicatingReadOnly, true);
#endif

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UAvaPlayableBlueprint::LoadAsset::DupAvaBp);
		const FName AvaInstanceName = MakeUniqueObjectName(AvaGameInstance, UAvalancheBlueprint::StaticClass(), Source->GetFName());
		ManagedBlueprint = Cast<UAvalancheBlueprint>(StaticDuplicateObject(Source, AvaGameInstance, AvaInstanceName, RF_Transient));
	}

	if (!ManagedBlueprint)
	{
		return false;
	}
	
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UAvaPlayableBlueprint::LoadAsset::LoadAvaBpWorld);
		ManagedBlueprint->SetAvalancheWorld(AvaGameInstance->GetPlayWorld());
		ManagedBlueprint->LoadAvalancheWorld();
	}

	ManagedBlueprint->UpdatePlaceholderActor();
	
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UAvaPlayableBlueprint::LoadAsset::RebindRCP);
		ManagedBlueprint->RegisterRemoteControlPreset();
		FAvaRemoteControlRebind::RebindUnboundEntities(ManagedBlueprint->GetRemoteControlPreset());
	}

	AvaGameInstance->MarkSynchronousAssetLoadingThisFrame();
	
	SourceBlueprint = InSourceBlueprint;
	return true;
}

#undef LOCTEXT_NAMESPACE
