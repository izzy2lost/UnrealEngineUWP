// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSceneRigSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/Array.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "UObject/Class.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/NoExportTypes.h"

DEFINE_LOG_CATEGORY(AvaSceneRigSubsystemLog);

#define LOCTEXT_NAMESPACE "AvaSceneRigSubsystem"

void UAvaSceneRigSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UAvaSceneRigSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

UAvaSceneRigSubsystem* UAvaSceneRigSubsystem::ForWorld(const UWorld* const InWorld)
{
	if (IsValid(InWorld))
	{
		return InWorld->GetSubsystem<UAvaSceneRigSubsystem>();
	}
	return nullptr;
}

bool UAvaSceneRigSubsystem::ShouldCreateSubsystem(UObject* const InOuter) const
{
	if (!IsValid(InOuter))
	{
		return false;
	}

	const FAssetData AssetData(InOuter);

	// Only create scene rig subsystems for Motion Design scenes
	const FAssetDataTagMapSharedView::FFindTagResult SceneTagResult = AssetData.TagsAndValues.FindTag(TEXT("MotionDesignScene"));
	return SceneTagResult.IsSet() && SceneTagResult.GetValue().Equals(TEXT("Enabled"));
}

bool UAvaSceneRigSubsystem::IsSceneRigAssetData(const FAssetData& InAssetData)
{
	if (!InAssetData.IsValid())
	{
		return false;
	}

	UWorld* const SceneRigAsset = Cast<UWorld>(InAssetData.GetAsset());
	if (!IsValid(SceneRigAsset) || !IsValid(SceneRigAsset->PersistentLevel))
	{
		return false;
	}

	if (IInterface_AssetUserData* AssetUserData = Cast<IInterface_AssetUserData>(SceneRigAsset->PersistentLevel))
	{
		UAvaSceneRigData* const SceneRigData = AssetUserData->GetAssetUserData<UAvaSceneRigData>();
		if (IsValid(SceneRigData))
		{
			return true;
		}
	}

	return false;
}

bool UAvaSceneRigSubsystem::IsSceneRigAsset(UObject* const InObject)
{
	return IsSceneRigAssetData(FAssetData(InObject));
}

FString UAvaSceneRigSubsystem::GetSceneRigAssetSuffix()
{
	static const TCHAR* Suffix = TEXT("_SceneRig");
	return Suffix;
}

TSet<FName> UAvaSceneRigSubsystem::GetSupportedActorClassNames()
{
	static const TSet<FName> ClassNames =
		{
			// Cameras
			TEXT("CameraActor"),
			TEXT("CineCameraActor"),
			TEXT("AvaCineCameraActor"),
			// Lights
			TEXT("SkyLight"),
			TEXT("DirectionalLight"),
			TEXT("PointLight"),
			TEXT("RectLight"),
			TEXT("SpotLight"),
			// Misc
			TEXT("AvaNullActor"),
			TEXT("PostProcessVolume")
		};
	return ClassNames;
}

bool UAvaSceneRigSubsystem::IsSupportedActorClassName(const FName InName)
{
	return GetSupportedActorClassNames().Contains(InName);
}

bool UAvaSceneRigSubsystem::IsSupportedActorClass(const UClass* InClass)
{
	if (IsValid(InClass))
	{
		return GetSupportedActorClassNames().Contains(InClass->GetFName());
	}
	return false;
}

bool UAvaSceneRigSubsystem::AreActorsSupported(const TArray<AActor*>& InActors)
{
	bool bAllItemClassesSupported = !InActors.IsEmpty();

	for (const AActor* const Actor : InActors)
	{
		if (!UAvaSceneRigSubsystem::IsSupportedActorClass(Actor->GetClass()))
		{
			bAllItemClassesSupported = false;
			break;
		}
	}

	return bAllItemClassesSupported;
}

ULevelStreaming* UAvaSceneRigSubsystem::SceneRigFromActor(AActor* const InActor)
{
	const UWorld* const World = InActor->GetLevel()->GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	const UAvaSceneRigSubsystem* const SceneRigSubsystem = ForWorld(World);
	if (!IsValid(SceneRigSubsystem))
	{
		return nullptr;
	}

	return SceneRigSubsystem->FindFirstActiveSceneRig();
}

bool UAvaSceneRigSubsystem::AreAllActorsInLevel(ULevel* const InLevel, const TArray<AActor*>& InActors)
{
	if (!IsValid(InLevel))
	{
		return false;
	}

	for (const AActor* const Actor : InActors)
	{
		if (!InLevel->Actors.Contains(Actor))
		{
			return false;
		}
	}

	return true;
}

bool UAvaSceneRigSubsystem::AreSomeActorsInLevel(ULevel* const InLevel, const TArray<AActor*>& InActors)
{
	if (!IsValid(InLevel))
	{
		return false;
	}

	for (const AActor* const Actor : InActors)
	{
		if (InLevel->Actors.Contains(Actor))
		{
			return true;
		}
	}

	return false;
}

TArray<ULevelStreaming*> UAvaSceneRigSubsystem::FindAllSceneRigs() const
{
	TArray<ULevelStreaming*> OutStreamingLevels;

	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	for (ULevelStreaming* const LevelStreaming : GetWorldRef().GetStreamingLevels())
	{
		if (const TSoftObjectPtr<UWorld>& WorldAsset = LevelStreaming->GetWorldAsset())
		{
			if (IsSceneRigAsset(WorldAsset.Get()))
			{
				OutStreamingLevels.Add(LevelStreaming);
			}
		}
	}

	return OutStreamingLevels;
}

ULevelStreaming* UAvaSceneRigSubsystem::FindFirstActiveSceneRig() const
{
	const TArray<ULevelStreaming*> SceneRigs = FindAllSceneRigs();
	if (SceneRigs.IsEmpty())
	{
		return nullptr;
	}

	return SceneRigs[0];
}

UWorld* UAvaSceneRigSubsystem::FindFirstActiveSceneRigAsset() const
{
	ULevelStreaming* const ActiveSceneRig = UAvaSceneRigSubsystem::FindFirstActiveSceneRig();
	if (!IsValid(ActiveSceneRig))
	{
		return nullptr;
	}

	return ActiveSceneRig->GetWorldAsset().Get();
}

bool UAvaSceneRigSubsystem::IsActiveSceneRigActor(AActor* const InActor) const
{
	ULevelStreaming* const SceneRig = FindFirstActiveSceneRig();
	if (IsValid(SceneRig))
	{
		const UWorld* const WorldAsset = SceneRig->GetWorldAsset().Get();
		if (IsValid(WorldAsset) && IsValid(WorldAsset->PersistentLevel))
		{
			return WorldAsset->PersistentLevel->Actors.Contains(InActor);
		}
	}
	return false;
}

void UAvaSceneRigSubsystem::ForEachActiveSceneRigActor(TFunction<void(AActor* const InActor)> InFunction) const
{
	ULevelStreaming* const ActiveSceneRig = FindFirstActiveSceneRig();
	if (IsValid(ActiveSceneRig))
	{
		UWorld* const SceneRigAsset = ActiveSceneRig->GetWorldAsset().Get();
		if (IsValid(SceneRigAsset) && IsValid(SceneRigAsset->PersistentLevel))
		{
			for (AActor* const Actor : SceneRigAsset->PersistentLevel->Actors)
			{
				if (IsValid(Actor))
				{
					InFunction(Actor);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
