// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODEditorSubsystem.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/ActorPrimitiveColorHandler.h"
#include "GameFramework/WorldSettings.h"
#include "Subsystems/UnrealEditorSubsystem.h"
#include "WorldPartitionEditorModule.h"
#include "WorldPartition/HLOD/HLODEditorData.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionEditorSettings.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

#include "PropertyPermissionList.h"

static TAutoConsoleVariable<bool> CVarHLODInEditorEnabled(
	TEXT("wp.Editor.HLOD.AllowShowingHLODsInEditor"),
	true,
	TEXT("Allow showing World Partition HLODs in the editor."));

#define LOCTEXT_NAMESPACE "HLODEditorSubsystem"

static FName NAME_HLODRelevantColorHandler(TEXT("HLODRelevantColorHandler"));
TMap<EHLODSettingsVisibility, UWorldPartitionHLODEditorSubsystem::FStructsPropertiesMap> UWorldPartitionHLODEditorSubsystem::StructsPropertiesVisibility;

UWorldPartitionHLODEditorSubsystem::UWorldPartitionHLODEditorSubsystem()
{
#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER
	if (HasAnyFlags(RF_ClassDefaultObject) && ExactCast<UWorldPartitionHLODEditorSubsystem>(this))
	{
		FActorPrimitiveColorHandler::Get().RegisterPrimitiveColorHandler(NAME_HLODRelevantColorHandler, LOCTEXT("HLODRelevantColor", "HLOD Relevant Color"), [](const UPrimitiveComponent* InPrimitiveComponent) -> FLinearColor
		{
			if (AActor* Actor = InPrimitiveComponent->GetOwner())
			{
				if (InPrimitiveComponent->IsHLODRelevant() && Actor->IsHLODRelevant())
				{
					return FLinearColor::Green;
				}
			}
			return FLinearColor::Red;
		});
	}
#endif

	if (IsTemplate())
	{
		HLOD_ADD_CLASS_SETTING_FILTER_NAME(BasicSettings, UHLODLayer, UHLODLayer::GetHLODBuilderSettingsPropertyName());
	}
}

UWorldPartitionHLODEditorSubsystem::~UWorldPartitionHLODEditorSubsystem()
{
#if ENABLE_ACTOR_PRIMITIVE_COLOR_HANDLER
	if (HasAnyFlags(RF_ClassDefaultObject) && ExactCast<UWorldPartitionHLODEditorSubsystem>(this))
	{
		FActorPrimitiveColorHandler::Get().UnregisterPrimitiveColorHandler(NAME_HLODRelevantColorHandler);
	}
#endif
}

bool UWorldPartitionHLODEditorSubsystem::IsHLODInEditorEnabled()
{
	const IWorldPartitionEditorModule* WorldPartitionEditorModule = FModuleManager::GetModulePtr<IWorldPartitionEditorModule>("WorldPartitionEditor");
	const bool bShowHLODsInEditorForWorld = WorldPartitionEditorModule && WorldPartitionEditorModule->IsHLODInEditorAllowed(GetWorld());
	const bool bShowHLODsInEditorUserSetting = WorldPartitionEditorModule && WorldPartitionEditorModule->GetShowHLODsInEditor();
	const bool bWorldPartitionLoadingInEditorEnabled = WorldPartitionEditorModule && WorldPartitionEditorModule->GetEnableLoadingInEditor();
	return CVarHLODInEditorEnabled.GetValueOnGameThread() && bShowHLODsInEditorUserSetting && bShowHLODsInEditorForWorld && bWorldPartitionLoadingInEditorEnabled;
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
	CachedCameraLocation = FVector::Zero();
	CachedHLODMinDrawDistance = 0;
	CachedHLODMaxDrawDistance = 0;
	bCachedShowHLODsOverLoadedRegions = false;
	
	GetWorld()->OnWorldPartitionInitialized().AddUObject(this, &UWorldPartitionHLODEditorSubsystem::OnWorldPartitionInitialized);
	GetWorld()->OnWorldPartitionUninitialized().AddUObject(this, &UWorldPartitionHLODEditorSubsystem::OnWorldPartitionUninitialized);

	GEngine->OnLevelActorListChanged().AddUObject(this, &UWorldPartitionHLODEditorSubsystem::ForceHLODStateUpdate);

	UWorldPartitionEditorSettings::OnSettingsChanged().AddUObject(this, &UWorldPartitionHLODEditorSubsystem::OnWorldPartitionEditorSettingsChanged);

	ApplyHLODSettingsFiltering();
}

void UWorldPartitionHLODEditorSubsystem::Deinitialize()
{
	Super::Deinitialize();

	UWorldPartitionEditorSettings::OnSettingsChanged().RemoveAll(this);

	GEngine->OnLevelActorListChanged().RemoveAll(this);

	GetWorld()->OnWorldPartitionInitialized().RemoveAll(this);
	GetWorld()->OnWorldPartitionUninitialized().RemoveAll(this);
}

void UWorldPartitionHLODEditorSubsystem::OnWorldPartitionEditorSettingsChanged(const FName& InPropertyName, const UWorldPartitionEditorSettings& InWorldPartitionEditorSettings)
{
	if (InPropertyName == UWorldPartitionEditorSettings::GetEnableAdvancedHLODSettingsPropertyName())
	{
		ApplyHLODSettingsFiltering();
	}
}

void UWorldPartitionHLODEditorSubsystem::ApplyHLODSettingsFiltering()
{
	static const FName PropertyPermissionListOwnerName = "AdvancedHLODSettingsFiltering";

	FPropertyEditorPermissionList::Get().UnregisterOwner(PropertyPermissionListOwnerName);

	UEnum* HLODLayerTypeEnum = StaticEnum<EHLODLayerType>();
	if (!GetDefault<UWorldPartitionEditorSettings>()->GetEnableAdvancedHLODSettings())
	{
		for (const TPair<TSoftObjectPtr<UStruct>, TSet<FName>>& StructProperties : StructsPropertiesVisibility.FindOrAdd(EHLODSettingsVisibility::BasicSettings))
		{
			FNamePermissionList PermissionList;

			for (FName PropertyName : StructProperties.Value)
			{
				PermissionList.AddAllowListItem(PropertyPermissionListOwnerName, PropertyName);
			}

			FPropertyEditorPermissionList::Get().AddPermissionList(StructProperties.Key, PermissionList, EPropertyPermissionListRules::UseExistingPermissionList);
		}
	}
}

void UWorldPartitionHLODEditorSubsystem::OnWorldPartitionInitialized(UWorldPartition* InWorldPartition)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionHLODEditorSubsystem::OnWorldPartitionInitialized);
	
	if (InWorldPartition->IsMainWorldPartition())
	{
		InWorldPartition->LoaderAdapterStateChanged.AddUObject(this, &UWorldPartitionHLODEditorSubsystem::OnLoaderAdapterStateChanged);
		HLODEditorData = MakePimpl<FWorldPartitionHLODEditorData>(InWorldPartition);
		ForceHLODStateUpdate();
	}
}

void UWorldPartitionHLODEditorSubsystem::OnWorldPartitionUninitialized(UWorldPartition* InWorldPartition)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionHLODEditorSubsystem::OnWorldPartitionUninitialized);

	if (InWorldPartition->IsMainWorldPartition())
	{
		InWorldPartition->LoaderAdapterStateChanged.RemoveAll(this);
		HLODEditorData = nullptr;
	}
}

void UWorldPartitionHLODEditorSubsystem::OnLoaderAdapterStateChanged(const IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionHLODEditorSubsystem::OnLoaderAdapterStateChanged);

	ForceHLODStateUpdate();
}

void UWorldPartitionHLODEditorSubsystem::ForceHLODStateUpdate()
{
	if (IsHLODInEditorEnabled())
	{
		bForceHLODStateUpdate = true;
	}
}

void UWorldPartitionHLODEditorSubsystem::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionHLODEditorSubsystem::Tick);

	if (HLODEditorData)
	{
		HLODEditorData->SetHLODLoadingState(IsHLODInEditorEnabled());
		
		if (IsHLODInEditorEnabled())
		{
			bool bForceHLODVisibilityUpdate = false;

			IWorldPartitionEditorModule* WorldPartitionEditorModule = FModuleManager::GetModulePtr<IWorldPartitionEditorModule>("WorldPartitionEditor");

			// "Show HLODs over loaded region" option changed ?
			if (WorldPartitionEditorModule->GetShowHLODsOverLoadedRegions() != bCachedShowHLODsOverLoadedRegions)
			{ 
				bCachedShowHLODsOverLoadedRegions = WorldPartitionEditorModule->GetShowHLODsOverLoadedRegions();
				bForceHLODVisibilityUpdate = true;

				// Clear loading state of actors if we are going to always show HLODs
				if (bCachedShowHLODsOverLoadedRegions)
				{
					HLODEditorData->ClearLoadedActorsState();
				}
				else
				{
					bForceHLODStateUpdate = true;
				}
			}

			// Actors or regions were loaded ?
			if (bForceHLODStateUpdate && !bCachedShowHLODsOverLoadedRegions)
			{
				HLODEditorData->UpdateLoadedActorsState();
				bForceHLODVisibilityUpdate = true;
			}
			
			// Min/Max draw distance for HLODs was changed ?
			if (WorldPartitionEditorModule->GetHLODInEditorMinDrawDistance() != CachedHLODMinDrawDistance ||
				WorldPartitionEditorModule->GetHLODInEditorMaxDrawDistance() != CachedHLODMaxDrawDistance)
			{
				CachedHLODMinDrawDistance = WorldPartitionEditorModule->GetHLODInEditorMinDrawDistance();
				CachedHLODMaxDrawDistance = WorldPartitionEditorModule->GetHLODInEditorMaxDrawDistance();
				bForceHLODVisibilityUpdate = true;
			}

			UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();
			if (UnrealEditorSubsystem)
			{
				FVector CameraLocation;
				FRotator CameraRotation;
				UnrealEditorSubsystem->GetLevelViewportCameraInfo(CameraLocation, CameraRotation);

				// Camera was moved ?
				const bool bCameraMoved = CameraLocation != CachedCameraLocation;
				if (bCameraMoved)
				{
					CachedCameraLocation = CameraLocation;
				}

				if (bForceHLODVisibilityUpdate || bCameraMoved)
				{					
					HLODEditorData->UpdateVisibility(CameraLocation, CachedHLODMinDrawDistance, CachedHLODMaxDrawDistance, bForceHLODVisibilityUpdate);
				}
			}
			bForceHLODStateUpdate = false;
		}
	}
}

TStatId UWorldPartitionHLODEditorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(WorldPartitionHLODEditorSubsystem, STATGROUP_Tickables);
}

void UWorldPartitionHLODEditorSubsystem::AddHLODSettingsFilter(EHLODSettingsVisibility InSettingsVisibility, TSoftObjectPtr<UStruct> InStruct, FName InPropertyName)
{
	StructsPropertiesVisibility.FindOrAdd(InSettingsVisibility).FindOrAdd(InStruct).Add(InPropertyName);
}

#undef LOCTEXT_NAMESPACE
