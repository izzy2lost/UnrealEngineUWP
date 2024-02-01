// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playlist/AvalancheManagedInstanceBlueprint.h"

#include "AssetRegistry/AssetData.h"
#include "AvaBlueprint.h"
#include "AvaRemoteControlRebind.h"
#include "AvaRemoteControlUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "IAvaMediaModule.h"
#include "Playlist/AvalancheManagedInstanceUtils.h"
#include "RemoteControlPreset.h"
#include "UObject/Package.h"

namespace UE::AvalancheManagedInstanceBlueprint::Private
{
	UWorld* CreateManagedInstanceWorld(UPackage* InOuter, EWorldType::Type InWorldType)
	{
		const FName AvalancheWorldName = MakeUniqueObjectName(InOuter, UWorld::StaticClass(), TEXT("AvalancheManagedInstanceWorld"));
		

		// Note: this will initialize the world.
		UWorld* World = UWorld::CreateWorld(InWorldType, false, AvalancheWorldName, InOuter, false);

		if (InOuter == nullptr)
		{
			UPackage* const Package = World->GetPackage();
			Package->SetFlags(RF_Transient | RF_Public);
		}

		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(InWorldType);
		WorldContext.SetCurrentWorld(World);
		return World;
	}
	
	UAvalancheBlueprint* LoadAvalancheBlueprint(const FSoftObjectPath& InAssetPath)
	{
		return Cast<UAvalancheBlueprint>(InAssetPath.TryLoad());
	}
}

FAvalancheManagedInstanceBlueprint::FAvalancheManagedInstanceBlueprint(FAvalancheManagedInstanceCache* InParentCache, const FSoftObjectPath& InAssetPath)
	: FAvalancheManagedInstance(InParentCache, InAssetPath)
{
	using namespace UE::AvalancheManagedInstanceBlueprint::Private;
	SourceAvalancheBlueprint = LoadAvalancheBlueprint(InAssetPath);
	if (!SourceAvalancheBlueprint)
	{
		UE_LOG(LogAvaMedia, Error, TEXT("Unable to load Source Motion Design Blueprint: %s"), *InAssetPath.ToString());
		return;
	}
	
	RegisterSourceRemoteControlPresetDelegates(SourceAvalancheBlueprint->GetRemoteControlPreset());

#if WITH_EDITORONLY_DATA
	FGuardValue_Bitfield(SourceAvalancheBlueprint->bDuplicatingReadOnly, true);
#endif

	ManagedAvalancheBlueprintPackage = FAvalancheManagedInstanceUtils::MakeManagedInstancePackage(InAssetPath);
	if (!ManagedAvalancheBlueprintPackage)
	{
		UE_LOG(LogAvaMedia, Error, TEXT("Unable to create a Managed Motion Design Blueprint Package for %s"), *InAssetPath.ToString());
		return;
	}

	// Remark: using PIE duplication mode to avoid entity and controller Ids from being renewed.
	ManagedAvalancheBlueprint = Cast<UAvalancheBlueprint>(StaticDuplicateObject(SourceAvalancheBlueprint.Get(), ManagedAvalancheBlueprintPackage.Get(), NAME_None, RF_NoFlags, nullptr, EDuplicateMode::PIE));
	if (!ManagedAvalancheBlueprint)
	{
		UE_LOG(LogAvaMedia, Error, TEXT("Unable to duplicate Source Motion Design Blueprint: %s"), *InAssetPath.ToString());
		return;
	}

	ManagedAvalancheBlueprint->SetFlags(RF_Public | RF_Transient);

	ManagedWorld = CreateManagedInstanceWorld(ManagedAvalancheBlueprintPackage.Get(), EWorldType::Type::Game);
	ManagedAvalancheBlueprint->SetAvalancheWorld(ManagedWorld.Get());
	ManagedAvalancheBlueprint->LoadAvalancheWorld();
	FAvaRemoteControlRebind::RebindUnboundEntities(ManagedAvalancheBlueprint->GetRemoteControlPreset());

	// Backup the remote control values from the source.
	constexpr bool bIsDefault = true;	// Flag the values as "default".
	DefaultRemoteControlValues.CopyFrom(ManagedAvalancheBlueprint->GetRemoteControlPreset(), bIsDefault);

	FAvalancheManagedInstanceUtils::PreventWorldFromBeingSeenAsLeakingByLevelEditor(ManagedWorld.Get());
}

FAvalancheManagedInstanceBlueprint::~FAvalancheManagedInstanceBlueprint()
{
	if (ManagedAvalancheBlueprintPackage)
	{
		ManagedAvalancheBlueprintPackage->ClearDirtyFlag();
	}

	// Simplified version of UAvaGameInstance::EndPlay()
	if (ManagedWorld)
	{
		GEngine->DestroyWorldContext(ManagedWorld.Get());
		ManagedWorld->DestroyWorld(true);
	}

	ManagedWorld = nullptr;
	if (ManagedAvalancheBlueprint && ManagedAvalancheBlueprint->GetRemoteControlPreset())
	{
		FAvaRemoteControlUtils::UnregisterRemoteControlPreset(ManagedAvalancheBlueprint->GetRemoteControlPreset());
	}
	ManagedAvalancheBlueprint = nullptr;

	if (SourceAvalancheBlueprint)
	{
		UnregisterSourceRemoteControlPresetDelegates(SourceAvalancheBlueprint->GetRemoteControlPreset());
	}
	SourceAvalancheBlueprint = nullptr;
}

void FAvalancheManagedInstanceBlueprint::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( SourceAvalancheBlueprint );
	Collector.AddReferencedObject( ManagedWorld );
	Collector.AddReferencedObject( ManagedAvalancheBlueprint );
	Collector.AddReferencedObject( ManagedAvalancheBlueprintPackage );
}

FString FAvalancheManagedInstanceBlueprint::GetReferencerName() const
{
	return TEXT("FAvalancheManagedInstanceBlueprint");
}

IAvaSceneInterface* FAvalancheManagedInstanceBlueprint::GetSceneInterface() const
{
	return ManagedAvalancheBlueprint ? static_cast<IAvaSceneInterface*>(ManagedAvalancheBlueprint.Get()) : nullptr;
}

bool FAvalancheManagedInstanceBlueprint::IsValid() const
{
	return ::IsValid(ManagedAvalancheBlueprint) && ::IsValid(ManagedWorld);
}

URemoteControlPreset* FAvalancheManagedInstanceBlueprint::GetRemoteControlPreset() const
{
	return IsValid() ? ManagedAvalancheBlueprint->GetRemoteControlPreset() : nullptr;
}