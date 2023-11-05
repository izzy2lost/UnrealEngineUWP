// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchLibrary.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNode_Inertialization.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimRootMotionProvider.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSubsystem_Tag.h"
#include "Animation/BlendSpace.h"
#include "Animation/BuiltInAttributeTypes.h"
#include "Animation/AnimTrace.h"
#include "InstancedStruct.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearchFeatureChannel_Trajectory.h"
#include "PoseSearch/Trace/PoseSearchTraceLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PoseSearchLibrary)

#define LOCTEXT_NAMESPACE "PoseSearchLibrary"

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
TAutoConsoleVariable<int32> CVarAnimMotionMatchDrawQueryEnable(TEXT("a.MotionMatch.DrawQuery.Enable"), 0, TEXT("Enable / Disable MotionMatch Draw Query"));
TAutoConsoleVariable<int32> CVarAnimMotionMatchDrawMatchEnable(TEXT("a.MotionMatch.DrawMatch.Enable"), 0, TEXT("Enable / Disable MotionMatch Draw Match"));
TAutoConsoleVariable<int32> CVarAnimMotionMatchDrawHistoryEnable(TEXT("a.MotionMatch.DrawHistory.Enable"), 0, TEXT("Enable / Disable MotionMatch Draw History"));
#endif

namespace UE::PoseSearch
{
	static bool IsForceInterrupt(EPoseSearchInterruptMode InterruptMode, const UPoseSearchDatabase* CurrentResultDatabase, const TArray<TObjectPtr<const UPoseSearchDatabase>>& Databases)
	{
		switch (InterruptMode)
		{
		case EPoseSearchInterruptMode::DoNotInterrupt:
			return false;
		case EPoseSearchInterruptMode::InterruptOnDatabaseChange:
			return !Databases.Contains(CurrentResultDatabase);
		case EPoseSearchInterruptMode::ForceInterrupt:
			return true;
		default:
			unimplemented();
			return false;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FMotionMatchingState

void FMotionMatchingState::Reset(const FTransform& ComponentTransform)
{
	CurrentSearchResult.Reset();
	// Set the elapsed time to INFINITY to trigger a search right away
	ElapsedPoseSearchTime = INFINITY;
	WantedPlayRate = 1.f;
	bJumpedToPose = false;
	ComponentDeltaYaw = 0.f;
	ComponentWorldYaw = FRotator(ComponentTransform.GetRotation()).Yaw;
	AnimationDeltaYaw = 0.f;

	PoseIndicesHistory.Reset();

#if UE_POSE_SEARCH_TRACE_ENABLED
	RootMotionTransformDelta = FTransform::Identity;
#endif // UE_POSE_SEARCH_TRACE_ENABLED
}

void FMotionMatchingState::AdjustAssetTime(float AssetTime)
{
	CurrentSearchResult.Update(AssetTime);
}

void FMotionMatchingState::JumpToPose(const FAnimationUpdateContext& Context, const UE::PoseSearch::FSearchResult& Result, int32 MaxActiveBlends, float BlendTime)
{
	// Remember which pose and sequence we're playing from the database
	CurrentSearchResult = Result;

	bJumpedToPose = true;
}

FVector FMotionMatchingState::GetEstimatedFutureRootMotionVelocity() const
{
	using namespace UE::PoseSearch;
	if (CurrentSearchResult.IsValid())
	{
		if (const UPoseSearchFeatureChannel_Trajectory* TrajectoryChannel = CurrentSearchResult.Database->Schema->FindFirstChannelOfType<UPoseSearchFeatureChannel_Trajectory>())
		{
			TConstArrayView<float> ResultData = CurrentSearchResult.Database->GetSearchIndex().GetPoseValues(CurrentSearchResult.PoseIdx);
			return TrajectoryChannel->GetEstimatedFutureRootMotionVelocity(ResultData);
		}
	}

	return FVector::ZeroVector;
}

void FMotionMatchingState::UpdateWantedPlayRate(const UE::PoseSearch::FSearchContext& SearchContext, const FFloatInterval& PlayRate, float TrajectorySpeedMultiplier)
{
	if (CurrentSearchResult.IsValid())
	{
		if (!ensure(PlayRate.Min <= PlayRate.Max && PlayRate.Min > UE_KINDA_SMALL_NUMBER))
		{
			UE_LOG(LogPoseSearch, Error, TEXT("Couldn't update the WantedPlayRate in FMotionMatchingState::UpdateWantedPlayRate, because of invalid PlayRate interval (%f, %f)"), PlayRate.Min, PlayRate.Max);
			WantedPlayRate = 1.f;
		}
		else if (!FMath::IsNearlyEqual(PlayRate.Min, PlayRate.Max, UE_KINDA_SMALL_NUMBER))
		{
			if (const UE::PoseSearch::FFeatureVectorBuilder* PoseSearchFeatureVectorBuilder = SearchContext.GetCachedQuery(CurrentSearchResult.Database->Schema))
			{
				if (const UPoseSearchFeatureChannel_Trajectory* TrajectoryChannel = CurrentSearchResult.Database->Schema->FindFirstChannelOfType<UPoseSearchFeatureChannel_Trajectory>())
				{
					TConstArrayView<float> QueryData = PoseSearchFeatureVectorBuilder->GetValues();
					TConstArrayView<float> ResultData = CurrentSearchResult.Database->GetSearchIndex().GetPoseValues(CurrentSearchResult.PoseIdx);
					const float EstimatedSpeedRatio = TrajectoryChannel->GetEstimatedSpeedRatio(QueryData, ResultData);

					WantedPlayRate = FMath::Clamp(EstimatedSpeedRatio, PlayRate.Min, PlayRate.Max);
				}
				else
				{
					UE_LOG(LogPoseSearch, Warning,
						TEXT("Couldn't update the WantedPlayRate in FMotionMatchingState::UpdateWantedPlayRate, because Schema '%s' couldn't find a UPoseSearchFeatureChannel_Trajectory channel"),
						*GetNameSafe(CurrentSearchResult.Database->Schema));
				}
			}
		}
		else if (!FMath::IsNearlyZero(TrajectorySpeedMultiplier))
		{
			WantedPlayRate = PlayRate.Min / TrajectorySpeedMultiplier;
		}
		else
		{
			WantedPlayRate = PlayRate.Min;
		}
	}
}

void FMotionMatchingState::UpdateRootBoneControl(const FAnimationUpdateContext& Context, float YawFromAnimationBlendRate)
{
	const FAnimInstanceProxy* AnimInstanceProxy = Context.AnimInstanceProxy;

	const float CurrentComponentWorldYaw = FRotator(AnimInstanceProxy->GetComponentTransform().GetRotation()).Yaw;
	if (YawFromAnimationBlendRate < 0.f)
	{
		ComponentWorldYaw = CurrentComponentWorldYaw;
		ComponentDeltaYaw = 0.f;
	}
	else
	{
		// integrating MotionMatchingState.ComponentWorldYaw with a lerped value between 
		// the previous frame AnimationDeltaYaw and CurrentComponentDeltaYaw (component delta yaw happened during the delta time)
		const float CurrentComponentDeltaYaw = FRotator::NormalizeAxis(CurrentComponentWorldYaw - ComponentWorldYaw);

		// lerping the animation delta with the capsule delta
		const float LerpValue = FMath::Min(YawFromAnimationBlendRate * Context.GetDeltaTime(), 1.f);
		const float LerpedDeltaYaw = FMath::Lerp(AnimationDeltaYaw, CurrentComponentDeltaYaw, LerpValue);

		ComponentWorldYaw = FRotator::NormalizeAxis(ComponentWorldYaw + LerpedDeltaYaw);

		// @todo: handle the case when the character is on top of a rotating platform
		ComponentDeltaYaw = FRotator::NormalizeAxis(ComponentWorldYaw - CurrentComponentWorldYaw);
	}
}

#if UE_POSE_SEARCH_TRACE_ENABLED
void UPoseSearchLibrary::TraceMotionMatchingState(
	// Trajectory is the trajectory in mesh component space prior ProcessTrajectory
	const FPoseSearchQueryTrajectory& Trajectory, 
	// SearchContext.Trajectory is the trajectory in root bone component space after ProcessTrajectory
	UE::PoseSearch::FSearchContext& SearchContext,
	const UE::PoseSearch::FSearchResult& CurrentResult,
	float ElapsedPoseSearchTime,
	const FTransform& RootMotionTransformDelta,
	const UObject* AnimInstance,
	int32 NodeId,
	float DeltaTime,
	bool bSearch,
	float RecordingTime)
{
	using namespace UE::PoseSearch;
	
	const int32 CurrentPoseIdx = bSearch && CurrentResult.PoseCost.IsValid() ? CurrentResult.PoseIdx : INDEX_NONE;
	FTraceMotionMatchingState TraceState;
	TraceState.DatabaseEntries.SetNum(SearchContext.GetBestPoseCandidatesMap().Num());
	
	int32 DbEntryIdx = 0;
	for (TPair<const UPoseSearchDatabase*, FSearchContext::FBestPoseCandidates> DatabaseBestPoseCandidates : SearchContext.GetBestPoseCandidatesMap())
	{
		const UPoseSearchDatabase* Database = DatabaseBestPoseCandidates.Key;
		check(Database);

		FTraceMotionMatchingStateDatabaseEntry& DbEntry = TraceState.DatabaseEntries[DbEntryIdx];

		// if throttling is on, the continuing pose can be valid, but no actual search occurred, so the query will not be cached, and we need to build it
		DbEntry.QueryVector = SearchContext.GetOrBuildQuery(Database->Schema).GetValues();
		DbEntry.DatabaseId = FTraceMotionMatchingState::GetIdFromObject(Database);

		for (int32 CandidateIdx = 0; CandidateIdx < DatabaseBestPoseCandidates.Value.Num(); ++CandidateIdx)
		{
			const FSearchContext::FPoseCandidate PoseCandidate = DatabaseBestPoseCandidates.Value.GetUnsortedCandidate(CandidateIdx);

			FTraceMotionMatchingStatePoseEntry PoseEntry;
			PoseEntry.DbPoseIdx = PoseCandidate.PoseIdx;
			PoseEntry.Cost = PoseCandidate.Cost;
			PoseEntry.PoseCandidateFlags = PoseCandidate.PoseCandidateFlags;
			if (CurrentPoseIdx == PoseCandidate.PoseIdx && CurrentResult.Database.Get() == Database)
			{
				check(EnumHasAnyFlags(PoseEntry.PoseCandidateFlags, EPoseCandidateFlags::Valid_Pose | EPoseCandidateFlags::Valid_ContinuingPose));

				EnumAddFlags(PoseEntry.PoseCandidateFlags, EPoseCandidateFlags::Valid_CurrentPose);

				TraceState.CurrentDbEntryIdx = DbEntryIdx;
				TraceState.CurrentPoseEntryIdx = DbEntry.PoseEntries.Add(PoseEntry);
			}
			else
			{
				DbEntry.PoseEntries.Add(PoseEntry);
			}
		}

		++DbEntryIdx;
	}

	if (DeltaTime > SMALL_NUMBER)
	{
		// simulation
		if (SearchContext.IsTrajectoryValid())
		{
			const FTransform PrevRoot = SearchContext.GetWorldBoneTransformAtTime(-DeltaTime);
			const FTransform CurrRoot = SearchContext.GetWorldBoneTransformAtTime(0.f);
			const FTransform SimDelta = CurrRoot.GetRelativeTransform(PrevRoot);

			TraceState.SimLinearVelocity = SimDelta.GetTranslation().Size() / DeltaTime;
			TraceState.SimAngularVelocity = FMath::RadiansToDegrees(SimDelta.GetRotation().GetAngle()) / DeltaTime;
		}

		// animation
		TraceState.AnimLinearVelocity = RootMotionTransformDelta.GetTranslation().Size() / DeltaTime;
		TraceState.AnimAngularVelocity = FMath::RadiansToDegrees(RootMotionTransformDelta.GetRotation().GetAngle()) / DeltaTime;
	}

	TraceState.ElapsedPoseSearchTime = ElapsedPoseSearchTime;
	TraceState.AssetPlayerTime = CurrentResult.AssetTime;
	TraceState.DeltaTime = DeltaTime;

	TraceState.RecordingTime = RecordingTime;
	TraceState.SearchBestCost = CurrentResult.PoseCost.GetTotalCost();
	TraceState.SearchBruteForceCost = CurrentResult.BruteForcePoseCost.GetTotalCost();
	TraceState.SearchBestPosePos = CurrentResult.BestPosePos;

	TraceState.Trajectory = Trajectory;

	TraceState.Output(AnimInstance, NodeId);
}
#endif // UE_POSE_SEARCH_TRACE_ENABLED

void UPoseSearchLibrary::UpdateMotionMatchingState(
	const FAnimationUpdateContext& Context,
	const TArray<TObjectPtr<const UPoseSearchDatabase>>& Databases,
	const FPoseSearchQueryTrajectory& Trajectory,
	float TrajectorySpeedMultiplier,
	float BlendTime,
	int32 MaxActiveBlends,
	const FFloatInterval& PoseJumpThresholdTime,
	float PoseReselectHistory,
	float SearchThrottleTime,
	const FFloatInterval& PlayRate,
	FMotionMatchingState& InOutMotionMatchingState,
	float YawFromAnimationBlendRate,
	float YawFromAnimationTrajectoryBlendTime,
	EPoseSearchInterruptMode InterruptMode,
	bool bShouldSearch,
	bool bDebugDrawQuery,
	bool bDebugDrawCurResult,
	bool bDebugDrawPoseHistory)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_Update);

	using namespace UE::PoseSearch;

	check(Context.AnimInstanceProxy);

	if (Databases.IsEmpty())
	{
		Context.LogMessage(
			EMessageSeverity::Error,
			LOCTEXT("NoDatabases", "No database assets provided for motion matching."));
		return;
	}

	InOutMotionMatchingState.UpdateRootBoneControl(Context, YawFromAnimationBlendRate);

	const float DeltaTime = Context.GetDeltaTime();

	InOutMotionMatchingState.bJumpedToPose = false;

	const IPoseHistory* History = nullptr;
	if (IPoseHistoryProvider* PoseHistoryProvider = Context.GetMessage<IPoseHistoryProvider>())
	{
		History = &PoseHistoryProvider->GetPoseHistory();

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
		if (bDebugDrawPoseHistory)
		{
			History->DebugDraw(*Context.AnimInstanceProxy, FColor::Orange, &Trajectory);
		}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	}

	// @todo: perhaps store the previous frame root bone transform to use in ProcessTrajectory
	const FTransform& RootBoneTransform = Context.AnimInstanceProxy->GetSkeleton()->GetReferenceSkeleton().GetRefBonePose()[RootSchemaBoneIdx];
	const FPoseSearchQueryTrajectory TrajectoryRootSpace = ProcessTrajectory(Trajectory, RootBoneTransform, InOutMotionMatchingState.ComponentDeltaYaw, YawFromAnimationTrajectoryBlendTime, TrajectorySpeedMultiplier);

	FMemMark Mark(FMemStack::Get());
	const UAnimInstance* AnimInstance = Cast<const UAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject());
	check(AnimInstance);
	FSearchContext SearchContext(AnimInstance, History, TConstArrayView<const UAnimationAsset*>(), &TrajectoryRootSpace, 0.f,
		&InOutMotionMatchingState.PoseIndicesHistory, InOutMotionMatchingState.CurrentSearchResult, PoseJumpThresholdTime);

	const bool bCanAdvance = InOutMotionMatchingState.CurrentSearchResult.CanAdvance(DeltaTime);

	// If we can't advance or enough time has elapsed since the last pose jump then search
	const bool bSearch = !bCanAdvance || (bShouldSearch && (InOutMotionMatchingState.ElapsedPoseSearchTime >= SearchThrottleTime));
	if (bSearch)
	{
		InOutMotionMatchingState.ElapsedPoseSearchTime = 0.f;

		const UPoseSearchDatabase* CurrentResultDatabase = SearchContext.GetCurrentResult().Database.Get();
		const bool bForceInterrupt = IsForceInterrupt(InterruptMode, CurrentResultDatabase, Databases);

		// Evaluate continuing pose
		FSearchResult SearchResult;
		if (!bForceInterrupt && bCanAdvance)
		{
			SearchResult = CurrentResultDatabase->SearchContinuingPose(SearchContext);
			SearchContext.UpdateCurrentBestCost(SearchResult.PoseCost);
		}

		bool bJumpToPose = false;
		for (const TObjectPtr<const UPoseSearchDatabase>& Database : Databases)
		{
			if (ensure(Database))
			{
				FSearchResult NewSearchResult = Database->Search(SearchContext);
				if (NewSearchResult.PoseCost.GetTotalCost() < SearchResult.PoseCost.GetTotalCost())
				{
					bJumpToPose = true;
					SearchResult = NewSearchResult;
					SearchContext.UpdateCurrentBestCost(SearchResult.PoseCost);
				}
			}
		}

#if UE_POSE_SEARCH_TRACE_ENABLED
		if (!SearchResult.BruteForcePoseCost.IsValid())
		{
			SearchResult.BruteForcePoseCost = SearchResult.PoseCost;
		}
#endif // UE_POSE_SEARCH_TRACE_ENABLED

		
#if WITH_EDITOR
		// resetting CurrentSearchResult if any DDC indexing on the requested databases is still in progress
		if (SearchContext.IsAsyncBuildIndexInProgress())
		{
			InOutMotionMatchingState.CurrentSearchResult.Reset();
		}
#endif // WITH_EDITOR

#if !NO_LOGGING
		if (!SearchResult.IsValid())
		{
			TStringBuilder<1024> StringBuilder;
			StringBuilder << "UPoseSearchLibrary::UpdateMotionMatchingState invalid search result : ForceInterrupt [";
			StringBuilder << bForceInterrupt;
			StringBuilder << "], CanAdvance [";
			StringBuilder << bCanAdvance;
			StringBuilder << "], Indexing [";

#if WITH_EDITOR
			StringBuilder << SearchContext.IsAsyncBuildIndexInProgress();
//#else // WITH_EDITOR
			StringBuilder << false;
#endif // WITH_EDITOR

			StringBuilder << "], Databases [";

			for (int32 DatabaseIndex = 0; DatabaseIndex < Databases.Num(); ++DatabaseIndex)
			{
				StringBuilder << GetNameSafe(Databases[DatabaseIndex]);
				if (DatabaseIndex != Databases.Num() - 1)
				{
					StringBuilder << ", ";
				}
			}
			
			StringBuilder << "] ";

			FString String = StringBuilder.ToString();
			UE_LOG(LogPoseSearch, Warning, TEXT("%s"), *String);
		}
#endif // !NO_LOGGING

		if (bJumpToPose)
		{
			InOutMotionMatchingState.JumpToPose(Context, SearchResult, MaxActiveBlends, BlendTime);
		}
		else
		{
			// copying few properties of SearchResult into CurrentSearchResult to facilitate debug drawing
#if UE_POSE_SEARCH_TRACE_ENABLED
			InOutMotionMatchingState.CurrentSearchResult.BruteForcePoseCost = SearchResult.BruteForcePoseCost;
#endif // UE_POSE_SEARCH_TRACE_ENABLED
			InOutMotionMatchingState.CurrentSearchResult.PoseCost = SearchResult.PoseCost;
		}
	}
	else
	{
		InOutMotionMatchingState.ElapsedPoseSearchTime += DeltaTime;
	}

	InOutMotionMatchingState.UpdateWantedPlayRate(SearchContext, PlayRate, TrajectorySpeedMultiplier);

	InOutMotionMatchingState.PoseIndicesHistory.Update(InOutMotionMatchingState.CurrentSearchResult, DeltaTime, PoseReselectHistory);

#if UE_POSE_SEARCH_TRACE_ENABLED
	// Record debugger details
	if (IsTracing(Context))
	{
		TraceMotionMatchingState(Trajectory, SearchContext, InOutMotionMatchingState.CurrentSearchResult, InOutMotionMatchingState.ElapsedPoseSearchTime,
			InOutMotionMatchingState.RootMotionTransformDelta, Context.AnimInstanceProxy->GetAnimInstanceObject(), Context.GetCurrentNodeId(), DeltaTime, bSearch,
			AnimInstance ? FObjectTrace::GetWorldElapsedTime(AnimInstance->GetWorld()) : 0.f);
	}
#endif // UE_POSE_SEARCH_TRACE_ENABLED

#if WITH_EDITORONLY_DATA && ENABLE_ANIM_DEBUG
	const FSearchResult& CurResult = InOutMotionMatchingState.CurrentSearchResult;
	if ((bDebugDrawQuery || bDebugDrawCurResult) && CurResult.Database != nullptr)
	{
		const UPoseSearchDatabase* CurResultDatabase = CurResult.Database.Get();

#if WITH_EDITOR
		if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(CurResultDatabase, ERequestAsyncBuildFlag::ContinueRequest))
#endif // WITH_EDITOR
		{
			if (bDebugDrawCurResult)
			{
				UE::PoseSearch::FDebugDrawParams DrawParams(Context.AnimInstanceProxy, SearchContext.GetWorldBoneTransformAtTime(0.f), CurResultDatabase);
				DrawParams.DrawFeatureVector(CurResult.PoseIdx);
			}

			if (bDebugDrawQuery)
			{
				// @todo: use pose history to get the root bone!
				UE::PoseSearch::FDebugDrawParams DrawParams(Context.AnimInstanceProxy, SearchContext.GetWorldBoneTransformAtTime(0.f), CurResultDatabase, EDebugDrawFlags::DrawQuery);
				DrawParams.DrawFeatureVector(SearchContext.GetOrBuildQuery(CurResultDatabase->Schema).GetValues());
			}
		}
	}
#endif
}

// transforms Trajectory from SkeletalMeshComponent world space to root bone world space, and scale it by TrajectorySpeedMultiplier
FPoseSearchQueryTrajectory UPoseSearchLibrary::ProcessTrajectory(const FPoseSearchQueryTrajectory& Trajectory, const FTransform& RootBoneTransform, float RootBoneDeltaYaw, float YawFromAnimationTrajectoryBlendTime, float TrajectorySpeedMultiplier)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_ProcessTrajectory);

	const float TrajectorySpeedMultiplierInv = FMath::IsNearlyZero(TrajectorySpeedMultiplier) ? 1.f : 1.f / TrajectorySpeedMultiplier;

	FPoseSearchQueryTrajectory TrajectoryRootSpace = Trajectory;

	if (!RootBoneTransform.Equals(FTransform::Identity))
	{
		// @todo: optimize me
		for (FPoseSearchQueryTrajectorySample& Sample : TrajectoryRootSpace.Samples)
		{
			Sample.SetTransform(RootBoneTransform * Sample.GetTransform());
		}
	}

	if (!FMath::IsNearlyEqual(TrajectorySpeedMultiplierInv, 1.f))
	{
		for (FPoseSearchQueryTrajectorySample& Sample : TrajectoryRootSpace.Samples)
		{
			Sample.AccumulatedSeconds *= TrajectorySpeedMultiplierInv;
		}
	}

	if (!FMath::IsNearlyZero(RootBoneDeltaYaw))
	{
		for (FPoseSearchQueryTrajectorySample& Sample : TrajectoryRootSpace.Samples)
		{
			const float BlendParam = YawFromAnimationTrajectoryBlendTime < UE_KINDA_SMALL_NUMBER ? 1 : FMath::Clamp(1.f - (Sample.AccumulatedSeconds - YawFromAnimationTrajectoryBlendTime) / YawFromAnimationTrajectoryBlendTime, 0.f, 1.f);
			const FQuat RootBoneDelta(FRotator(0.f, RootBoneDeltaYaw * BlendParam, 0.f));
			Sample.Facing = RootBoneDelta * Sample.Facing;
		}
	}

	return TrajectoryRootSpace;
}

void UPoseSearchLibrary::MotionMatch(
	UAnimInstance* AnimInstance,
	const UPoseSearchDatabase* Database,
	const FPoseSearchQueryTrajectory Trajectory,
	float TrajectorySpeedMultiplier,
	const FName PoseHistoryName,
	FPoseSearchBlueprintResult& Result,
	const UAnimationAsset* FutureAnimation,
	float FutureAnimationStartTime,
	float TimeToFutureAnimationStart,
	const int32 DebugSessionUniqueIdentifier)
{
#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	class UAnimInstanceProxyProvider : public UAnimInstance
	{
	public:
		static FAnimInstanceProxy* GetAnimInstanceProxy(UAnimInstance* AnimInstance)
		{
			if (AnimInstance)
			{
				return &static_cast<UAnimInstanceProxyProvider*>(AnimInstance)->GetProxyOnAnyThread<FAnimInstanceProxy>();
			}
			return nullptr;
		}
	};
#endif //ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG

	using namespace UE::Anim;
	using namespace UE::PoseSearch;

	Result.SelectedAnimation = nullptr;
	Result.SelectedTime = 0.f;
	Result.bLoop = false;
	Result.bIsMirrored = false;
	Result.BlendParameters = FVector::ZeroVector;
	Result.SelectedDatabase = nullptr;
	Result.SearchCost = MAX_flt;

	FMemMark Mark(FMemStack::Get());
	if (Database && AnimInstance && AnimInstance->CurrentSkeleton)
	{
#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
		FColor HistoryCollectorColor = FColor::Red;
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG

		// @todo: perhaps store the previous frame root bone transform to use in ProcessTrajectory
		const FTransform& RootBoneTransform = AnimInstance->CurrentSkeleton->GetReferenceSkeleton().GetRefBonePose()[RootSchemaBoneIdx];
		const FPoseSearchQueryTrajectory TrajectoryRootSpace = ProcessTrajectory(Trajectory, RootBoneTransform, 0.f, 0.f, TrajectorySpeedMultiplier);

		// ExtendedPoseHistory will hold future poses to match AssetSamplerBase (at FutureAnimationStartTime) TimeToFutureAnimationStart seconds in the future
		FExtendedPoseHistory ExtendedPoseHistory;
		if (IAnimClassInterface* AnimBlueprintClass = IAnimClassInterface::GetFromClass(AnimInstance->GetClass()))
		{
			if (const FAnimSubsystem_Tag* TagSubsystem = AnimBlueprintClass->FindSubsystem<FAnimSubsystem_Tag>())
			{
				if (const FAnimNode_PoseSearchHistoryCollector_Base* PoseHistoryNode = TagSubsystem->FindNodeByTag<FAnimNode_PoseSearchHistoryCollector_Base>(PoseHistoryName, AnimInstance))
				{
					ExtendedPoseHistory.Init(&PoseHistoryNode->GetPoseHistory());
#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG && WITH_EDITORONLY_DATA
					HistoryCollectorColor = PoseHistoryNode->DebugColor.ToFColor(true);
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG && WITH_EDITORONLY_DATA
				}
			}
		}

		if (!ExtendedPoseHistory.IsInitialized())
		{
			if (FutureAnimation)
			{
				UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchLibrary::MotionMatch - Couldn't find pose history with name '%s'. FutureAnimation search will not be performed"), *PoseHistoryName.ToString());
			}
			else
			{
				UE_LOG(LogPoseSearch, Warning, TEXT("UPoseSearchLibrary::MotionMatch - Couldn't find pose history with name '%s'"), *PoseHistoryName.ToString());
			}
		}
		else if (FutureAnimation)
		{
			const FBoneContainer& BoneContainer = AnimInstance->GetRequiredBonesOnAnyThread();
			// @todo... add input BlendParameters to support sampling FutureAnimation blendspaces
			const FAnimationAssetSampler Sampler(FutureAnimation, FVector::ZeroVector);

			if (FutureAnimationStartTime < FiniteDelta)
			{
				UE_LOG(LogPoseSearch, Warning, TEXT("UPoseSearchLibrary::MotionMatch - provided FutureAnimationStartTime (%f) is too small to be able to calculate velocities. Clamping it to minimum value of %f"), FutureAnimationStartTime, FiniteDelta);
				FutureAnimationStartTime = FiniteDelta;
			}

			const float MinTimeToFutureAnimationStart = FiniteDelta + UE_KINDA_SMALL_NUMBER;
			if (TimeToFutureAnimationStart < MinTimeToFutureAnimationStart)
			{
				UE_LOG(LogPoseSearch, Warning, TEXT("UPoseSearchLibrary::MotionMatch - provided TimeToFutureAnimationStart (%f) is too small. Clamping it to minimum value of %f"), TimeToFutureAnimationStart, MinTimeToFutureAnimationStart);
				TimeToFutureAnimationStart = MinTimeToFutureAnimationStart;
			}

			// extracting 2 poses to be able to calculate velocities
			for (int32 i = 0; i < 2; ++i)
			{
				const float ExtractionTime = FutureAnimationStartTime + (i - 1) * FiniteDelta;
				const float FutureAnimationTime = TimeToFutureAnimationStart + (i - 1) * FiniteDelta;

				FCompactPose Pose;
				Pose.SetBoneContainer(&BoneContainer);
				Sampler.ExtractPose(ExtractionTime, Pose);

				FCSPose<FCompactPose> ComponentSpacePose;
				ComponentSpacePose.InitPose(Pose);

				const FPoseSearchQueryTrajectorySample TrajectorySample = TrajectoryRootSpace.GetSampleAtTime(ExtractionTime);
				const FTransform& ComponentTransform = AnimInstance->GetOwningComponent()->GetComponentTransform();
				const FTransform FutureComponentTransform = TrajectorySample.GetTransform() * ComponentTransform;

				ExtendedPoseHistory.AddFuturePose(FutureAnimationTime, ComponentSpacePose, FutureComponentTransform);
			}

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
			if (CVarAnimMotionMatchDrawHistoryEnable.GetValueOnAnyThread())
			{
				if (FAnimInstanceProxy* AnimInstanceProxy = UAnimInstanceProxyProvider::GetAnimInstanceProxy(AnimInstance))
				{
					ExtendedPoseHistory.DebugDraw(*AnimInstanceProxy, HistoryCollectorColor, &TrajectoryRootSpace);
				}
			}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
		}

		FSearchContext SearchContext(AnimInstance, ExtendedPoseHistory.IsInitialized() ? &ExtendedPoseHistory : nullptr, TConstArrayView<const UAnimationAsset*>(), &TrajectoryRootSpace, TimeToFutureAnimationStart);

		FSearchResult SearchResult = Database->Search(SearchContext);
		if (SearchResult.IsValid())
		{
			const FSearchIndexAsset* SearchIndexAsset = SearchResult.GetSearchIndexAsset();
			if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAsset = SearchResult.Database->GetAnimationAssetBase(*SearchIndexAsset))
			{
				Result.SelectedAnimation = DatabaseAsset->GetAnimationAsset();
				Result.SelectedTime = SearchResult.AssetTime;
				Result.bLoop = SearchIndexAsset->IsLooping();
				Result.bIsMirrored = SearchIndexAsset->IsMirrored();
				Result.BlendParameters = SearchIndexAsset->GetBlendParameters();
				Result.SelectedDatabase = Database;
				Result.SearchCost = SearchResult.PoseCost.GetTotalCost();
			}
		}

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
		if (SearchResult.IsValid())
		{
			FAnimInstanceProxy* AnimInstanceProxy = UAnimInstanceProxyProvider::GetAnimInstanceProxy(AnimInstance);
			if (CVarAnimMotionMatchDrawMatchEnable.GetValueOnAnyThread())
			{
				FDebugDrawParams DrawParams(AnimInstanceProxy, SearchContext.GetWorldBoneTransformAtTime(0.f), SearchResult.Database.Get());
				DrawParams.DrawFeatureVector(SearchResult.PoseIdx);
			}

			if (CVarAnimMotionMatchDrawQueryEnable.GetValueOnAnyThread())
			{
				FDebugDrawParams DrawParams(AnimInstanceProxy, SearchContext.GetWorldBoneTransformAtTime(0.f), SearchResult.Database.Get(), EDebugDrawFlags::DrawQuery);
				DrawParams.DrawFeatureVector(SearchContext.GetOrBuildQuery(SearchResult.Database->Schema).GetValues());
			}
		}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG

#if UE_POSE_SEARCH_TRACE_ENABLED
		
		TraceMotionMatchingState(Trajectory, SearchContext, SearchResult, 0.f, FTransform::Identity, AnimInstance, DebugSessionUniqueIdentifier,
			AnimInstance->GetDeltaSeconds(), true, FObjectTrace::GetWorldElapsedTime(AnimInstance->GetWorld()));
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}
}

UE::PoseSearch::FSearchResult UPoseSearchLibrary::MotionMatch(const FAnimationBaseContext& Context, TConstArrayView<UAnimationAsset*> AnimationAssets)
{
	using namespace UE::PoseSearch;

	FSearchResult SearchResult;

	// budgeting some stack allocations for simple use cases. bigger requests of AnimationAssets contining 
	// UAnimNotifyState_PoseSearchBranchIn referencing multiple datbases will default to slower heap allocations
	enum { MAX_STACK_ALLOCATED_ANIMATIONS = 16 };
	enum { MAX_STACK_ALLOCATED_SETS = 2 };
	typedef	TArray<const UAnimationAsset*, TInlineAllocator<MAX_STACK_ALLOCATED_ANIMATIONS>> TDbAnims;
	typedef TMap<const UPoseSearchDatabase*, TDbAnims, TInlineSetAllocator<MAX_STACK_ALLOCATED_SETS>> FPerDbAnimMap;
	typedef TPair<const UPoseSearchDatabase*, TDbAnims> FPerDbAnimPair;
	FPerDbAnimMap PerDbAnimMap;
	
	// colecting all the UAnimSequenceBase to consider for each database
	for (const UAnimationAsset* AnimationAsset : AnimationAssets)
	{
		if (const UAnimSequenceBase* SequenceBase = Cast<const UAnimSequenceBase>(AnimationAsset))
		{
			for (const FAnimNotifyEvent& NotifyEvent : SequenceBase->Notifies)
			{
				if (const UAnimNotifyState_PoseSearchBranchIn* PoseSearchBranchIn = Cast<UAnimNotifyState_PoseSearchBranchIn>(NotifyEvent.NotifyStateClass))
				{
					if (PoseSearchBranchIn->Database)
					{
						PerDbAnimMap.FindOrAdd(PoseSearchBranchIn->Database).AddUnique(SequenceBase);
					}
					else
					{
						UE_LOG(LogPoseSearch, Error, TEXT("improperly setup UAnimNotifyState_PoseSearchBranchIn with null Database in %s"), *SequenceBase->GetName());
					}
				}
			}
		}
	}

	if (!PerDbAnimMap.IsEmpty())
	{
		const IPoseHistory* History = nullptr;
		if (IPoseHistoryProvider* PoseHistoryProvider = Context.GetMessage<IPoseHistoryProvider>())
		{
			History = &PoseHistoryProvider->GetPoseHistory();
		}

		const UAnimInstance* AnimInstance = Cast<const UAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject());
		check(AnimInstance);

		FMemMark Mark(FMemStack::Get());
		FSearchContext SearchContext(AnimInstance, History);

		for (const FPerDbAnimPair& PerDbAnimPair : PerDbAnimMap)
		{
			check(PerDbAnimPair.Key);
			
			SearchContext.SetAnimationsToConsider(PerDbAnimPair.Value);

			const FSearchResult NewSearchResult = PerDbAnimPair.Key->Search(SearchContext);
			if (NewSearchResult.PoseCost.GetTotalCost() < SearchResult.PoseCost.GetTotalCost())
			{
				SearchResult = NewSearchResult;
				SearchContext.UpdateCurrentBestCost(SearchResult.PoseCost);
			}
		}

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
		if (SearchResult.IsValid())
		{
			if (CVarAnimMotionMatchDrawMatchEnable.GetValueOnAnyThread())
			{
				FDebugDrawParams DrawParams(Context.AnimInstanceProxy, SearchContext.GetWorldBoneTransformAtTime(0.f), SearchResult.Database.Get());
				DrawParams.DrawFeatureVector(SearchResult.PoseIdx);
			}

			if (CVarAnimMotionMatchDrawQueryEnable.GetValueOnAnyThread())
			{
				FDebugDrawParams DrawParams(Context.AnimInstanceProxy, SearchContext.GetWorldBoneTransformAtTime(0.f), SearchResult.Database.Get(), EDebugDrawFlags::DrawQuery);
				DrawParams.DrawFeatureVector(SearchContext.GetOrBuildQuery(SearchResult.Database->Schema).GetValues());
			}
		}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG

#if UE_POSE_SEARCH_TRACE_ENABLED
		const float SearchBestCost = SearchResult.PoseCost.GetTotalCost();
		const float SearchBruteForceCost = SearchResult.BruteForcePoseCost.GetTotalCost();
		TraceMotionMatchingState(FPoseSearchQueryTrajectory(), SearchContext, SearchResult, 0.f, FTransform::Identity, AnimInstance, Context.GetCurrentNodeId(),
			AnimInstance->GetDeltaSeconds(), true, FObjectTrace::GetWorldElapsedTime(AnimInstance->GetWorld()));
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}

	return SearchResult;
}

#undef LOCTEXT_NAMESPACE
