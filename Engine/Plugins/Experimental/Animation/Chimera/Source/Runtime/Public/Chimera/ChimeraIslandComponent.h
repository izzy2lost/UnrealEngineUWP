// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "ChimeraIslandComponent.generated.h"

struct FChimeraBlueprintResult;
struct FPoseSearchContinuingProperties;
class UCharacterMovementComponent;

namespace UE::Chimera
{
	struct FSearchContext
	{
		// @todo: since AnimInstances, HistoryCollectors, Roles, and BroadPhaseRadiuses have the same cardinality,
		// add an array of structs instead of many arrays
		TArray<TWeakObjectPtr<UAnimInstance>> AnimInstances;
		// @todo: perhaps use the animnode of the pose history collector instead
		TArray<const FAnimNode_PoseSearchHistoryCollector_Base*> HistoryCollectors;
		TArray<UE::PoseSearch::FRole> Roles;

#if ENABLE_DRAW_DEBUG
		TArray<float> BroadPhaseRadiuses;
#endif // ENABLE_DRAW_DEBUG

		float MaxCost = MAX_flt;

		TWeakObjectPtr<const UPoseSearchDatabase> Database;
		FPoseSearchContinuingProperties ContinuingProperties;

		bool IsValid() const;
		bool IsEquivalent(const FSearchContext& Other) const;
	};

	struct FSearchResult : public UE::PoseSearch::FSearchResult
	{
		int32 SearchIndex = INDEX_NONE;
		TArray<FTransform, TInlineAllocator<UE::PoseSearch::PreallocatedRolesNum>> FullAlignedActorRootBoneTransforms;
	};
} // namespace UE::Chimera

// UChimeraIslandComponent is a component injected between the interacting actors UCharacterMovementComponent and USkeletalMeshComponent
// to create a execution threading fence to be able to perform motion matching searches between the involved characters in a thread safe manner.
// Look at UChimeraSubsystem "Execution model and threading details" for additional information
UCLASS(Experimental, Category="Animation|Chimera")
class UChimeraIslandComponent : public UActorComponent
{
public:
	GENERATED_BODY()

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	void InjectToActor(AActor* Actor);
	void UninjectFromAllActors();
	bool IsUninjected();

	bool DoSearch_AnyThread(UObject* AnimInstance, FChimeraBlueprintResult& Result);

	void AddSearchContext(const UE::Chimera::FSearchContext& SearchContext);
	void ResetSearchContexts();
	void ResetSearchResults();

	void DebugDraw(const FColor& Color = FColor::Red) const;

	const TArray<TWeakObjectPtr<UCharacterMovementComponent>>& GetCharacterMovementComponents() const { return CharacterMovementComponents; }
	const TArray<TWeakObjectPtr<USkeletalMeshComponent>>& GetSkeletalMeshComponents() const { return SkeletalMeshComponents; }
	const TArray<UE::Chimera::FSearchContext>& GetSearchContexts() const { return SearchContexts; }
	const TArray<UE::Chimera::FSearchResult>& GetSearchResults() const { return SearchResults; }

	const UE::Chimera::FSearchResult* FindSearchResult(const UE::Chimera::FSearchContext& SearchContext) const;

private:
	TArray<TWeakObjectPtr<UCharacterMovementComponent>> CharacterMovementComponents;
	TArray<TWeakObjectPtr<USkeletalMeshComponent>> SkeletalMeshComponents;
	
	// there's one FSearchContext for each search we need to perform (including all the possible roles permutations). Added by UChimeraSubsystem::Tick
	TArray<UE::Chimera::FSearchContext> SearchContexts;

	// SearchResults contains only the best results, and it has not necessarly the same cardinality as SearchContexts. usually SearchResults.Num() < SearchContexts.Num()
	TArray<UE::Chimera::FSearchResult> SearchResults;
	bool bSearchPerfomed = false;

	FCriticalSection SearchResultsMutex;
};
