// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionLibrary.h"
#include "AvaTransitionContext.h"
#include "AvaTransitionLayer.h"
#include "AvaTransitionLayerUtils.h"
#include "AvaTransitionSubsystem.h"
#include "Behavior/AvaTransitionBehaviorInstance.h"
#include "Behavior/AvaTransitionBehaviorInstanceCache.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "IAvaTransitionNodeInterface.h"
#include "UObject/Package.h"

bool UAvaTransitionLibrary::IsTransitionActiveInLayer(UObject* InTransitionNode
	, EAvaTransitionComparisonResult InSceneComparisonType
	, EAvaTransitionLayerCompareType InLayerComparisonType
	, const FAvaTagHandleContainer& InSpecificLayers)
{
	const IAvaTransitionNodeInterface* NodeInterface = Cast<IAvaTransitionNodeInterface>(InTransitionNode);
	if (!NodeInterface)
	{
		return false;
	}

	const FAvaTransitionContext* TransitionContext = NodeInterface->GetBehaviorInstanceCache().GetTransitionContext();
	if (!TransitionContext)
	{
		return false;
	}

	const FAvaTransitionScene* TransitionScene = TransitionContext->GetTransitionScene();
	if (!TransitionScene)
	{
		return false;
	}

	ULevel* TransitionLevel = TransitionScene->GetLevel();
	if (!TransitionLevel || !TransitionLevel->OwningWorld)
	{
		return false;
	}

	UAvaTransitionSubsystem* TransitionSubsystem = TransitionLevel->OwningWorld->GetSubsystem<UAvaTransitionSubsystem>();
	if (!TransitionSubsystem)
	{
		return false;
	}

	const FAvaTransitionLayerComparator Comparator = FAvaTransitionLayerUtils::BuildComparator(*TransitionContext
		, InLayerComparisonType
		, InSpecificLayers);

	const TArray<const FAvaTransitionBehaviorInstance*> BehaviorInstances = FAvaTransitionLayerUtils::QueryBehaviorInstances(*TransitionSubsystem, Comparator);
	if (BehaviorInstances.IsEmpty())
	{
		return false;
	}

	bool bHasMatchingInstance = BehaviorInstances.ContainsByPredicate(
		[TransitionScene, InSceneComparisonType](const FAvaTransitionBehaviorInstance* InInstance)
		{
			const FAvaTransitionScene* OtherTransitionScene = InInstance->GetTransitionContext().GetTransitionScene();

			const EAvaTransitionComparisonResult ComparisonResult = OtherTransitionScene
				? TransitionScene->Compare(*OtherTransitionScene)
				: EAvaTransitionComparisonResult::None;

			return ComparisonResult == InSceneComparisonType;
		});

	return bHasMatchingInstance;
}

EAvaTransitionType UAvaTransitionLibrary::GetTransitionType(UObject* InTransitionNode)
{
	const IAvaTransitionNodeInterface* NodeInterface = Cast<IAvaTransitionNodeInterface>(InTransitionNode);
	if (!NodeInterface)
	{
		return EAvaTransitionType::None;
	}

	const FAvaTransitionContext* TransitionContext = NodeInterface->GetBehaviorInstanceCache().GetTransitionContext();
	if (!TransitionContext)
	{
		return EAvaTransitionType::None;
	}

	return TransitionContext->GetTransitionType();
}

bool UAvaTransitionLibrary::IsOtherSceneTransitioning(UObject* InTransitionNode, const TSoftObjectPtr<UWorld>& InSceneAsset)
{
	const IAvaTransitionNodeInterface* NodeInterface = Cast<IAvaTransitionNodeInterface>(InTransitionNode);
	if (!NodeInterface)
	{
		return false;
	}

	const FAvaTransitionContext* TransitionContext = NodeInterface->GetBehaviorInstanceCache().GetTransitionContext();
	if (!TransitionContext)
	{
		return false;
	}

	const FAvaTransitionScene* TransitionScene = TransitionContext->GetTransitionScene();
	if (!TransitionScene)
	{
		return false;
	}

	ULevel* TransitionLevel = TransitionScene->GetLevel();
	if (!TransitionLevel || !TransitionLevel->OwningWorld)
	{
		return false;
	}

	UAvaTransitionSubsystem* TransitionSubsystem = TransitionLevel->OwningWorld->GetSubsystem<UAvaTransitionSubsystem>();
	if (!TransitionSubsystem)
	{
		return false;
	}

	const FAvaTransitionLayerComparator Comparator = FAvaTransitionLayerUtils::BuildComparator(*TransitionContext
		, EAvaTransitionLayerCompareType::Different
		, FAvaTagHandleContainer());

	TArray<const FAvaTransitionBehaviorInstance*> BehaviorInstances = FAvaTransitionLayerUtils::QueryBehaviorInstances(*TransitionSubsystem, Comparator);
	if (BehaviorInstances.IsEmpty())
	{
		return false;
	}

	bool bFoundInstance = BehaviorInstances.ContainsByPredicate(
		[TransitionScene, &InSceneAsset](const FAvaTransitionBehaviorInstance* InInstance)
		{
			const FAvaTransitionScene* const OtherTransitionScene = InInstance->GetTransitionContext().GetTransitionScene();

			// ignore check on scenes marked as needing discard
			if (!OtherTransitionScene || OtherTransitionScene->HasAnyFlags(EAvaTransitionSceneFlags::NeedsDiscard))
			{
				return false;
			}

			const ULevel* const OtherLevel = OtherTransitionScene->GetLevel();
			if (!OtherLevel)
			{
				return false;
			}

			const UPackage* const OtherPackage = OtherLevel->GetPackage();
			if (!OtherPackage)
			{
				return false;
			}

			// Remove the /Temp from the Package Name
			FString PackageName = OtherPackage->GetName();
			if (PackageName.StartsWith(TEXT("/Temp")))
			{
				PackageName.RightChopInline(-1 + sizeof(TEXT("/Temp")) / sizeof(TCHAR));
			}

			// Remove the _LevelInstance_[Num] from the Package Name
			const int32 LevelInstancePosition = PackageName.Find(TEXT("_LevelInstance_"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (LevelInstancePosition != INDEX_NONE)
			{
				PackageName.LeftChopInline(PackageName.Len() - LevelInstancePosition);
			}

			const FString SceneAssetName = InSceneAsset.GetLongPackageName();
			return PackageName == SceneAssetName;
		});

	return bFoundInstance;
}
