// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/Trace/PoseSearchTraceLogger.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "Serialization/MemoryWriter.h"
#include "Trace/Trace.inl"
#include "TraceFilter.h"

UE_TRACE_CHANNEL_DEFINE(PoseSearchChannel);

UE_TRACE_EVENT_BEGIN(PoseSearch, MotionMatchingState)
	UE_TRACE_EVENT_FIELD(uint8[], Data)
UE_TRACE_EVENT_END()

namespace UE::PoseSearch
{

const FName FTraceLogger::Name("PoseSearch");
const FName FTraceMotionMatchingState::Name("MotionMatchingState");

bool IsTracing(const FAnimationBaseContext& InContext)
{
#if UE_POSE_SEARCH_TRACE_ENABLED
	const bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(PoseSearchChannel);
	if (!bChannelEnabled)
	{
		return false;
	}

	if (InContext.GetCurrentNodeId() == INDEX_NONE)
	{
		return false;
	}

	check(InContext.AnimInstanceProxy);
	return !CANNOT_TRACE_OBJECT(InContext.AnimInstanceProxy->GetSkelMeshComponent());
#else // UE_POSE_SEARCH_TRACE_ENABLED
	return false;
#endif // UE_POSE_SEARCH_TRACE_ENABLED
}

FArchive& operator<<(FArchive& Ar, FTraceMessage& State)
{
	Ar << State.Cycle;
	Ar << State.AnimInstanceId;
	Ar << State.SkeletalMeshComponentId;
	Ar << State.NodeId;
	Ar << State.FrameCounter;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FTraceMotionMatchingStatePoseEntry& Entry)
{
	Ar << Entry.DbPoseIdx;
	FPoseSearchCost::StaticStruct()->SerializeItem(Ar, &Entry.Cost, nullptr);
	Ar << Entry.PoseCandidateFlags;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FTraceMotionMatchingStateDatabaseEntry& Entry)
{
	Ar << Entry.DatabaseId;
	Ar << Entry.QueryVector;
	Ar << Entry.PoseEntries;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FTraceMotionMatchingState& State)
{
	Ar << State.ElapsedPoseSearchTime;
	Ar << State.AssetPlayerTime;
	Ar << State.DeltaTime;
	Ar << State.SimLinearVelocity;
	Ar << State.SimAngularVelocity;
	Ar << State.AnimLinearVelocity;
	Ar << State.AnimAngularVelocity;
	Ar << State.RecordingTime;
	Ar << State.SearchBestCost;
	Ar << State.SearchBruteForceCost;
	Ar << State.SearchBestPosePos;
	Ar << State.DatabaseEntries;
	Ar << State.Trajectory;
	Ar << State.CurrentDbEntryIdx;
	Ar << State.CurrentPoseEntryIdx;
	return Ar;
}

void FTraceMotionMatchingState::Output(const UObject* AnimInstance, int32 NodeId)
{
#if OBJECT_TRACE_ENABLED
	TArray<uint8> ArchiveData;
	FMemoryWriter Archive(ArchiveData);

	TRACE_OBJECT(AnimInstance);
	UObject* SkeletalMeshComponent = AnimInstance->GetOuter();

	FTraceMessage TraceMessage;
	TraceMessage.Cycle = FPlatformTime::Cycles64();
	TraceMessage.AnimInstanceId = FObjectTrace::GetObjectId(AnimInstance);
	TraceMessage.SkeletalMeshComponentId = FObjectTrace::GetObjectId(SkeletalMeshComponent);
	TraceMessage.NodeId = NodeId;
	TraceMessage.FrameCounter = FObjectTrace::GetObjectWorldTickCounter(AnimInstance);

	Archive << TraceMessage;
	Archive << *this;

	UE_TRACE_LOG(PoseSearch, MotionMatchingState, PoseSearchChannel) << MotionMatchingState.Data(ArchiveData.GetData(), ArchiveData.Num());
#endif
}

const UPoseSearchDatabase* FTraceMotionMatchingState::GetCurrentDatabase() const
{
	const UPoseSearchDatabase* Database = nullptr;
	if (DatabaseEntries.IsValidIndex(CurrentDbEntryIdx))
	{
		Database = GetObjectFromId<UPoseSearchDatabase>(DatabaseEntries[CurrentDbEntryIdx].DatabaseId);
	}
	return Database;
}

int32 FTraceMotionMatchingState::GetCurrentDatabasePoseIndex() const
{
	if (const FTraceMotionMatchingStatePoseEntry* PoseEntry = GetCurrentPoseEntry())
	{
		return PoseEntry->DbPoseIdx;
	}
	return INDEX_NONE;
}

const FTraceMotionMatchingStatePoseEntry* FTraceMotionMatchingState::GetCurrentPoseEntry() const
{
	if (DatabaseEntries.IsValidIndex(CurrentDbEntryIdx))
	{
		const FTraceMotionMatchingStateDatabaseEntry& DbEntry = DatabaseEntries[CurrentDbEntryIdx];
		if (DbEntry.PoseEntries.IsValidIndex(CurrentPoseEntryIdx))
		{
			return &DbEntry.PoseEntries[CurrentPoseEntryIdx];
		}
	}
	return nullptr;
}

} // namespace UE::PoseSearch