// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rundown/AvaRundownManagedInstanceBlueprint.h"

#include "AssetRegistry/AssetData.h"
#include "AvaBlueprint.h"
#include "AvaRemoteControlRebind.h"
#include "AvaRemoteControlUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "IAvaMediaModule.h"
#include "RemoteControlPreset.h"
#include "Rundown/AvaRundownManagedInstanceUtils.h"
#include "UObject/Package.h"

namespace UE::AvaRundownManagedInstanceBlueprint::Private
{
	UWorld* CreateManagedInstanceWorld(UPackage* InOuter, EWorldType::Type InWorldType)
	{
		const FName WorldName = MakeUniqueObjectName(InOuter, UWorld::StaticClass(), TEXT("MotionDesignManagedInstanceWorld"));

		// Note: this will initialize the world.
		UWorld* World = UWorld::CreateWorld(InWorldType, false, WorldName, InOuter, false);

		if (InOuter == nullptr)
		{
			UPackage* const Package = World->GetPackage();
			Package->SetFlags(RF_Transient | RF_Public);
		}

		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(InWorldType);
		WorldContext.SetCurrentWorld(World);
		return World;
	}
	
	UAvalancheBlueprint* LoadBlueprint(const FSoftObjectPath& InAssetPath)
	{
		return Cast<UAvalancheBlueprint>(InAssetPath.TryLoad());
	}
}

FAvaRundownManagedInstanceBlueprint::FAvaRundownManagedInstanceBlueprint(FAvaRundownManagedInstanceCache* InParentCache, const FSoftObjectPath& InAssetPath)
	: FAvaRundownManagedInstance(InParentCache, InAssetPath)
{
	using namespace UE::AvaRundownManagedInstanceBlueprint::Private;
	SourceBlueprint = LoadBlueprint(InAssetPath);
	if (!SourceBlueprint)
	{
		UE_LOG(LogAvaMedia, Error, TEXT("Unable to load Source Motion Design Blueprint: %s"), *InAssetPath.ToString());
		return;
	}
	
	RegisterSourceRemoteControlPresetDelegates(SourceBlueprint->GetRemoteControlPreset());

#if WITH_EDITORONLY_DATA
	FGuardValue_Bitfield(SourceBlueprint->bDuplicatingReadOnly, true);
#endif

	ManagedBlueprintPackage = FAvaRundownManagedInstanceUtils::MakeManagedInstancePackage(InAssetPath);
	if (!ManagedBlueprintPackage)
	{
		UE_LOG(LogAvaMedia, Error, TEXT("Unable to create a Managed Motion Design Blueprint Package for %s"), *InAssetPath.ToString());
		return;
	}

	// Remark: using PIE duplication mode to avoid entity and controller Ids from being renewed.
	ManagedBlueprint = Cast<UAvalancheBlueprint>(StaticDuplicateObject(SourceBlueprint.Get(), ManagedBlueprintPackage.Get(), NAME_None, RF_NoFlags, nullptr, EDuplicateMode::PIE));
	if (!ManagedBlueprint)
	{
		UE_LOG(LogAvaMedia, Error, TEXT("Unable to duplicate Source Motion Design Blueprint: %s"), *InAssetPath.ToString());
		return;
	}

	ManagedBlueprint->SetFlags(RF_Public | RF_Transient);

	ManagedWorld = CreateManagedInstanceWorld(ManagedBlueprintPackage.Get(), EWorldType::Type::Game);
	ManagedBlueprint->SetAvalancheWorld(ManagedWorld.Get());
	ManagedBlueprint->LoadAvalancheWorld();
	FAvaRemoteControlRebind::RebindUnboundEntities(ManagedBlueprint->GetRemoteControlPreset());

	// Backup the remote control values from the source.
	constexpr bool bIsDefault = true;	// Flag the values as "default".
	DefaultRemoteControlValues.CopyFrom(ManagedBlueprint->GetRemoteControlPreset(), bIsDefault);

	FAvaRundownManagedInstanceUtils::PreventWorldFromBeingSeenAsLeakingByLevelEditor(ManagedWorld.Get());
}

FAvaRundownManagedInstanceBlueprint::~FAvaRundownManagedInstanceBlueprint()
{
	if (ManagedBlueprintPackage)
	{
		ManagedBlueprintPackage->ClearDirtyFlag();
	}

	// Simplified version of UAvaGameInstance::EndPlay()
	if (ManagedWorld)
	{
		GEngine->DestroyWorldContext(ManagedWorld.Get());
		ManagedWorld->DestroyWorld(true);
	}

	ManagedWorld = nullptr;
	if (ManagedBlueprint && ManagedBlueprint->GetRemoteControlPreset())
	{
		FAvaRemoteControlUtils::UnregisterRemoteControlPreset(ManagedBlueprint->GetRemoteControlPreset());
	}
	ManagedBlueprint = nullptr;

	if (SourceBlueprint)
	{
		UnregisterSourceRemoteControlPresetDelegates(SourceBlueprint->GetRemoteControlPreset());
	}
	SourceBlueprint = nullptr;
}

void FAvaRundownManagedInstanceBlueprint::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( SourceBlueprint );
	Collector.AddReferencedObject( ManagedWorld );
	Collector.AddReferencedObject( ManagedBlueprint );
	Collector.AddReferencedObject( ManagedBlueprintPackage );
}

FString FAvaRundownManagedInstanceBlueprint::GetReferencerName() const
{
	return TEXT("FAvaRundownManagedInstanceBlueprint");
}

IAvaSceneInterface* FAvaRundownManagedInstanceBlueprint::GetSceneInterface() const
{
	return ManagedBlueprint ? static_cast<IAvaSceneInterface*>(ManagedBlueprint.Get()) : nullptr;
}

bool FAvaRundownManagedInstanceBlueprint::IsValid() const
{
	return ::IsValid(ManagedBlueprint) && ::IsValid(ManagedWorld);
}

URemoteControlPreset* FAvaRundownManagedInstanceBlueprint::GetRemoteControlPreset() const
{
	return IsValid() ? ManagedBlueprint->GetRemoteControlPreset() : nullptr;
}