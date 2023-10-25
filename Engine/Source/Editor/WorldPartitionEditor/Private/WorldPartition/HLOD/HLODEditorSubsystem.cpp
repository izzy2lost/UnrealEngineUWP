// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODEditorSubsystem.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Subsystems/UnrealEditorSubsystem.h"
#include "WorldPartition/HLOD/HLODEditorData.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionSubsystem.h"


static TAutoConsoleVariable<bool> CVarHLODInEditorEnabled(
	TEXT("wp.Editor.HLOD"),
	false,
	TEXT("Show World Partition HLODs in the editor."));


UWorldPartitionHLODEditorSubsystem::UWorldPartitionHLODEditorSubsystem()
{
}

UWorldPartitionHLODEditorSubsystem::~UWorldPartitionHLODEditorSubsystem()
{
}

bool UWorldPartitionHLODEditorSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Editor && !IsRunningCommandlet();
}

void UWorldPartitionHLODEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// Ensure the WorldPartitionSubsystem gets created before the HLODEditorSubsystem
	Collection.InitializeDependency<UWorldPartitionSubsystem>();

	Super::Initialize(Collection);

	bForceHLODStateUpdate = true;
	bHLODInEditorEnabled = CVarHLODInEditorEnabled.GetValueOnGameThread();
	
	GetWorld()->OnWorldPartitionInitialized().AddUObject(this, &UWorldPartitionHLODEditorSubsystem::OnWorldPartitionInitialized);
	GetWorld()->OnWorldPartitionUninitialized().AddUObject(this, &UWorldPartitionHLODEditorSubsystem::OnWorldPartitionUninitialized);

	GEngine->OnLevelActorListChanged().AddUObject(this, &UWorldPartitionHLODEditorSubsystem::ForceHLODStateUpdate);
}

void UWorldPartitionHLODEditorSubsystem::Deinitialize()
{
	Super::Deinitialize();

	GEngine->OnLevelActorListChanged().RemoveAll(this);

	GetWorld()->OnWorldPartitionInitialized().RemoveAll(this);
	GetWorld()->OnWorldPartitionUninitialized().RemoveAll(this);
}

void UWorldPartitionHLODEditorSubsystem::OnWorldPartitionInitialized(UWorldPartition* InWorldPartition)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionHLODEditorSubsystem::OnWorldPartitionInitialized);

	UActorDescContainer* ActorDescContainer = InWorldPartition->GetActorDescContainer();
	if (!ActorDescContainer || ActorDescContainer->IsTemplateContainer())
	{
		return;
	}

	InWorldPartition->LoaderAdapterStateChanged.AddUObject(this, &UWorldPartitionHLODEditorSubsystem::OnLoaderAdapterStateChanged);

	FWorldPartitionHLODEditorData* HLODEditorData = WorldPartitionsHLODEditorData.Emplace(InWorldPartition, new FWorldPartitionHLODEditorData(InWorldPartition));
	if (CVarHLODInEditorEnabled.GetValueOnGameThread())
	{
		HLODEditorData->LoadHLODActors();
		ForceHLODStateUpdate();
	}
}

void UWorldPartitionHLODEditorSubsystem::OnWorldPartitionUninitialized(UWorldPartition* InWorldPartition)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionHLODEditorSubsystem::OnWorldPartitionUninitialized);

	UActorDescContainer* ActorDescContainer = InWorldPartition->GetActorDescContainer();
	if (!ActorDescContainer || ActorDescContainer->IsTemplateContainer())
	{
		return;
	}

	InWorldPartition->LoaderAdapterStateChanged.RemoveAll(this);

	FWorldPartitionHLODEditorData* HLODEditorData = WorldPartitionsHLODEditorData.FindAndRemoveChecked(InWorldPartition);
	delete HLODEditorData;
}

void UWorldPartitionHLODEditorSubsystem::OnLoaderAdapterStateChanged(const IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionHLODEditorSubsystem::OnLoaderAdapterStateChanged);

	ForceHLODStateUpdate();
}

void UWorldPartitionHLODEditorSubsystem::ForceHLODStateUpdate()
{
	bForceHLODStateUpdate = true;
}

void UWorldPartitionHLODEditorSubsystem::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionHLODEditorSubsystem::Tick);

	if (CVarHLODInEditorEnabled.GetValueOnGameThread() != bHLODInEditorEnabled)
	{
		bHLODInEditorEnabled = CVarHLODInEditorEnabled.GetValueOnGameThread();
		for (auto& [WorldPartition, HLODEditorData] : WorldPartitionsHLODEditorData)
		{
			if (bHLODInEditorEnabled)
			{
				HLODEditorData->LoadHLODActors();
			}
			else
			{
				HLODEditorData->UnloadHLODActors();
			}
		}
	}

	if (bForceHLODStateUpdate)
	{
		for (auto& [WorldPartition, HLODEditorData] : WorldPartitionsHLODEditorData)
		{
			HLODEditorData->UpdateLoadedActorsState();
		}
	}

	UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();
	if (UnrealEditorSubsystem)
	{
		FVector CameraLocation;
		FRotator CameraRotation;
		UnrealEditorSubsystem->GetLevelViewportCameraInfo(CameraLocation, CameraRotation);

		if (bForceHLODStateUpdate || CameraLocation != CachedCameraLocation)
		{
			CachedCameraLocation = CameraLocation;			

			for (auto& [WorldPartition, HLODEditorData] : WorldPartitionsHLODEditorData)
			{
				HLODEditorData->UpdateVisibility(CameraLocation, bForceHLODStateUpdate);
			}
		}
	}

	bForceHLODStateUpdate = false;
}

TStatId UWorldPartitionHLODEditorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(WorldPartitionHLODEditorSubsystem, STATGROUP_Tickables);
}
