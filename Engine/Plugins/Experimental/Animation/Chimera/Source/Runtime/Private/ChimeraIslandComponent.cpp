// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chimera/ChimeraIslandComponent.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Chimera/ChimeraAsset.h"
#include "Chimera/ChimeraDefines.h"
#include "Chimera/ChimeraLibrary.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"

namespace UE::Chimera
{
#if ENABLE_ANIM_DEBUG
	static TAutoConsoleVariable<bool> CVarChimeraShowIslands(TEXT("a.Chimera.ShowIslands"), false, TEXT("Show Chimera Islands"));
#endif

	static UE::Chimera::FSearchResult InitSearchResult(const UE::PoseSearch::FSearchResult& SearchResult, int32 SearchIndex, const UE::Chimera::FSearchContext& SearchContext)
	{
		UE::Chimera::FSearchResult ChimeraSearchResult;
		static_cast<UE::PoseSearch::FSearchResult&>(ChimeraSearchResult) = SearchResult;
		ChimeraSearchResult.SearchIndex = SearchIndex;

		// @todo: WIP! calculating warping: currently supporting only UChimeraAsset(s), but we should generalize for UMultiAnimAsset(s)
		if (const UE::PoseSearch::FSearchIndexAsset* SearchIndexAsset = SearchResult.GetSearchIndexAsset())
		{
			if (const FPoseSearchDatabaseMultiAnimAsset* DatabaseMultiAnimAsset = SearchResult.Database->GetDatabaseAnimationAsset<FPoseSearchDatabaseMultiAnimAsset>(*SearchIndexAsset))
			{
				if (const UChimeraAsset* ChimeraAsset = Cast<UChimeraAsset>(DatabaseMultiAnimAsset->GetAnimationAsset()))
				{
					check(ChimeraAsset->GetNumRoles() == SearchContext.AnimInstances.Num());
					check(ChimeraAsset->GetNumRoles() == SearchContext.HistoryCollectors.Num());
					check(ChimeraAsset->GetNumRoles() == SearchContext.Roles.Num());

					UE::PoseSearch::FRoleToIndex SearchContextRoleToIndex;
					SearchContextRoleToIndex.Reserve(SearchContext.Roles.Num());
					for (int32 SearchContextRoleIndex = 0; SearchContextRoleIndex < SearchContext.Roles.Num(); ++SearchContextRoleIndex)
					{
						SearchContextRoleToIndex.Add(SearchContext.Roles[SearchContextRoleIndex]) = SearchContextRoleIndex;
					}

					// mapping ChimeraAsset Roles to SearchContext.* indexes
					TArray<FTransform, TInlineAllocator<UE::PoseSearch::PreallocatedRolesNum>> ActorRootBoneTransforms;
					const UWorld* DebugDrawWorld = nullptr;
					for (int32 ChimeraAssetRoleIndex = 0; ChimeraAssetRoleIndex < ChimeraAsset->GetNumRoles(); ++ChimeraAssetRoleIndex)
					{
						const UE::PoseSearch::FRole& ChimeraAssetRole = ChimeraAsset->GetRole(ChimeraAssetRoleIndex);
						const int32 SearchContextIndex = SearchContextRoleToIndex[ChimeraAssetRole];

						if (UAnimInstance* AnimInstance = SearchContext.AnimInstances[SearchContextIndex].Get())
						{
							const FTransform RootBoneTransform = AnimInstance->GetSkelMeshComponent()->GetBoneTransform(0);
							ActorRootBoneTransforms.Add(RootBoneTransform);
					
							if (!DebugDrawWorld)
							{
								DebugDrawWorld = AnimInstance->GetWorld();
							}
						}
					}

					if (ActorRootBoneTransforms.Num() == ChimeraAsset->GetNumRoles())
					{
						// FullAlignedActorRootBoneTransforms is mapped to the ChimeraAsset roles:
						// FullAlignedActorRootBoneTransforms[0] is for ChimeraAsset->GetRole(0)
						TArray<FTransform, TInlineAllocator<UE::PoseSearch::PreallocatedRolesNum>> FullAlignedActorRootBoneTransforms;
						FullAlignedActorRootBoneTransforms.SetNum(ChimeraAsset->GetNumRoles());

						ChimeraAsset->CalculateWarpTransforms(SearchResult.AssetTime, ActorRootBoneTransforms, FullAlignedActorRootBoneTransforms, DebugDrawWorld);
						ChimeraSearchResult.FullAlignedActorRootBoneTransforms.SetNum(ChimeraAsset->GetNumRoles());

						for (int32 ChimeraAssetRoleIndex = 0; ChimeraAssetRoleIndex < ChimeraAsset->GetNumRoles(); ++ChimeraAssetRoleIndex)
						{
							const UE::PoseSearch::FRole& ChimeraAssetRole = ChimeraAsset->GetRole(ChimeraAssetRoleIndex);
							const int32 SearchContextIndex = SearchContextRoleToIndex[ChimeraAssetRole];

							ChimeraSearchResult.FullAlignedActorRootBoneTransforms[SearchContextIndex] = FullAlignedActorRootBoneTransforms[ChimeraAssetRoleIndex];
						}
					}
				}
			}
		}

		return ChimeraSearchResult;
	}

	bool FSearchContext::IsValid() const
	{
		if (Database == nullptr)
		{
			return false;
		}

		const int32 Num = AnimInstances.Num();
		if (Num < 1)
		{
			return false;
		}

		if (Num != HistoryCollectors.Num())
		{
			return false;
		}

		if (Num != Roles.Num())
		{
			return false;
		}

		for (int32 IndexA = 0; IndexA < Num; ++IndexA)
		{
			if (AnimInstances[IndexA] == nullptr)
			{
				return false;
			}

			for (int32 IndexB = IndexA + 1; IndexB < Num; ++IndexB)
			{
				if (AnimInstances[IndexA] == AnimInstances[IndexB])
				{
					return false;
				}
			}
		}

		for (int32 IndexA = 0; IndexA < Num; ++IndexA)
		{
			for (int32 IndexB = IndexA + 1; IndexB < Num; ++IndexB)
			{
				if (Roles[IndexA] == Roles[IndexB])
				{
					return false;
				}
			}
		}

		for (int32 IndexA = 0; IndexA < Num; ++IndexA)
		{
			if (HistoryCollectors[IndexA] == nullptr)
			{
				return false;
			}

			for (int32 IndexB = IndexA + 1; IndexB < Num; ++IndexB)
			{
				if (HistoryCollectors[IndexA] == HistoryCollectors[IndexB])
				{
					return false;
				}
			}
		}

		return true;
	}

	bool FSearchContext::IsEquivalent(const FSearchContext& Other) const
	{
		if (Database != Other.Database)
		{
			return false;
		}

		const int32 Num = AnimInstances.Num();
		if (Num != Other.AnimInstances.Num())
		{
			return false;
		}

		int32 CommonRoledAnimInstances = 0;
		for (int32 IndexThis = 0; IndexThis < Num; ++IndexThis)
		{
			for (int32 IndexOther = 0; IndexOther < Num; ++IndexOther)
			{
				if (AnimInstances[IndexThis] == Other.AnimInstances[IndexOther] &&
					HistoryCollectors[IndexThis] == Other.HistoryCollectors[IndexOther] &&
					Roles[IndexThis] == Other.Roles[IndexOther])
				{
					++CommonRoledAnimInstances;
				}
			}
		}

		// using >= in case there are duplicated animinstances in this or Other (this->IsValid() should be false!)
		if (CommonRoledAnimInstances >= Num)
		{
			return true;
		}

		return false;
	}
}

void UChimeraIslandComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Called before any skeletal mesh component tick, when there aren't animation jobs flying. No need to FScopeLock Lock(&Mutex);
	using namespace UE::Chimera;

	// generating trajectories before running any of the skeletal mesh component ticks
	for (FSearchContext& SearchContext : SearchContexts)
	{
		for (int32 Index = 0; Index < SearchContext.AnimInstances.Num(); ++Index)
		{
			if (UAnimInstance* AnimInstance = SearchContext.AnimInstances[Index].Get())
			{
				// since UChimeraIslandComponent has a tick dependency with the USkeletalMeshComponent it's safe modify the FAnimNode_PoseSearchHistoryCollector_Base
				FAnimNode_PoseSearchHistoryCollector_Base* HistoryCollector = const_cast<FAnimNode_PoseSearchHistoryCollector_Base*>(SearchContext.HistoryCollectors[Index]);
				check(HistoryCollector);
				HistoryCollector->GenerateTrajectory(AnimInstance);
			}
		}
	}
}

void UChimeraIslandComponent::DebugDraw(const FColor& Color) const
{
	// called only by UChimeraSubsystem::Tick so no need to lock SearchResultsMutex to protect the read of SearchContexts
#if ENABLE_DRAW_DEBUG
	using namespace UE::Chimera;

	check(IsInGameThread());

	if (CVarChimeraShowIslands.GetValueOnAnyThread())
	{
		for (const FSearchContext& SearchContext : SearchContexts)
		{
			for (int32 Index = 0; Index < SearchContext.AnimInstances.Num(); ++Index)
			{
				if (const UAnimInstance* AnimInstance = SearchContext.AnimInstances[Index].Get())
				{
					const FVector Position = AnimInstance->GetSkelMeshComponent()->GetComponentLocation();
					const float BroadPhaseRadius = SearchContext.BroadPhaseRadiuses[Index];
					DrawDebugCircle(GetWorld(), Position, BroadPhaseRadius, 40, Color, false, 0.f, SDPG_Foreground, 0.f, FVector::XAxisVector, FVector::YAxisVector, false);
				}
			}
		}
	}
#endif // ENABLE_DRAW_DEBUG
}

void UChimeraIslandComponent::InjectToActor(AActor* Actor)
{
	check(IsInGameThread());

	// Called by UChimeraSubsystem::Tick when there aren't animation jobs flying. No need to FScopeLock Lock(&Mutex);
	if (Actor)
	{
		if (UCharacterMovementComponent* CharacterMovementComponent = Actor->GetComponentByClass<UCharacterMovementComponent>())
		{
			if (USkeletalMeshComponent* SkeletalMeshComponent = Actor->GetComponentByClass<USkeletalMeshComponent>())
			{
				// tick order: CharacterMovementComponent(s) -> ChimeraIslandComponent -> SkeletalMeshComponent(s)
				CharacterMovementComponents.AddUnique(CharacterMovementComponent);
				SkeletalMeshComponents.AddUnique(SkeletalMeshComponent);

				AddTickPrerequisiteComponent(CharacterMovementComponent);
				SkeletalMeshComponent->AddTickPrerequisiteComponent(this);
			}
		}
	}
}

void UChimeraIslandComponent::AddSearchContext(const UE::Chimera::FSearchContext& SearchContext)
{
#if DO_CHECK
	check(SearchContext.IsValid());
	check(IsInGameThread());

	for (const UE::Chimera::FSearchContext& ContainedSearchContext : SearchContexts)
	{
		check(!ContainedSearchContext.IsEquivalent(SearchContext));
	}
#endif // DO_CHECK
	SearchContexts.Add(SearchContext);
}

void UChimeraIslandComponent::Uninject()
{
	check(IsInGameThread());

	// Called by UChimeraSubsystem::Tick when there aren't animation jobs flying. No need to FScopeLock Lock(&Mutex);
	for (TWeakObjectPtr<UCharacterMovementComponent>& CharacterMovementComponentPtr : CharacterMovementComponents)
	{
		RemoveTickPrerequisiteComponent(CharacterMovementComponentPtr.Get());
	}

	for (TWeakObjectPtr<USkeletalMeshComponent>& SkeletalMeshComponentPtr : SkeletalMeshComponents)
	{
		SkeletalMeshComponentPtr->RemoveTickPrerequisiteComponent(this);
	}

	CharacterMovementComponents.Reset();
	SkeletalMeshComponents.Reset();

	SearchContexts.Reset();
	SearchResults.Reset();
	bSearchPerfomed = false;
}

bool UChimeraIslandComponent::IsUninjected()
{
	return SkeletalMeshComponents.IsEmpty();
}

bool UChimeraIslandComponent::DoSearch_AnyThread(UObject* AnimInstance, FChimeraBlueprintResult& Result)
{
	using namespace UE::Chimera;

	bool bDoPerfomSearch = false;
	{
		// thread safety note!
		// goal:	avoiding deadlock between SearchResultsMutex lock and waiting for UAnimInstance::HandleExistingParallelEvaluationTask.
		// why:		UPoseSearchLibrary::MotionMatch could call via AnimInstance GetProxyOnAnyThread<FAnimInstanceProxy>() that, if on GameThread, 
		//			could call UAnimInstance::HandleExistingParallelEvaluationTask 
		// fix:		avoid UPoseSearchLibrary::MotionMatch calls wrapped by any lock, at the cost of eventually (by design should be NEVER) performing the searches twice
		//			By design we should inject ticks dependencies (added by UChimeraIslandComponent::InjectToActor via AddTickPrerequisiteComponent), so concurrently 
		//			fly of UChimeraIslandComponent within the same island that requires searches is forbidden
		FScopeLock Lock(&SearchResultsMutex);
		bDoPerfomSearch = !bSearchPerfomed;
	}

	if (bDoPerfomSearch)
	{
		FMemMark Mark(FMemStack::Get());

		TArray<UAnimInstance*, TInlineAllocator<UE::PoseSearch::PreallocatedRolesNum, TMemStackAllocator<>>> AnimInstances;
		TArray<const UE::PoseSearch::IPoseHistory*, TInlineAllocator<UE::PoseSearch::PreallocatedRolesNum, TMemStackAllocator<>>> PoseHistories;
		TArray<UE::PoseSearch::FSearchResult, TMemStackAllocator<>> PoseSearchResults;

		// SearchContexts are modified only by UChimeraSubsystem::Tick and constant otherwise, so it's safe to access them in a threaded enviroment without locks
		PoseSearchResults.SetNum(SearchContexts.Num());

		for (int32 SearchIndex = 0; SearchIndex < SearchContexts.Num(); ++SearchIndex)
		{
			const FSearchContext& SearchContext = SearchContexts[SearchIndex];
			const UPoseSearchDatabase* Database = SearchContext.Database.Get();
			if (!Database)
			{
				UE_LOG(LogChimera, Error, TEXT("UChimeraIslandComponent::DoSearch_AnyThread invalid context database"));
				return false;
			}

			if (!Database->Schema)
			{
				UE_LOG(LogChimera, Error, TEXT("UChimeraIslandComponent::DoSearch_AnyThread invalid schema for context database %s"), *Database->GetName());
				return false;
			}

			AnimInstances.Reset();
			for (const TWeakObjectPtr<UAnimInstance>& AnimInstancePtr : SearchContext.AnimInstances)
			{
				UAnimInstance* SearchContextAnimInstance = AnimInstancePtr.Get();
				if (!SearchContextAnimInstance)
				{
					UE_LOG(LogChimera, Error, TEXT("UChimeraIslandComponent::DoSearch_AnyThread null anim instance"));
					return false;
				}

				AnimInstances.Add(SearchContextAnimInstance);
			}

			PoseHistories.Reset();
			for (const FAnimNode_PoseSearchHistoryCollector_Base* HistoryCollector : SearchContext.HistoryCollectors)
			{
				PoseHistories.Add(&HistoryCollector->GetPoseHistory());
			}

			const UObject* AssetsToSearch[] = { Database };
			FPoseSearchFutureProperties PoseSearchFutureProperties;

			// @todo: we could perform multiple UPoseSearchLibrary::MotionMatch in parallel!
			const UE::PoseSearch::FSearchResult PoseSearchResult = UPoseSearchLibrary::MotionMatch(AnimInstances, SearchContext.Roles,
				PoseHistories, AssetsToSearch, SearchContext.ContinuingProperties, PoseSearchFutureProperties);

			if (PoseSearchResult.PoseCost.GetTotalCost() < SearchContext.MaxCost)
			{
				PoseSearchResults[SearchIndex] = PoseSearchResult;
			}
		}

		// making sure we called Uninject()
		check(SearchResults.IsEmpty());

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// WIP!
		// @todo: figure out multiple policies to use the most characters? right now only the best search is "valid" with the most characters
		if (!PoseSearchResults.IsEmpty())
		{
			// locking to update SearchResults and bSearchPerfomed
			FScopeLock Lock(&SearchResultsMutex);

			int32 BestSearchIndex = INDEX_NONE;
			for (int32 SearchIndex = 0; SearchIndex < PoseSearchResults.Num(); ++SearchIndex)
			{
				if (PoseSearchResults[SearchIndex].IsValid())
				{
					if (BestSearchIndex == INDEX_NONE)
					{
						BestSearchIndex = SearchIndex;
					}
					else if (SearchContexts[SearchIndex].Roles.Num() > SearchContexts[BestSearchIndex].Roles.Num())
					{
						BestSearchIndex = SearchIndex;
					}
					else if (SearchContexts[SearchIndex].Roles.Num() == SearchContexts[BestSearchIndex].Roles.Num() &&
						PoseSearchResults[SearchIndex].PoseCost < PoseSearchResults[BestSearchIndex].PoseCost)
					{
						BestSearchIndex = SearchIndex;
					}
				}
			}

			if (BestSearchIndex != INDEX_NONE)
			{
				SearchResults.SetNum(1);
				SearchResults[0] = InitSearchResult(PoseSearchResults[BestSearchIndex], BestSearchIndex, SearchContexts[BestSearchIndex]);
			}
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			bSearchPerfomed = true;
	
			// calling this funtion within this scope since we already locked SearchResultsMutex
			return GetResult_AnyThread(AnimInstance, Result);
		}
	}

	return GetResult_AnyThread(AnimInstance, Result);
}

bool UChimeraIslandComponent::GetResult_AnyThread(UObject* AnimInstance, FChimeraBlueprintResult& Result)
{
	using namespace UE::Chimera;

	// locking to read SearchResults
	FScopeLock Lock(&SearchResultsMutex);

	// looking for AnimInstance in SearchResults to fill up Result
	for (const FSearchResult& SearchResult : SearchResults)
	{
		const FSearchContext& SearchContext = SearchContexts[SearchResult.SearchIndex];
		for (int32 AnimInstanceIndex = 0; AnimInstanceIndex < SearchContext.AnimInstances.Num(); ++AnimInstanceIndex)
		{
			if (SearchContext.AnimInstances[AnimInstanceIndex].Get() == AnimInstance)
			{
				const UPoseSearchDatabase* Database = SearchResult.Database.Get();
				check(Database);

				const UE::PoseSearch::FSearchIndexAsset* SearchIndexAsset = SearchResult.GetSearchIndexAsset();
				check(SearchIndexAsset);

				const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = Database->GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimationAssetBase>(*SearchIndexAsset);
				check(DatabaseAnimationAssetBase);

				const UE::PoseSearch::FRole Role = SearchContext.Roles[AnimInstanceIndex];

				Result.SelectedAnimation = DatabaseAnimationAssetBase->GetAnimationAsset();
				Result.SelectedTime = SearchResult.AssetTime;
				Result.bIsContinuingPoseSearch = SearchResult.bIsContinuingPoseSearch;
				Result.bLoop = SearchIndexAsset->IsLooping();
				Result.bIsMirrored = SearchIndexAsset->IsMirrored();
				Result.BlendParameters = SearchIndexAsset->GetBlendParameters();
				Result.SelectedDatabase = Database;
				Result.SearchCost = SearchResult.PoseCost.GetTotalCost();
				Result.Role = Role;

				if (SearchResult.FullAlignedActorRootBoneTransforms.IsValidIndex(AnimInstanceIndex))
				{
					Result.FullAlignedActorRootBoneTransform = SearchResult.FullAlignedActorRootBoneTransforms[AnimInstanceIndex];
				}

				//Result.bIsFromContinuingPlaying = SearchResult.bIsContinuingPoseSearch;

				// figuring out the WantedPlayRate
				Result.WantedPlayRate = 1.f;
				//if (Future.Animation && Future.IntervalTime > 0.f)
				//{
				//	if (const UPoseSearchFeatureChannel_PermutationTime* PermutationTimeChannel = Database->Schema->FindFirstChannelOfType<UPoseSearchFeatureChannel_PermutationTime>())
				//	{
				//		const FSearchIndex& SearchIndex = Database->GetSearchIndex();
				//		if (!SearchIndex.IsValuesEmpty())
				//		{
				//			TConstArrayView<float> ResultData = Database->GetSearchIndex().GetPoseValues(SearchResult.PoseIdx);
				//			const float ActualIntervalTime = PermutationTimeChannel->GetPermutationTime(ResultData);
				//			ProviderResult.WantedPlayRate = ActualIntervalTime / Future.IntervalTime;
				//		}
				//	}
				//}

				// we found our AnimInstance: we can stop searching
				return true;
			}
		}
	}

	return false;
}

const UE::Chimera::FSearchResult* UChimeraIslandComponent::FindSearchResult(const UE::Chimera::FSearchContext& SearchContext) const
{
	// called only by UChimeraSubsystem::Tick via UChimeraSubsystem::PopulateContinuingProperties so no need to lock SearchResultsMutex to protect the read of SearchResults
	check(IsInGameThread());

	using namespace UE::Chimera;

	// searching for InSearchContext in all the SearchContexts referenced by valid active SearchResults
	for (const FSearchResult& SearchResult : SearchResults)
	{
		const FSearchContext& LocalSearchContext = SearchContexts[SearchResult.SearchIndex];
		if (LocalSearchContext.Database == SearchContext.Database &&
			LocalSearchContext.AnimInstances == SearchContext.AnimInstances &&
			LocalSearchContext.Roles == SearchContext.Roles)
		{
			return &SearchResult;
		}
	}
	return nullptr;
}
