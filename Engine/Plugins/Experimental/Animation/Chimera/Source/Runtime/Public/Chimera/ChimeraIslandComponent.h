// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchLibrary.h"

struct FChimeraBlueprintResult;
struct FPoseSearchContinuingProperties;
class UCharacterMovementComponent;

namespace UE::Chimera
{
struct FIsland;

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

	bool operator==(const FSearchResult& Other) const;
};

// FIsland contains ticks functions injected between the interacting actors UCharacterMovementComponent and USkeletalMeshComponent
// to create a execution threading fence to be able to perform motion matching searches between the involved characters in a thread safe manner.
// Look at UChimeraSubsystem "Execution model and threading details" for additional information
struct FIsland
{
	UE_NONCOPYABLE(FIsland);
	
	FIsland(ULevel* Level);
	~FIsland();

	bool DoSearch_AnyThread(UObject* AnimInstance, const FPoseSearchContinuingProperties& ContinuingProperties, FChimeraBlueprintResult& Result);

	const TArray<TWeakObjectPtr<UCharacterMovementComponent>>& GetCharacterMovementComponents() const { return CharacterMovementComponents; }
	const TArray<TWeakObjectPtr<USkeletalMeshComponent>>& GetSkeletalMeshComponents() const { return SkeletalMeshComponents; }
	const TArray<UE::Chimera::FSearchContext>& GetSearchContexts() const { return SearchContexts; }
	const TArray<UE::Chimera::FSearchResult>& GetSearchResults() const { return SearchResults; }

	bool IsUninjected() const;
	void InjectToActor(AActor* Actor);
	void Uninject();

	const UE::Chimera::FSearchResult* FindSearchResult(const UE::Chimera::FSearchContext& SearchContext) const;
	void AddSearchContext(const UE::Chimera::FSearchContext& SearchContext);

	void DebugDraw(const FColor& Color = FColor::Red) const;

private:
	bool GetResult_AnyThread(UObject* AnimInstance, FChimeraBlueprintResult& Result);

	struct FPreTickFunction : public FTickFunction
	{
		virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
		virtual FString DiagnosticMessage() override { return TEXT("FPreTickFunction"); }
		FIsland* Island = nullptr;
	};
	FPreTickFunction PreTickFunction;

	struct FPostTickFunction : public FTickFunction
	{
		virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
		virtual FString DiagnosticMessage() override { return TEXT("FPostTickFunction"); }
	};
	FPostTickFunction PostTickFunction;

	TArray<TWeakObjectPtr<UCharacterMovementComponent>> CharacterMovementComponents;
	TArray<TWeakObjectPtr<USkeletalMeshComponent>> SkeletalMeshComponents;
	
	// there's one FSearchContext for each search we need to perform (including all the possible roles permutations). Added by UChimeraSubsystem::Tick
	TArray<UE::Chimera::FSearchContext> SearchContexts;

	// SearchResults contains only the best results, and it has not necessarly the same cardinality as SearchContexts. usually SearchResults.Num() < SearchContexts.Num()
	TArray<UE::Chimera::FSearchResult> SearchResults;
	bool bSearchPerfomed = false;

	FCriticalSection SearchResultsMutex;
};

} // namespace UE::Chimera
