// Copyright Epic Games, Inc. All Rights Reserved.

#include "RuntimeGen/PCGGenSourceManager.h"

#include "PCGWorldActor.h"
#include "RuntimeGen/GenSources/PCGGenSourceBase.h"
#include "RuntimeGen/GenSources/PCGGenSourceComponent.h"
#include "RuntimeGen/GenSources/PCGGenSourceEditorCamera.h"
#include "RuntimeGen/GenSources/PCGGenSourcePlayer.h"
#include "RuntimeGen/GenSources/PCGGenSourceWPStreamingSource.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "WorldPartition/WorldPartition.h"

#if WITH_EDITOR
#include "EditorViewportClient.h"
#endif

FPCGGenSourceManager::FPCGGenSourceManager(const UWorld* InWorld)
{
	// We capture the World so we can differentiate between editor-world and PIE-world Generation Sources.
	World = InWorld;

	// Collect PlayerControllers.
	FGameModeEvents::GameModePostLoginEvent.AddRaw(this, &FPCGGenSourceManager::OnGameModePostLogin);
	FGameModeEvents::GameModeLogoutEvent.AddRaw(this, &FPCGGenSourceManager::OnGameModePostLogout);
}

FPCGGenSourceManager::~FPCGGenSourceManager()
{
	FGameModeEvents::GameModePostLoginEvent.RemoveAll(this);
	FGameModeEvents::GameModeLogoutEvent.RemoveAll(this);
}

TSet<IPCGGenSourceBase*> FPCGGenSourceManager::GetGenSources(const APCGWorldActor* InPCGWorldActor) const
{
	// Some the the generation sources need to be queried each frame, so build the 'current' set now.
	TSet<IPCGGenSourceBase*> CurrentGenSources = GenSources;

	if (const UWorld* InWorld = InPCGWorldActor->GetWorld())
	{
		if (const UWorldPartition* WorldPartition = InWorld->GetWorldPartition())
		{
			// TODO: Grab StreamingSourceProviders instead of StreamingSources? This could allow us to avoid calling NewObject every tick.
			// Note: GetStreamingSources only works in GameWorld, so StreamingSources do not act as GenSources in editor.
			for (const FWorldPartitionStreamingSource& Source : WorldPartition->GetStreamingSources())
			{
				// StreamingSources live only for this tick, so they don't need to be rooted.
				UPCGGenSourceWPStreamingSource* GenSource = NewObject<UPCGGenSourceWPStreamingSource>();
				GenSource->StreamingSource = &Source;

				// TODO: Is it possible to avoid adding a StreamingSource for the Player, which we already capture in OnGameModePostLogin?
				CurrentGenSources.Add(GenSource);
			}
		}
	}

#if WITH_EDITOR
	if (InPCGWorldActor->bTreatEditorViewportAsGenerationSource && !World->IsGameWorld())
	{
		for (FEditorViewportClient* EditorViewportClient : GEditor->GetAllViewportClients())
		{
			if (EditorViewportClient->IsVisible())
			{
				// TODO: grab all 4 of these when GenSourceManager is created, instead of creating a new UObject for each every frame?
				UPCGGenSourceEditorCamera* GenSource = NewObject<UPCGGenSourceEditorCamera>();
				GenSource->EditorViewportClient = EditorViewportClient;

				CurrentGenSources.Add(GenSource);
			}
		}
	}
#endif

	return CurrentGenSources;
}

bool FPCGGenSourceManager::RegisterGenSource(IPCGGenSourceBase* InGenSource)
{
	if (UPCGGenSourceComponent* GenSourceComponent = Cast<UPCGGenSourceComponent>(InGenSource))
	{
		if (GenSourceComponent->GetWorld() != World)
		{
			return false;
		}
	}

	return GenSources.Add(InGenSource).IsValidId();
}

bool FPCGGenSourceManager::UnregisterGenSource(const IPCGGenSourceBase* InGenSource)
{
	if (const UPCGGenSourceComponent* GenSourceComponent = Cast<UPCGGenSourceComponent>(InGenSource))
	{
		if (GenSourceComponent->GetWorld() != World)
		{
			return false;
		}
	}

	return GenSources.Remove(InGenSource) > 0;
}

void FPCGGenSourceManager::OnGameModePostLogin(AGameModeBase* InGameMode, APlayerController* InPlayerController)
{
	if (InPlayerController == nullptr || InPlayerController->GetWorld() != World)
	{
		return;
	}
	ensure(IsInGameThread());

	if (InPlayerController->GetPawn())
	{
		UPCGGenSourcePlayer* GenSource = NewObject<UPCGGenSourcePlayer>();
		GenSource->SetPlayerController(InPlayerController);

		FSetElementId ElementId = GenSources.Add(GenSource);
		if (ElementId.IsValidId())
		{
			GenSource->AddToRoot();
		}
	}
}

void FPCGGenSourceManager::OnGameModePostLogout(AGameModeBase* InGameMode, AController* InController)
{
	if (InController->GetWorld() != World)
	{
		return;
	}
	ensure(IsInGameThread());

	for (IPCGGenSourceBase* GenSource : GenSources)
	{
		if (UPCGGenSourcePlayer* GenSourcePlayer = Cast<UPCGGenSourcePlayer>(GenSource))
		{
			if (GenSourcePlayer->GetPlayerController() == InController || !GenSourcePlayer->IsValid())
			{
				GenSourcePlayer->RemoveFromRoot();
				GenSources.Remove(GenSourcePlayer);
				break;
			}
		}
	}
}
