// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterBodyManager.h"
#include "WaterBodyComponent.h"
#include "WaterSubsystem.h"
#include "WaterViewExtension.h"

void FWaterBodyManager::Initialize(UWorld* World)
{
	if (World != nullptr)
	{
		WaterViewExtension = FSceneViewExtensions::NewExtension<FWaterViewExtension>(World);
		WaterViewExtension->Initialize();
	}
}

void FWaterBodyManager::Deinitialize()
{
	WaterViewExtension->Deinitialize();
	WaterViewExtension.Reset();
}

int32 FWaterBodyManager::AddWaterBodyComponent(UWaterBodyComponent* InWaterBodyComponent)
{
	RequestWaveDataRebuild();
	const int32 WaterIndex = WaterBodyComponents.Register(InWaterBodyComponent);
	OnWaterBodyAdded.Broadcast(InWaterBodyComponent);
	return WaterIndex;
}

void FWaterBodyManager::RemoveWaterBodyComponent(UWaterBodyComponent* InWaterBodyComponent)
{
	RequestWaveDataRebuild();
	WaterBodyComponents.Unregister(InWaterBodyComponent, InWaterBodyComponent->GetWaterBodyIndex());
	OnWaterBodyRemoved.Broadcast(InWaterBodyComponent);
}

int32 FWaterBodyManager::AddWaterZone(AWaterZone* InWaterZone)
{
	RequestGPUDataRebuild();
	return WaterZones.Register(InWaterZone);
}

void FWaterBodyManager::RemoveWaterZone(AWaterZone* InWaterZone)
{
	RequestGPUDataRebuild();
	WaterZones.Unregister(InWaterZone, InWaterZone->GetWaterZoneIndex());
}

void FWaterBodyManager::RequestGPUDataRebuild()
{
	if (WaterViewExtension)
	{
		WaterViewExtension->MarkGPUDataDirty();
	}
}

void FWaterBodyManager::RequestWaveDataRebuild()
{
	RequestGPUDataRebuild();

	// Recompute the maximum of all MaxWaveHeight : 
	GlobalMaxWaveHeight = 0.0f;
	ForEachWaterBodyComponent([this](UWaterBodyComponent* WaterBodyComponent)
	{
		GlobalMaxWaveHeight = FMath::Max(GlobalMaxWaveHeight, WaterBodyComponent->GetMaxWaveHeight());
		return true;
	});
}

void FWaterBodyManager::ForEachWaterBodyComponent(TFunctionRef<bool(UWaterBodyComponent*)> Pred) const
{
	for (UWaterBodyComponent* WaterBodyComponent : WaterBodyComponents.Elements)
	{
		if (WaterBodyComponent)
		{
			if (!Pred(WaterBodyComponent))
			{
				return;
			}
		}
	}
}

void FWaterBodyManager::ForEachWaterBodyComponent(const UWorld* World, TFunctionRef<bool(UWaterBodyComponent*)> Pred)
{
	if (FWaterBodyManager* Manager = UWaterSubsystem::GetWaterBodyManager(World))
	{
		Manager->ForEachWaterBodyComponent(Pred);
	}
}

void FWaterBodyManager::ForEachWaterZone(TFunctionRef<bool(AWaterZone*)> Pred) const
{
	for (AWaterZone* WaterZone : WaterZones.Elements)
	{
		if (WaterZone)
		{
			if (!Pred(WaterZone))
			{
				return;
			}
		}
	}
}

void FWaterBodyManager::ForEachWaterZone(const UWorld* World, TFunctionRef<bool(AWaterZone*)> Pred)
{
	if (FWaterBodyManager* Manager = UWaterSubsystem::GetWaterBodyManager(World))
	{
		Manager->ForEachWaterZone(Pred);
	}
}

