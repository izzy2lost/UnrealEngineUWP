// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "PoseSearch/PoseSearchAssetIndexer.h"
#include "AnimationRuntime.h"
#include "Animation/MirrorDataTable.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "PoseSearch/PoseSearchAssetSampler.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearch/PoseSearchDatabase.h"

namespace UE::PoseSearch
{

#if ENABLE_ANIM_DEBUG
static TAutoConsoleVariable<bool> CVarMotionMatchTestDisableIndexerCaching(TEXT("a.MotionMatch.TestDisableIndexerCaching"), false, TEXT("Disable Motion Matching Indexer Caching"));
static TAutoConsoleVariable<int32> CVarMotionMatchTestExtractPoseDeterminismNumIterations(TEXT("a.MotionMatch.TestExtractPoseDeterminismNumIterations"), 0, TEXT("Test Motion Matching ExtractPose Determinism via this NumIterations retries"));
#endif // ENABLE_ANIM_DEBUG

//////////////////////////////////////////////////////////////////////////
// FSamplingParam helpers
struct FSamplingParam
{
	float WrappedParam = 0.0f;
	int32 NumCycles = 0;

	// If the animation can't loop, WrappedParam contains the clamped value and whatever is left is stored here
	float Extrapolation = 0.0f;
};

static FSamplingParam WrapOrClampSamplingParam(bool bCanWrap, float SamplingParamExtent, float SamplingParam)
{
	// This is a helper function used by both time and distance sampling. A schema may specify time or distance
	// offsets that are multiple cycles of a clip away from the current pose being sampled.
	// And that time or distance offset may before the beginning of the clip (SamplingParam < 0.0f)
	// or after the end of the clip (SamplingParam > SamplingParamExtent). So this function
	// helps determine how many cycles need to be applied and what the wrapped value should be, clamping
	// if necessary.

	FSamplingParam Result;

	Result.WrappedParam = SamplingParam;

	const bool bIsSamplingParamExtentKindaSmall = SamplingParamExtent <= UE_KINDA_SMALL_NUMBER;
	if (!bIsSamplingParamExtentKindaSmall && bCanWrap)
	{
		if (SamplingParam < 0.0f)
		{
			while (Result.WrappedParam < 0.0f)
			{
				Result.WrappedParam += SamplingParamExtent;
				++Result.NumCycles;
			}
		}

		else
		{
			while (Result.WrappedParam > SamplingParamExtent)
			{
				Result.WrappedParam -= SamplingParamExtent;
				++Result.NumCycles;
			}
		}
	}

	const float ParamClamped = FMath::Clamp(Result.WrappedParam, 0.0f, SamplingParamExtent);
	if (ParamClamped != Result.WrappedParam)
	{
		check(bIsSamplingParamExtentKindaSmall || !bCanWrap);
		Result.Extrapolation = Result.WrappedParam - ParamClamped;
		Result.WrappedParam = ParamClamped;
	}

	return Result;
}

//////////////////////////////////////////////////////////////////////////
// FAssetSamplingContext
FAssetSamplingContext::FAssetSamplingContext(const UPoseSearchDatabase& Database, const FBoneContainer& BoneContainer)
: FMirrorDataCache(Database.Schema ? Database.Schema->MirrorDataTable : nullptr, BoneContainer)
{
	check(Database.Schema);
	BaseCostBias = Database.BaseCostBias;
	LoopingCostBias = Database.LoopingCostBias;
}

//////////////////////////////////////////////////////////////////////////
// FAssetIndexer
FAssetIndexer::FAssetIndexer(const FBoneContainer& InBoneContainer, const FSearchIndexAsset& InSearchIndexAsset, const FAssetSamplingContext& InSamplingContext,
	const UPoseSearchSchema& InSchema, const FAnimationAssetSampler& InAssetSampler, const FFloatInterval& InExtrapolationTimeInterval)
: BoneContainer(InBoneContainer)
, CachedEntries()
, SearchIndexAsset(InSearchIndexAsset)
, SamplingContext(InSamplingContext)
, Schema(InSchema)
, AssetSampler(InAssetSampler)
, ExtrapolationTimeInterval(InExtrapolationTimeInterval)
{
}

void FAssetIndexer::AssignWorkingData(int32 InStartPoseIdx, TArrayView<float> InOutFeatureVectorTable, TArrayView<FPoseMetadata> InOutPoseMetadata)
{
	const int32 NumIndexedPoses = GetNumIndexedPoses();

	StartPoseIdx = InStartPoseIdx;
	FeatureVectorTable = MakeArrayView(InOutFeatureVectorTable.GetData() + Schema.SchemaCardinality * StartPoseIdx, Schema.SchemaCardinality * NumIndexedPoses);
	PoseMetadata = MakeArrayView(InOutPoseMetadata.GetData() + StartPoseIdx, NumIndexedPoses);
}

void FAssetIndexer::Process(int32 AssetIdx)
{
	check(Schema.IsValid());

	bProcessFailed = false;

	// Generate pose metadata
	const float SequenceLength = AssetSampler.GetPlayLength();
	for (int32 SampleIdx = GetBeginSampleIdx(); SampleIdx != GetEndSampleIdx(); ++SampleIdx)
	{
		const float SampleTime = FMath::Min(CalculateSampleTime(SampleIdx), SequenceLength);
		float CostAddend = SamplingContext.BaseCostBias;
		bool bBlockTransition = false;

		AssetSampler.ExtractPoseSearchNotifyStates(SampleTime, [&bBlockTransition, &CostAddend](const UAnimNotifyState_PoseSearchBase* PoseSearchNotify)
			{
				if (PoseSearchNotify->GetClass()->IsChildOf<UAnimNotifyState_PoseSearchBlockTransition>())
				{
					bBlockTransition = true;
				}
				else if (const UAnimNotifyState_PoseSearchModifyCost* ModifyCostNotify = Cast<const UAnimNotifyState_PoseSearchModifyCost>(PoseSearchNotify))
				{
					CostAddend = ModifyCostNotify->CostAddend;
				}
				return true;
			});

		if (AssetSampler.IsLoopable())
		{
			CostAddend += SamplingContext.LoopingCostBias;
		}

		const int32 ValueOffset = (StartPoseIdx + GetVectorIdx(SampleIdx)) * Schema.SchemaCardinality;
		check(ValueOffset >= 0 && AssetIdx >= 0);
		PoseMetadata[GetVectorIdx(SampleIdx)] = FPoseMetadata(ValueOffset, AssetIdx, bBlockTransition, CostAddend);
	}

	// Generate pose features data
	if (Schema.SchemaCardinality > 0)
	{
		for (const TObjectPtr<UPoseSearchFeatureChannel>& ChannelPtr : Schema.GetChannels())
		{
			if (!ChannelPtr->IndexAsset(*this))
			{
				bProcessFailed = true;
				break;
			}
		}
	}

	// Computing stats
	if (!bProcessFailed)
	{
		ComputeStats();
	}
}

void FAssetIndexer::ComputeStats()
{
	Stats = FStats();

	for (int32 SampleIdx = GetBeginSampleIdx(); SampleIdx != GetEndSampleIdx(); ++SampleIdx)
	{
		const float SampleTime = FMath::Min(CalculateSampleTime(SampleIdx), AssetSampler.GetPlayLength());

		bool AnyClamped = false;
		const FTransform TrajTransformsPast = GetTransform(SampleTime - FiniteDelta, AnyClamped);
		if (!AnyClamped)
		{
			const FTransform TrajTransformsPresent = GetTransform(SampleTime, AnyClamped);
			if (!AnyClamped)
			{
				const FTransform TrajTransformsFuture = GetTransform(SampleTime + FiniteDelta, AnyClamped);
				if (!AnyClamped)
				{
					// if any transform is clamped we just skip the sample entirely
					const FVector LinearVelocityPresent = (TrajTransformsPresent.GetTranslation() - TrajTransformsPast.GetTranslation()) / FiniteDelta;
					const FVector LinearVelocityFuture = (TrajTransformsFuture.GetTranslation() - TrajTransformsPresent.GetTranslation()) / FiniteDelta;
					const FVector LinearAcceleration = (LinearVelocityFuture - LinearVelocityPresent) / FiniteDelta;

					const float Speed = LinearVelocityPresent.Length();
					const float Acceleration = LinearAcceleration.Length();

					Stats.AccumulatedSpeed += Speed;
					Stats.MaxSpeed = FMath::Max(Stats.MaxSpeed, Speed);

					Stats.AccumulatedAcceleration += Acceleration;
					Stats.MaxAcceleration = FMath::Max(Stats.MaxAcceleration, Acceleration);

					++Stats.NumAccumulatedSamples;
				}
			}
		}
	}
}

FAssetIndexer::FSampleInfo FAssetIndexer::GetSampleInfo(float SampleTime) const
{
	FSampleInfo Sample;

	const float PlayLength = AssetSampler.GetPlayLength();
	const bool bCanWrap = AssetSampler.IsLoopable();

	float MainRelativeTime = SampleTime;
	if (SampleTime < 0.0f && bCanWrap)
	{
		// In this case we're sampling a loop backwards, so MainRelativeTime must adjust so the number of cycles is
		// counted correctly.
		MainRelativeTime += PlayLength;
	}

	const FSamplingParam SamplingParam = WrapOrClampSamplingParam(bCanWrap, PlayLength, MainRelativeTime);

	if (FMath::Abs(SamplingParam.Extrapolation) > SMALL_NUMBER)
	{
		Sample.bClamped = true;
		Sample.ClipTime = SamplingParam.WrappedParam + SamplingParam.Extrapolation;
		Sample.RootTransform = AssetSampler.ExtractRootTransform(Sample.ClipTime);
	}
	else
	{
		Sample.ClipTime = SamplingParam.WrappedParam;
		Sample.RootTransform = FTransform::Identity;

		// Find the remaining motion deltas after wrapping
		FTransform RootMotionRemainder = AssetSampler.ExtractRootTransform(Sample.ClipTime);

		const bool bNegativeSampleTime = SampleTime < 0.f;
		if (SamplingParam.NumCycles > 0 || bNegativeSampleTime)
		{
			const FTransform RootMotionLast = AssetSampler.GetTotalRootTransform();

			// Determine how to accumulate motion for every cycle of the anim. If the sample
			// had to be clamped, this motion will end up not getting applied below.
			// Also invert the accumulation direction if the requested sample was wrapped backwards.
			FTransform RootMotionPerCycle = RootMotionLast;

			if (bNegativeSampleTime)
			{
				RootMotionPerCycle = RootMotionPerCycle.Inverse();
			}
			
			// Invert motion deltas if we wrapped backwards
			if (bNegativeSampleTime)
			{
				RootMotionRemainder.SetToRelativeTransform(RootMotionLast);
			}

			// Note if the sample was clamped, no motion will be applied here because NumCycles will be zero
			int32 CyclesRemaining = SamplingParam.NumCycles;
			while (CyclesRemaining--)
			{
				Sample.RootTransform = RootMotionPerCycle * Sample.RootTransform;
			}
		}

		Sample.RootTransform = RootMotionRemainder * Sample.RootTransform;
	}

	return Sample;
}

FTransform FAssetIndexer::MirrorTransform(const FTransform& Transform) const
{
	return SearchIndexAsset.IsMirrored() ? SamplingContext.MirrorTransform(Transform) : Transform;
}

FAssetIndexer::CachedEntry& FAssetIndexer::GetEntry(float SampleTime)
{
	using namespace UE::Anim;
	
	bool bDisableCaching = false;
#if ENABLE_ANIM_DEBUG
	bDisableCaching = CVarMotionMatchTestDisableIndexerCaching.GetValueOnAnyThread();
#endif // ENABLE_ANIM_DEBUG

	SampleTime = FMath::Clamp(SampleTime, ExtrapolationTimeInterval.Min, ExtrapolationTimeInterval.Max);

	CachedEntry* Entry = bDisableCaching ? nullptr : CachedEntries.Find(SampleTime);
	if (!Entry)
	{
		Entry = &CachedEntries.Add(SampleTime);
		Entry->SampleTime = SampleTime;

		if (!BoneContainer.IsValid())
		{
			UE_LOG(LogPoseSearch,
				Warning,
				TEXT("Invalid BoneContainer encountered in FAssetIndexer::GetEntry. Asset: %s. Schema: %s. BoneContainerAsset: %s. NumBoneIndices: %d"),
				*GetNameSafe(AssetSampler.GetAsset()),
				*GetNameSafe(&Schema),
				*GetNameSafe(BoneContainer.GetAsset()),
				BoneContainer.GetCompactPoseNumBones());
		}

		const FAssetIndexer::FSampleInfo Sample = GetSampleInfo(SampleTime);
		float CurrentTime = Sample.ClipTime;

		const bool bLoopable = AssetSampler.IsLoopable();
		const float PlayLength = AssetSampler.GetPlayLength();

		if (!bLoopable)
		{
			CurrentTime = FMath::Clamp(CurrentTime, 0.f, PlayLength);
		}

		FMemMark Mark(FMemStack::Get());
		FCompactPose Pose;
		Pose.SetBoneContainer(&BoneContainer);
		AssetSampler.ExtractPose(CurrentTime, Pose);

#if ENABLE_ANIM_DEBUG
		const int32 NumIterations = CVarMotionMatchTestExtractPoseDeterminismNumIterations.GetValueOnAnyThread();
		for (int32 IterationIndex = 0; IterationIndex < NumIterations; ++IterationIndex)
		{
			FCompactPose TestPose;
			TestPose.SetBoneContainer(&BoneContainer);
			AssetSampler.ExtractPose(CurrentTime, TestPose);

			const TConstArrayView<FTransform> Bones = Pose.GetBones();
			const TConstArrayView<FTransform> TestBones = TestPose.GetBones();
			if (Bones.Num() != TestBones.Num())
			{
				UE_LOG(LogPoseSearch, Warning, TEXT("FAssetIndexer::GetEntry - ExtractPose is not deterministic"));
			}
			else
			{
				for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
				{
					if (FMemory::Memcmp(&Bones[BoneIndex], &TestBones[BoneIndex], sizeof(FTransform)) != 0)
					{
						UE_LOG(LogPoseSearch, Warning, TEXT("FAssetIndexer::GetEntry - ExtractPose is not deterministic"));
					}
				}
			}
		}
#endif // ENABLE_ANIM_DEBUG

		if (SearchIndexAsset.IsMirrored())
		{
			SamplingContext.MirrorPose(Pose);
		}

		FCSPose<FCompactPose> StackComponentSpacePose;
		StackComponentSpacePose.InitPose(MoveTemp(Pose));
		Entry->ComponentSpacePose.CopyPose(StackComponentSpacePose);

		Entry->RootTransform = Sample.RootTransform;
		Entry->bClamped = Sample.bClamped;
	}

	return *Entry;
}

// returns the transform in component space for the bone indexed by Schema->BoneReferences[SchemaBoneIdx] at SampleTime seconds
FTransform FAssetIndexer::GetComponentSpaceTransform(float SampleTime, bool& bClamped, int8 SchemaBoneIdx)
{
	using namespace UE::PoseSearch;

	CachedEntry& Entry = GetEntry(SampleTime);
	bClamped = Entry.bClamped;

	return CalculateComponentSpaceTransform(Entry, SchemaBoneIdx);
}

// returns the transform in animation space for the bone indexed by Schema->BoneReferences[SchemaBoneIdx] at SampleTime seconds
FTransform FAssetIndexer::GetTransform(float SampleTime, bool& bClamped, int8 SchemaBoneIdx)
{
	using namespace UE::PoseSearch;

	CachedEntry& Entry = GetEntry(SampleTime);
	bClamped = Entry.bClamped;

	return CalculateComponentSpaceTransform(Entry, SchemaBoneIdx) * MirrorTransform(Entry.RootTransform);
}

// returns the transform in animation space for the BoneReference at SampleTime seconds
FTransform FAssetIndexer::GetTransform(float SampleTime, bool& bClamped, const FBoneReference& BoneReference)
{
	CachedEntry& Entry = GetEntry(SampleTime);
	bClamped = Entry.bClamped;

	return CalculateComponentSpaceTransform(Entry, BoneReference) * MirrorTransform(Entry.RootTransform);
}

FTransform FAssetIndexer::CalculateComponentSpaceTransform(FAssetIndexer::CachedEntry& Entry, int8 SchemaBoneIdx)
{
	return CalculateComponentSpaceTransform(Entry, Schema.BoneReferences[SchemaBoneIdx]);
}

FTransform FAssetIndexer::CalculateComponentSpaceTransform(FAssetIndexer::CachedEntry& Entry, const FBoneReference& BoneReference)
{
	const FCompactPoseBoneIndex CompactBoneIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(BoneReference.BoneIndex));
	return Entry.ComponentSpacePose.GetComponentSpaceTransform(CompactBoneIndex);
}

float FAssetIndexer::CalculateSampleTime(int32 SampleIdx) const
{
	return SampleIdx / float(Schema.SampleRate);
}

bool FAssetIndexer::GetSampleRotation(FQuat& OutSampleRotation, float SampleTimeOffset, float OriginTimeOffset, int32 SampleIdx, int8 SchemaSampleBoneIdx, int8 SchemaOriginBoneIdx, EPermutationTimeType PermutationTimeType, int32 SamplingAttributeId)
{
	using namespace UE::PoseSearch;

	float PermutationSampleTimeOffset = 0.f;
	float PermutationOriginTimeOffset = 0.f;
	UPoseSearchFeatureChannel::GetPermutationTimeOffsets(PermutationTimeType, CalculatePermutationTimeOffset(), PermutationSampleTimeOffset, PermutationOriginTimeOffset);

	const float Time = CalculateSampleTime(SampleIdx);
	const float SampleTime = Time + SampleTimeOffset + PermutationSampleTimeOffset;
	const float OriginTime = Time + OriginTimeOffset + PermutationOriginTimeOffset;

	// @todo: add support for SchemaSampleBoneIdx
	if (SchemaOriginBoneIdx != RootSchemaBoneIdx)
	{
		UE_LOG(LogPoseSearch,
			Error,
			TEXT("FAssetIndexer::GetSampleRotation: support for non root origin bones not implemented (bone: '%s', schema: '%s'"), 
			*Schema.BoneReferences[SchemaOriginBoneIdx].BoneName.ToString(),
			*GetNameSafe(&Schema));
	}

	bool bUnused;
	if (SamplingAttributeId >= 0)
	{
		const TConstArrayView<FAnimNotifyEvent> AnimNotifyEvents = AssetSampler.GetAllAnimNotifyEvents();
		if (!AnimNotifyEvents.IsEmpty())
		{
			for (const FAnimNotifyEvent& AnimNotifyEvent : AnimNotifyEvents)
			{
				if (const UAnimNotifyState_PoseSearchSamplingAttribute* SamplingAttribute = Cast<UAnimNotifyState_PoseSearchSamplingAttribute>(AnimNotifyEvent.NotifyStateClass))
				{
					if (SamplingAttribute->SamplingAttributeId == SamplingAttributeId)
					{
						if (SamplingAttribute->Bone.BoneName != NAME_None)
						{
							// @todo perhaps cache the initialized bone to speed up this method
							FBoneReference TempBoneReference = SamplingAttribute->Bone;
							TempBoneReference.Initialize(BoneContainer.GetSkeletonAsset());
							if (TempBoneReference.HasValidSetup())
							{
								const float SamplingAttributeTime = AnimNotifyEvent.GetTime();
								const FTransform RootBoneTransform = GetTransform(OriginTime, bUnused, RootSchemaBoneIdx);
								const FTransform SamplingAttributeBoneTransform = GetTransform(SamplingAttributeTime, bUnused, TempBoneReference);
								OutSampleRotation = RootBoneTransform.InverseTransformRotation(SamplingAttributeBoneTransform.GetRotation());
								return true;
							}

							UE_LOG(LogPoseSearch, Error, TEXT("FAssetIndexer::GetSampleRotation: required UAnimNotifyState_PoseSearchSamplingAttribute in '%s' has an invalid Bone"), *GetNameSafe(AssetSampler.GetAsset()));
							OutSampleRotation = FQuat::Identity;
							return false;
						}
						
						const FTransform RootBoneTransform = GetTransform(OriginTime, bUnused, RootSchemaBoneIdx);
						OutSampleRotation = RootBoneTransform.InverseTransformRotation(SamplingAttribute->Rotation);
						return true;
					}
				}
			}
		}

		UE_LOG(LogPoseSearch, Error, TEXT("FAssetIndexer::GetSamplePositionInternal: required UAnimNotifyState_PoseSearchSamplingAttribute not found in '%s'"), *GetNameSafe(AssetSampler.GetAsset()));
		OutSampleRotation = FQuat::Identity;
		return false;
	}

	const FTransform RootBoneTransform = GetTransform(OriginTime, bUnused, RootSchemaBoneIdx);
	const FTransform SampleBoneTransform = GetTransform(SampleTime, bUnused, SchemaSampleBoneIdx);
	OutSampleRotation = RootBoneTransform.InverseTransformRotation(SampleBoneTransform.GetRotation());
	return true;
}

bool FAssetIndexer::GetSamplePosition(FVector& OutSamplePosition, float SampleTimeOffset, float OriginTimeOffset, int32 SampleIdx, int8 SchemaSampleBoneIdx, int8 SchemaOriginBoneIdx, EPermutationTimeType PermutationTimeType, int32 SamplingAttributeId)
{
	float PermutationSampleTimeOffset = 0.f;
	float PermutationOriginTimeOffset = 0.f;
	UPoseSearchFeatureChannel::GetPermutationTimeOffsets(PermutationTimeType, CalculatePermutationTimeOffset(), PermutationSampleTimeOffset, PermutationOriginTimeOffset);

	const float Time = CalculateSampleTime(SampleIdx);
	const float SampleTime = Time + SampleTimeOffset + PermutationSampleTimeOffset;
	const float OriginTime = Time + OriginTimeOffset + PermutationOriginTimeOffset;
	
	bool bUnused;
	return GetSamplePositionInternal(OutSamplePosition, SampleTime, OriginTime, bUnused, SchemaSampleBoneIdx, SchemaOriginBoneIdx, SamplingAttributeId);
}

bool FAssetIndexer::GetSamplePositionInternal(FVector& OutSamplePosition, float SampleTime, float OriginTime, bool& bClamped, int8 SchemaSampleBoneIdx, int8 SchemaOriginBoneIdx, int32 SamplingAttributeId)
{
	using namespace UE::PoseSearch;

	bool bUnused;
	if (SamplingAttributeId >= 0)
	{
		const TConstArrayView<FAnimNotifyEvent> AnimNotifyEvents = AssetSampler.GetAllAnimNotifyEvents();
		if (!AnimNotifyEvents.IsEmpty())
		{
			for (const FAnimNotifyEvent& AnimNotifyEvent : AnimNotifyEvents)
			{
				if (const UAnimNotifyState_PoseSearchSamplingAttribute* SamplingAttribute = Cast<UAnimNotifyState_PoseSearchSamplingAttribute>(AnimNotifyEvent.NotifyStateClass))
				{
					if (SamplingAttribute->SamplingAttributeId == SamplingAttributeId)
					{
						if (SamplingAttribute->Bone.BoneName != NAME_None)
						{
							// @todo perhaps cache the initialized bone to speed up this method
							FBoneReference TempBoneReference = SamplingAttribute->Bone;
							TempBoneReference.Initialize(BoneContainer.GetSkeletonAsset());
							if (TempBoneReference.HasValidSetup())
							{
								const float SamplingAttributeTime = AnimNotifyEvent.GetTime();
								const FTransform RootBoneTransform = GetTransform(OriginTime, bUnused, RootSchemaBoneIdx);
								const FTransform SamplingAttributeBoneTransform = GetTransform(SamplingAttributeTime, bClamped, TempBoneReference);
								if (SchemaOriginBoneIdx == RootSchemaBoneIdx)
								{
									OutSamplePosition = RootBoneTransform.InverseTransformPosition(SamplingAttributeBoneTransform.GetTranslation());
								}
								else
								{
									bool bOriginClamped;
									const FTransform OriginBoneTransform = GetTransform(OriginTime, bOriginClamped, SchemaOriginBoneIdx);
									bClamped |= bOriginClamped;
									const FVector DeltaBoneTranslation = SamplingAttributeBoneTransform.GetTranslation() - OriginBoneTransform.GetTranslation();
									OutSamplePosition = RootBoneTransform.InverseTransformVector(DeltaBoneTranslation);
								}
								return true;
							}
							
							UE_LOG(LogPoseSearch, Error, TEXT("FAssetIndexer::GetSamplePositionInternal: required UAnimNotifyState_PoseSearchSamplingAttribute in '%s' has an invalid Bone"), *GetNameSafe(AssetSampler.GetAsset()));
							OutSamplePosition = FVector::ZeroVector;
							return false;
						}
						
						const FTransform RootBoneTransform = GetTransform(OriginTime, bUnused, RootSchemaBoneIdx);
						OutSamplePosition = RootBoneTransform.InverseTransformPosition(SamplingAttribute->Position);
						return true;
					}
				}
			}
		}

		UE_LOG(LogPoseSearch, Error, TEXT("FAssetIndexer::GetSamplePositionInternal: required UAnimNotifyState_PoseSearchSamplingAttribute not found in '%s'"), *GetNameSafe(AssetSampler.GetAsset()));
		OutSamplePosition = FVector::ZeroVector;
		return false;
	}

	const FTransform RootBoneTransform = GetTransform(OriginTime, bUnused, RootSchemaBoneIdx);
	const FTransform SampleBoneTransform = GetTransform(SampleTime, bClamped, SchemaSampleBoneIdx);
	if (SchemaOriginBoneIdx == RootSchemaBoneIdx)
	{
		OutSamplePosition = RootBoneTransform.InverseTransformPosition(SampleBoneTransform.GetTranslation());
		return true;
	}

	bool bOriginClamped;
	const FTransform OriginBoneTransform = GetTransform(OriginTime, bOriginClamped, SchemaOriginBoneIdx);
	bClamped |= bOriginClamped;
	const FVector DeltaBoneTranslation = SampleBoneTransform.GetTranslation() - OriginBoneTransform.GetTranslation();
	OutSamplePosition = RootBoneTransform.InverseTransformVector(DeltaBoneTranslation);
	return true;
}

bool FAssetIndexer::GetSampleVelocity(FVector& OutSampleVelocity, float SampleTimeOffset, float OriginTimeOffset, int32 SampleIdx, int8 SchemaSampleBoneIdx, int8 SchemaOriginBoneIdx, bool bUseCharacterSpaceVelocities, EPermutationTimeType PermutationTimeType, int32 SamplingAttributeId)
{
	float PermutationSampleTimeOffset = 0.f;
	float PermutationOriginTimeOffset = 0.f;
	UPoseSearchFeatureChannel::GetPermutationTimeOffsets(PermutationTimeType, CalculatePermutationTimeOffset(), PermutationSampleTimeOffset, PermutationOriginTimeOffset);

	const float Time = CalculateSampleTime(SampleIdx);
	const float SampleTime = Time + SampleTimeOffset + PermutationSampleTimeOffset;
	const float OriginTime = Time + OriginTimeOffset + PermutationOriginTimeOffset;

	bool bUnused, bClampedPast;
	FVector BonePositionPast, BonePositionPresent;
	if (SamplingAttributeId >= 0)
	{
		const TConstArrayView<FAnimNotifyEvent> AnimNotifyEvents = AssetSampler.GetAllAnimNotifyEvents();
		if (!AnimNotifyEvents.IsEmpty())
		{
			for (const FAnimNotifyEvent& AnimNotifyEvent : AnimNotifyEvents)
			{
				if (const UAnimNotifyState_PoseSearchSamplingAttribute* SamplingAttribute = Cast<UAnimNotifyState_PoseSearchSamplingAttribute>(AnimNotifyEvent.NotifyStateClass))
				{
					if (SamplingAttribute->SamplingAttributeId == SamplingAttributeId)
					{
						if (SamplingAttribute->Bone.BoneName != NAME_None)
						{
							// @todo perhaps cache the initialized bone to speed up this method
							FBoneReference TempBoneReference = SamplingAttribute->Bone;
							TempBoneReference.Initialize(BoneContainer.GetSkeletonAsset());
							if (TempBoneReference.HasValidSetup())
							{
								const float SamplingAttributeTime = AnimNotifyEvent.GetTime();

								if (GetSamplePositionInternal(BonePositionPast, SamplingAttributeTime - FiniteDelta, bUseCharacterSpaceVelocities ? OriginTime - FiniteDelta : OriginTime, bClampedPast, SchemaSampleBoneIdx, SchemaOriginBoneIdx, SamplingAttributeId) &&
									GetSamplePositionInternal(BonePositionPresent, SamplingAttributeTime, OriginTime, bUnused, SchemaSampleBoneIdx, SchemaOriginBoneIdx, SamplingAttributeId))
								{
									if (!bClampedPast)
									{
										OutSampleVelocity = (BonePositionPresent - BonePositionPast) / FiniteDelta;
										return true;
									}

									FVector BonePositionFuture;
									if (GetSamplePositionInternal(BonePositionFuture, SamplingAttributeTime + FiniteDelta, bUseCharacterSpaceVelocities ? OriginTime + FiniteDelta : OriginTime, bUnused, SchemaSampleBoneIdx, SchemaOriginBoneIdx, SamplingAttributeId))
									{
										OutSampleVelocity = (BonePositionFuture - BonePositionPresent) / FiniteDelta;
										return true;
									}
								}

								// @todo: should we log some error message?
								OutSampleVelocity = FVector::ZeroVector;
								return false;
							}

							UE_LOG(LogPoseSearch, Error, TEXT("FAssetIndexer::GetSampleVelocity: required UAnimNotifyState_PoseSearchSamplingAttribute in '%s' has an invalid Bone"), *GetNameSafe(AssetSampler.GetAsset()));
							OutSampleVelocity = FVector::ZeroVector;
							return false;
						}
						
						const FTransform RootBoneTransform = GetTransform(OriginTime, bUnused, RootSchemaBoneIdx);
						OutSampleVelocity = RootBoneTransform.InverseTransformPosition(SamplingAttribute->LinearVelocity);
						return true;
					}
				}
			}
		}

		UE_LOG(LogPoseSearch, Error, TEXT("FAssetIndexer::GetSampleVelocity: required UAnimNotifyState_PoseSearchSamplingAttribute not found in '%s'"), *GetNameSafe(AssetSampler.GetAsset()));
		OutSampleVelocity = FVector::ZeroVector;
		return false;
	}

	if (GetSamplePositionInternal(BonePositionPast, SampleTime - FiniteDelta, bUseCharacterSpaceVelocities ? OriginTime - FiniteDelta : OriginTime, bClampedPast, SchemaSampleBoneIdx, SchemaOriginBoneIdx, INDEX_NONE) &&
		GetSamplePositionInternal(BonePositionPresent, SampleTime, OriginTime, bUnused, SchemaSampleBoneIdx, SchemaOriginBoneIdx, INDEX_NONE))
	{
		if (!bClampedPast)
		{
			OutSampleVelocity = (BonePositionPresent - BonePositionPast) / FiniteDelta;
			return true;
		}

		FVector BonePositionFuture;
		if (GetSamplePositionInternal(BonePositionFuture, SampleTime + FiniteDelta, bUseCharacterSpaceVelocities ? OriginTime + FiniteDelta : OriginTime, bUnused, SchemaSampleBoneIdx, SchemaOriginBoneIdx, INDEX_NONE))
		{
			OutSampleVelocity = (BonePositionFuture - BonePositionPresent) / FiniteDelta;
			return true;
		}
	}

	// @todo: should we log some error message?
	OutSampleVelocity = FVector::ZeroVector;
	return false;
}

int32 FAssetIndexer::GetBeginSampleIdx() const
{
	return SearchIndexAsset.GetBeginSampleIdx();
}

int32 FAssetIndexer::GetEndSampleIdx() const
{
	return SearchIndexAsset.GetEndSampleIdx();
}

int32 FAssetIndexer::GetNumIndexedPoses() const
{
	return SearchIndexAsset.GetNumPoses();
}

int32 FAssetIndexer::GetVectorIdx(int32 SampleIdx) const
{
	return SampleIdx - GetBeginSampleIdx();
}

TArrayView<float> FAssetIndexer::GetPoseVector(int32 SampleIdx) const
{
	return MakeArrayView(&FeatureVectorTable[GetVectorIdx(SampleIdx) * Schema.SchemaCardinality], Schema.SchemaCardinality);
}

const UPoseSearchSchema* FAssetIndexer::GetSchema() const
{
	return &Schema;
}

#if WITH_EDITOR
float FAssetIndexer::CalculatePermutationTimeOffset() const
{
	check(Schema.PermutationsSampleRate > 0 && SearchIndexAsset.IsInitialized());
	const float PermutationTimeOffset = Schema.PermutationsTimeOffset + SearchIndexAsset.GetPermutationIdx() / float(Schema.PermutationsSampleRate);
	return PermutationTimeOffset;
}
#endif // WITH_EDITOR

#if ENABLE_ANIM_DEBUG
void FAssetIndexer::CompareCachedEntries(const FAssetIndexer& Other) const
{
	if (CachedEntries.Num() != Other.CachedEntries.Num())
	{
		UE_LOG(LogPoseSearch, Warning, TEXT("CompareCachedEntries - FAssetIndexer::CachedEntries::Num is not deterministic"));
	}
	else
	{
		for (const TPair<float, CachedEntry>& Pair : CachedEntries)
		{
			if (const CachedEntry* OtherEntry = Other.CachedEntries.Find(Pair.Key))
			{
				const CachedEntry* Entry = &Pair.Value;
				if (Entry->SampleTime != OtherEntry->SampleTime)
				{
					UE_LOG(LogPoseSearch, Warning, TEXT("CompareCachedEntries - FAssetIndexer::CachedEntries::SampleTime is not deterministic (%f, %f)"), Entry->SampleTime, OtherEntry->SampleTime);
				}

				if (Entry->bClamped != OtherEntry->bClamped)
				{
					UE_LOG(LogPoseSearch, Warning, TEXT("CompareCachedEntries - FAssetIndexer::CachedEntries::bClamped is not deterministic"));
				}

				if (FMemory::Memcmp(&Entry->RootTransform, &OtherEntry->RootTransform, sizeof(FTransform)) != 0)
				{
					UE_LOG(LogPoseSearch, Warning, TEXT("CompareCachedEntries - FAssetIndexer::CachedEntries::RootTransform is not deterministic"));
				}

				if (Entry->ComponentSpacePose.GetComponentSpaceFlags() != OtherEntry->ComponentSpacePose.GetComponentSpaceFlags())
				{
					UE_LOG(LogPoseSearch, Warning, TEXT("CompareCachedEntries - FAssetIndexer::CachedEntries::ComponentSpacePose::ComponentSpaceFlags is not deterministic"));
				}

				const TConstArrayView<FTransform> Bones = Entry->ComponentSpacePose.GetPose().GetBones();
				const TConstArrayView<FTransform> OtherBones = OtherEntry->ComponentSpacePose.GetPose().GetBones();
				if (Bones.Num() != OtherBones.Num())
				{
					UE_LOG(LogPoseSearch, Warning, TEXT("CompareCachedEntries - FAssetIndexer::CachedEntries::ComponentSpacePose::Bones is not deterministic"));
				}
				else
				{
					for (int32 Index = 0; Index < Bones.Num(); ++Index)
					{
						if (FMemory::Memcmp(&Bones[Index], &OtherBones[Index], sizeof(FTransform)) != 0)
						{
							UE_LOG(LogPoseSearch, Warning, TEXT("CompareCachedEntries - FAssetIndexer::CachedEntries::Bones[%d] is not deterministic"), Index);
						}
					}
				}
			}
			else
			{
				UE_LOG(LogPoseSearch, Warning, TEXT("CompareCachedEntries - FAssetIndexer::CachedEntries is not deterministic. Missing CachedEntry at time %f"), Pair.Key);
			}
		}
	}
}
#endif // ENABLE_ANIM_DEBUG

} // namespace UE::PoseSearch
#endif // WITH_EDITOR
