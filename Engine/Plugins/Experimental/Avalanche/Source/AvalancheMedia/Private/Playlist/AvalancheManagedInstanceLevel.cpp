// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playlist/AvalancheManagedInstanceLevel.h"

#include "AssetRegistry/AssetData.h"
#include "AvaMediaDefines.h"
#include "AvaRemoteControlRebind.h"
#include "AvaRemoteControlUtils.h"
#include "AvaScene.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "IAvaMediaModule.h"
#include "Playlist/AvalancheManagedInstanceUtils.h"
#include "UObject/Package.h"

namespace UE::AvalancheManagedInstanceLevel::Private
{
	UWorld* LoadAvalancheLevel(const FSoftObjectPath& InAssetPath)
	{
		return Cast<UWorld>(InAssetPath.TryLoad());
	}

	URemoteControlPreset* FindRemoteControlPreset(ULevel* InLevel)
	{
		const AAvaScene* AvaScene = AAvaScene::GetScene(InLevel, false);
		return AvaScene ? AvaScene->GetRemoteControlPreset() : nullptr;
	}
}

FAvalancheManagedInstanceLevel::FAvalancheManagedInstanceLevel(FAvalancheManagedInstanceCache* InParentCache, const FSoftObjectPath& InAssetPath)
	: FAvalancheManagedInstance(InParentCache, InAssetPath)
{
	using namespace UE::AvalancheManagedInstanceLevel::Private;
	// We need to load the source avalanche level.
	UWorld* SourceAvalancheLevel = LoadAvalancheLevel(InAssetPath);
	if (!SourceAvalancheLevel)
	{
		UE_LOG(LogAvaMedia, Error, TEXT("Unable to load Source Avalanche Level: %s"), *InAssetPath.ToString());
		return;
	}

	// Keep a weak pointer
	SourceAvalancheLevelWeak = SourceAvalancheLevel;

	// Register the delegates on the source RCP just in case the level is currently being edited.
	RegisterSourceRemoteControlPresetDelegates(FindRemoteControlPreset(SourceAvalancheLevel->PersistentLevel));

	ManagedAvalancheLevelPackage = FAvalancheManagedInstanceUtils::MakeManagedInstancePackage(InAssetPath);
	if (!ManagedAvalancheLevelPackage)
	{
		UE_LOG(LogAvaMedia, Error, TEXT("Unable to create a Managed Avalanche Level Package for %s"), *InAssetPath.ToString());
		return;
	}

	// Remark: using PIE duplication mode to avoid entity and controller Ids from being renewed.
	ManagedAvalancheLevel = Cast<UWorld>(StaticDuplicateObject(SourceAvalancheLevel, ManagedAvalancheLevelPackage.Get(), NAME_None, RF_NoFlags, nullptr, EDuplicateMode::PIE));
	if (!ManagedAvalancheLevel)
	{
		UE_LOG(LogAvaMedia, Error, TEXT("Unable to duplicate Source Avalanche Level: %s"), *InAssetPath.ToString());
		return;
	}

	ManagedAvalancheLevel->SetFlags(RF_Public | RF_Transient);
	
	ManagedRemoteControlPreset = FindRemoteControlPreset(ManagedAvalancheLevel->PersistentLevel);
	FAvaRemoteControlRebind::RebindUnboundEntities(ManagedRemoteControlPreset, ManagedAvalancheLevel->PersistentLevel);
	
	// Backup the remote control values from the source asset.
	constexpr bool bIsDefault = true;	// Flag the values as "default".
	DefaultRemoteControlValues.CopyFrom(ManagedRemoteControlPreset, bIsDefault);

	FAvalancheManagedInstanceUtils::PreventWorldFromBeingSeenAsLeakingByLevelEditor(ManagedAvalancheLevel.Get());

	IAvaMediaModule::Get().GetOnMapChangedEvent().AddRaw(this, &FAvalancheManagedInstanceLevel::OnMapChangedEvent);
}

FAvalancheManagedInstanceLevel::~FAvalancheManagedInstanceLevel()
{
	IAvaMediaModule::Get().GetOnMapChangedEvent().RemoveAll(this);

	if (ManagedRemoteControlPreset)
	{
		FAvaRemoteControlUtils::UnregisterRemoteControlPreset(ManagedRemoteControlPreset);
	}

	if (ManagedAvalancheLevelPackage)
	{
		ManagedAvalancheLevelPackage->ClearDirtyFlag();
	}
	
	ManagedAvalancheLevel = nullptr;
	ManagedAvalancheLevelPackage = nullptr;
	ManagedRemoteControlPreset = nullptr;
	DiscardSourceAvalancheLevel();
}

void FAvalancheManagedInstanceLevel::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( ManagedAvalancheLevel );
	Collector.AddReferencedObject( ManagedAvalancheLevelPackage );
	Collector.AddReferencedObject( ManagedRemoteControlPreset );
}

FString FAvalancheManagedInstanceLevel::GetReferencerName() const
{
	return TEXT("FAvalancheManagedInstanceLevel");
}

IAvaSceneInterface* FAvalancheManagedInstanceLevel::GetSceneInterface() const
{
	return ManagedAvalancheLevel ? static_cast<IAvaSceneInterface*>(AAvaScene::GetScene(ManagedAvalancheLevel->PersistentLevel, false)) : nullptr;
}

void FAvalancheManagedInstanceLevel::DiscardSourceAvalancheLevel()
{
	if (const UWorld* SourceAvalancheLevel = SourceAvalancheLevelWeak.Get())
	{
		using namespace UE::AvalancheManagedInstanceLevel::Private;
		UnregisterSourceRemoteControlPresetDelegates(FindRemoteControlPreset(SourceAvalancheLevel->PersistentLevel));
	}
	SourceAvalancheLevelWeak.Reset();
}

void FAvalancheManagedInstanceLevel::OnMapChangedEvent(UWorld* InWorld, EAvaMediaMapChangeType InEventType)
{
	if (!InWorld->GetPackage())
	{
		return;
	}

	if (SourceAssetPath.GetLongPackageFName() != InWorld->GetPackage()->GetFName())
	{
		return;
	}
	
	if (InEventType == EAvaMediaMapChangeType::LoadMap)
	{
		using namespace UE::AvalancheManagedInstanceLevel::Private;
		// This should be fast given the level has been loaded in the editor.
		if (UWorld* SourceAvalancheLevel = LoadAvalancheLevel(SourceAssetPath))
		{
			SourceAvalancheLevelWeak = SourceAvalancheLevel;
			RegisterSourceRemoteControlPresetDelegates(FindRemoteControlPreset(SourceAvalancheLevel->PersistentLevel));
		}
	}
	else if (InEventType == EAvaMediaMapChangeType::TearDownWorld)
	{
		DiscardSourceAvalancheLevel();
	}
}