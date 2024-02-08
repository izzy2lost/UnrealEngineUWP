// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/CEEffectorSubsystem.h"

#include "Effector/CEEffectorActor.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataChannelPublic.h"

DEFINE_LOG_CATEGORY_STATIC(LogACEEffectorSubsystem, Log, All);

UCEEffectorSubsystem::FOnSubsystemInitialized UCEEffectorSubsystem::OnSubsystemInitializedDelegate;
UCEEffectorSubsystem::FOnEffectorIdentifierChanged UCEEffectorSubsystem::OnEffectorIdentifierChangedDelegate;

UCEEffectorSubsystem* UCEEffectorSubsystem::Get(const UWorld* InWorld)
{
	if (InWorld)
	{
		return InWorld->GetSubsystem<UCEEffectorSubsystem>();
	}

	return nullptr;
}

void UCEEffectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Load niagara data channel asset for effectors and cache it
	EffectorDataChannelAsset = LoadObject<UNiagaraDataChannelAsset>(nullptr, DataChannelAssetPath);

	check(EffectorDataChannelAsset->Get());
}

void UCEEffectorSubsystem::PostInitialize()
{
	Super::PostInitialize();

	if (const UWorld* World = GetWorld())
	{
		OnSubsystemInitializedDelegate.Broadcast(World);
	}
}

bool UCEEffectorSubsystem::RegisterChannelEffector(ACEEffectorActor* InEffector)
{
	if (!IsValid(InEffector))
	{
		return false;
	}

	int32 EffectorIndex = INDEX_NONE;

	if (!EffectorsWeak.Contains(InEffector))
	{
		EffectorIndex = EffectorsWeak.Add(InEffector);
		UE_LOG(LogACEEffectorSubsystem, Log, TEXT("%s effector registered in channel %i"), *InEffector->GetActorNameOrLabel(), EffectorIndex);
	}

	if (InEffector->ChannelData.Identifier != EffectorIndex)
	{
		InEffector->ChannelData.Identifier = EffectorIndex;
		OnEffectorIdentifierChangedDelegate.Broadcast(InEffector);
	}

	return true;
}

bool UCEEffectorSubsystem::UnregisterChannelEffector(ACEEffectorActor* InEffector)
{
	if (!InEffector)
	{
		return false;
	}

	const bool bUnregistered = EffectorsWeak.Remove(InEffector) > 0;

	if (bUnregistered)
	{
		InEffector->ChannelData.Identifier = INDEX_NONE;
		OnEffectorIdentifierChangedDelegate.Broadcast(InEffector);

		UE_LOG(LogACEEffectorSubsystem, Log, TEXT("%s effector unregistered from channel"), *InEffector->GetActorNameOrLabel());
	}

	return bUnregistered;
}

void UCEEffectorSubsystem::UpdateEffectorChannel()
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
	UNiagaraDataChannelWriter* ChannelWriter = UNiagaraDataChannelLibrary::WriteToNiagaraDataChannel(World, EffectorDataChannelAsset.Get(), SearchParameters, EffectorsWeak.Num(), true, true, true, UCEEffectorSubsystem::StaticClass()->GetName());

	if (!ChannelWriter)
	{
		UE_LOG(LogACEEffectorSubsystem, Warning, TEXT("Effector data channel writer is invalid"));
		return;
	}

	// Remove invalid effectors and push updates to effector assigned channel indexes
	int32 EffectorIndex = 0;
	for (TArray<TWeakObjectPtr<ACEEffectorActor>>::TIterator It(EffectorsWeak); It; ++It)
	{
		ACEEffectorActor* Effector = It->Get();

		if (!IsValid(Effector))
		{
			It.RemoveCurrent();
			continue;
		}

		FCEClonerEffectorChannelData& ChannelData = Effector->GetChannelData();

		const bool bIdentifierChanged = ChannelData.Identifier != EffectorIndex;

		// Set channel before writing
		ChannelData.Identifier = EffectorIndex++;

		// Push effector data to channel
		ChannelData.Write(ChannelWriter);

		// When changed, update cloners DI linked to this effector
		if (bIdentifierChanged)
		{
			OnEffectorIdentifierChangedDelegate.Broadcast(Effector);
		}
	}
}

bool UCEEffectorSubsystem::IsTickableInEditor() const
{
	return true;
}

TStatId UCEEffectorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCEEffectorSubsystem, STATGROUP_Tickables);
}

void UCEEffectorSubsystem::Tick(float InDeltaTime)
{
	UpdateEffectorChannel();
}
