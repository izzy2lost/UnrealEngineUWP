// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/AvaEffectorSubsystem.h"

#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataChannelPublic.h"
#include "Effector/AvaEffectorActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogAvaEffectorSubsystem, Log, All);

UAvaEffectorSubsystem::FOnSubsystemInitialized UAvaEffectorSubsystem::OnSubsystemInitializedDelegate;

UAvaEffectorSubsystem* UAvaEffectorSubsystem::Get(const UWorld* InWorld)
{
	if (InWorld)
	{
		return InWorld->GetSubsystem<UAvaEffectorSubsystem>();
	}

	return nullptr;
}

void UAvaEffectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Load niagara data channel asset for effectors and cache it
	static const FString DataChannelAssetPath = TEXT("/Script/Niagara.NiagaraDataChannelAsset'/Avalanche/ClonerResources/Channels/NDC_Effector.NDC_Effector'");

	EffectorDataChannelAsset = FindObject<UNiagaraDataChannelAsset>(nullptr, *DataChannelAssetPath);

	if (!EffectorDataChannelAsset)
	{
		EffectorDataChannelAsset = LoadObject<UNiagaraDataChannelAsset>(nullptr, *DataChannelAssetPath);
	}

	check(EffectorDataChannelAsset->Get());
}

void UAvaEffectorSubsystem::PostInitialize()
{
	Super::PostInitialize();

	if (const UWorld* World = GetWorld())
	{
		OnSubsystemInitializedDelegate.Broadcast(World);
	}
}

bool UAvaEffectorSubsystem::RegisterChannelEffector(AAvaEffectorActor* InEffector)
{
	if (!InEffector)
	{
		return false;
	}

	const int32 EffectorIndex = EffectorsWeak.AddUnique(InEffector);
	const bool bRegistered = EffectorIndex == EffectorsWeak.Num() - 1;

	if (bRegistered)
	{
		UE_LOG(LogAvaEffectorSubsystem, Log, TEXT("%s effector registered in channel %i"), *InEffector->GetActorNameOrLabel(), EffectorIndex);
	}

	return bRegistered;
}

bool UAvaEffectorSubsystem::UnregisterChannelEffector(AAvaEffectorActor* InEffector)
{
	if (!InEffector)
	{
		return false;
	}

	const bool bUnregistered = EffectorsWeak.Remove(InEffector) > 0;

	if (bUnregistered)
	{
		UE_LOG(LogAvaEffectorSubsystem, Log, TEXT("%s effector unregistered from channel"), *InEffector->GetActorNameOrLabel());
	}

	return bUnregistered;
}

void UAvaEffectorSubsystem::UpdateEffectorChannel()
{
	const UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return;
	}

	if (EffectorsWeak.IsEmpty())
	{
		return;
	}

	// Reserve space in channel for each effectors
	static const FNiagaraDataChannelSearchParameters SearchParameters;
	UNiagaraDataChannelWriter* ChannelWriter = UNiagaraDataChannelLibrary::WriteToNiagaraDataChannel(World, EffectorDataChannelAsset.Get(), SearchParameters, EffectorsWeak.Num(), true, true, true, TEXT("AvaEffectorSubsystem"));

	if (!ChannelWriter)
	{
		UE_LOG(LogAvaEffectorSubsystem, Warning, TEXT("Effector data channel writer is invalid"));
		return;
	}

	// Remove invalid effectors and push updates to effector assigned channel indexes
	int32 EffectorIndex = 0;
	for (TArray<TWeakObjectPtr<AAvaEffectorActor>>::TIterator It(EffectorsWeak); It; ++It)
	{
		AAvaEffectorActor* Effector = It->Get();

		if (!Effector)
		{
			It.RemoveCurrent();
			continue;
		}

		FAvaClonerEffectorChannelData& ChannelData = Effector->GetEffectorChannelData();

		const bool bIdentifierChanged = ChannelData.Identifier != EffectorIndex;

		// Set channel before writing 
		ChannelData.Identifier = EffectorIndex++;

		// Push effector data to channel
		ChannelData.Write(ChannelWriter);

		// When changed, update cloners DI linked to this effector
		if (bIdentifierChanged)
		{
			Effector->OnEffectorIdentifierChanged();
		}
	}
}

bool UAvaEffectorSubsystem::IsTickableInEditor() const
{
	return true;
}

TStatId UAvaEffectorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAvaEffectorSubsystem, STATGROUP_Tickables);
}

void UAvaEffectorSubsystem::Tick(float InDeltaTime)
{
	UpdateEffectorChannel();
}
