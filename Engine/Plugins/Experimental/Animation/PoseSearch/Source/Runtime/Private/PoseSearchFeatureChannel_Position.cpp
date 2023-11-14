// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchFeatureChannel_Position.h"
#include "PoseSearch/PoseSearchAssetIndexer.h"
#include "PoseSearch/PoseSearchAssetSampler.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "Engine/BlueprintGeneratedClass.h"

UPoseSearchFeatureChannel_Position::UPoseSearchFeatureChannel_Position()
{
	bUseBlueprintQueryOverride = Cast<UBlueprintGeneratedClass>(GetClass()) != nullptr;
}

void UPoseSearchFeatureChannel_Position::FindOrAddToSchema(UPoseSearchSchema* Schema, float SampleTimeOffset, const FName& BoneName, EPermutationTimeType PermutationTimeType)
{
	if (!Schema->FindChannel([SampleTimeOffset, &BoneName, PermutationTimeType](const UPoseSearchFeatureChannel* Channel) -> const UPoseSearchFeatureChannel_Position*
		{
			if (const UPoseSearchFeatureChannel_Position* Position = Cast<UPoseSearchFeatureChannel_Position>(Channel))
			{
				if (Position->Bone.BoneName == BoneName && Position->OriginBone.BoneName == NAME_None && Position->SampleTimeOffset == SampleTimeOffset && Position->OriginTimeOffset == 0.f && Position->PermutationTimeType == PermutationTimeType)
				{
					return Position;
				}
			}
			return nullptr;
		}))
	{
		UPoseSearchFeatureChannel_Position* Position = NewObject<UPoseSearchFeatureChannel_Position>(Schema, NAME_None, RF_Transient);
		Position->Bone.BoneName = BoneName;
#if WITH_EDITORONLY_DATA
		Position->Weight = 0.f;
		// @todo: perhaps add a tunable color for injected channels
		Position->DebugColor = FLinearColor::Gray;
#endif // WITH_EDITORONLY_DATA
		Position->SampleTimeOffset = SampleTimeOffset;
		Position->PermutationTimeType = PermutationTimeType;
		Schema->AddTemporaryChannel(Position);
	}
}

void UPoseSearchFeatureChannel_Position::Finalize(UPoseSearchSchema* Schema)
{
	ChannelDataOffset = Schema->SchemaCardinality;
	ChannelCardinality = UE::PoseSearch::FFeatureVectorHelper::GetVectorCardinality(ComponentStripping);
	Schema->SchemaCardinality += ChannelCardinality;

	SchemaBoneIdx = Schema->AddBoneReference(Bone);
	SchemaOriginBoneIdx = Schema->AddBoneReference(OriginBone);
}

void UPoseSearchFeatureChannel_Position::AddDependentChannels(UPoseSearchSchema* Schema) const
{
	using namespace UE::PoseSearch;

	if (Schema->bInjectAdditionalDebugChannels)
	{
		if (SchemaOriginBoneIdx != RootSchemaBoneIdx)
		{
			const EPermutationTimeType DependentChannelsPermutationTimeType = PermutationTimeType == EPermutationTimeType::UsePermutationTime ? EPermutationTimeType::UseSampleToPermutationTime : EPermutationTimeType::UseSampleTime;
			UPoseSearchFeatureChannel_Position::FindOrAddToSchema(Schema, 0.f, OriginBone.BoneName, DependentChannelsPermutationTimeType);
		}
	}
}

void UPoseSearchFeatureChannel_Position::BuildQuery(UE::PoseSearch::FSearchContext& SearchContext) const
{
	using namespace UE::PoseSearch;

	if (bUseBlueprintQueryOverride)
	{
		const FVector BonePositionWorld = BP_GetWorldPosition(SearchContext.GetAnimInstance());
		const FVector BonePosition = SearchContext.GetSamplePosition(SampleTimeOffset, OriginTimeOffset, SchemaBoneIdx, SchemaOriginBoneIdx, PermutationTimeType, &BonePositionWorld);
  		FFeatureVectorHelper::EncodeVector(SearchContext.EditFeatureVector(), ChannelDataOffset, BonePosition, ComponentStripping);
		return;
	}

	// trying to get the BuildQuery data from another schema UPoseSearchFeatureChannel_Position already cached in the SearchContext
	if (SearchContext.IsUseCachedChannelData())
	{
		// composing a unique identifier to specify this channel with all the required properties to be able to share the query data with other channels of the same type
		uint32 UniqueIdentifier = GetClass()->GetUniqueID();
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, ::GetTypeHash(SamplingAttributeId));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, ::GetTypeHash(SampleTimeOffset));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, ::GetTypeHash(OriginTimeOffset));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, ::GetTypeHash(SchemaBoneIdx));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, ::GetTypeHash(SchemaOriginBoneIdx));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, ::GetTypeHash(InputQueryPose));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, ::GetTypeHash(ComponentStripping));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, ::GetTypeHash(PermutationTimeType));

		TConstArrayView<float> CachedChannelData;
		if (const UPoseSearchFeatureChannel* CachedChannel = SearchContext.GetCachedChannelData(UniqueIdentifier, this, CachedChannelData))
		{
#if DO_CHECK
			const UPoseSearchFeatureChannel_Position* CachedPositionChannel = Cast<UPoseSearchFeatureChannel_Position>(CachedChannel);
			check(CachedPositionChannel);
			check(CachedPositionChannel->GetChannelCardinality() == ChannelCardinality);
			check(CachedChannelData.Num() == ChannelCardinality);

			// making sure there were no hash collisions
			check(CachedPositionChannel->SamplingAttributeId == SamplingAttributeId);
			check(CachedPositionChannel->SampleTimeOffset == SampleTimeOffset);
			check(CachedPositionChannel->OriginTimeOffset == OriginTimeOffset);
			check(CachedPositionChannel->SchemaBoneIdx == SchemaBoneIdx);
			check(CachedPositionChannel->SchemaOriginBoneIdx == SchemaOriginBoneIdx);
			check(CachedPositionChannel->InputQueryPose == InputQueryPose);
			check(CachedPositionChannel->ComponentStripping == ComponentStripping);
			check(CachedPositionChannel->PermutationTimeType == PermutationTimeType);
#endif //DO_CHECK

			// copying the CachedChannelData into this channel portion of the FeatureVectorBuilder
			FFeatureVectorHelper::Copy(SearchContext.EditFeatureVector().Slice(ChannelDataOffset, ChannelCardinality), 0, ChannelCardinality, CachedChannelData);
			return;
		}
	}

	const bool bCanUseCurrentResult = SearchContext.CanUseCurrentResult();
	const bool bSkip = InputQueryPose != EInputQueryPose::UseCharacterPose && bCanUseCurrentResult;
	const bool bIsRootBone = SchemaBoneIdx == RootSchemaBoneIdx;
	if (bSkip || (!SearchContext.IsHistoryValid() && !bIsRootBone))
	{
		if (bCanUseCurrentResult)
		{
			FFeatureVectorHelper::Copy(SearchContext.EditFeatureVector(), ChannelDataOffset, ChannelCardinality, SearchContext.GetCurrentResultPoseVector());
			return;
		}

		// we leave the SearchContext.EditFeatureVector() set to zero since the SearchContext.History is invalid and it'll fail if we continue
		UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchFeatureChannel_Position::BuildQuery - Failed because Pose History Node is missing."));
		return;
	}
	
	// calculating the BonePosition in root bone space for the bone indexed by SchemaBoneIdx
	const FVector BonePosition = SearchContext.GetSamplePosition(SampleTimeOffset, OriginTimeOffset, SchemaBoneIdx, SchemaOriginBoneIdx, PermutationTimeType);
	FFeatureVectorHelper::EncodeVector(SearchContext.EditFeatureVector(), ChannelDataOffset, BonePosition, ComponentStripping);
}

#if ENABLE_DRAW_DEBUG
void UPoseSearchFeatureChannel_Position::DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const
{
	using namespace UE::PoseSearch;

	FColor Color;
#if WITH_EDITORONLY_DATA
	Color = DebugColor.ToFColor(true);
#else // WITH_EDITORONLY_DATA
	Color = FLinearColor::Blue.ToFColor(true);
#endif // WITH_EDITORONLY_DATA

	const FVector FeaturesVector = FFeatureVectorHelper::DecodeVector(PoseVector, ChannelDataOffset, ComponentStripping);
	if (SchemaOriginBoneIdx == RootSchemaBoneIdx && FMath::IsNearlyZero(OriginTimeOffset))
	{
		const FVector BonePos = DrawParams.GetRootTransform().TransformPosition(FeaturesVector);
		DrawParams.DrawPoint(BonePos, Color);
	}
	else
	{
		const EPermutationTimeType TimeType = PermutationTimeType == EPermutationTimeType::UsePermutationTime ? EPermutationTimeType::UseSampleToPermutationTime : EPermutationTimeType::UseSampleTime;
		const FVector OriginBonePos = DrawParams.ExtractPosition(PoseVector, OriginTimeOffset, SchemaOriginBoneIdx, TimeType);
		const FVector DeltaPos = DrawParams.GetRootTransform().TransformVector(FeaturesVector);
		const FVector BonePos = OriginBonePos + DeltaPos;
		DrawParams.DrawLine(OriginBonePos, BonePos, Color);
		DrawParams.DrawPoint(OriginBonePos, Color);
		DrawParams.DrawPoint(BonePos, Color);
	}	
}
#endif // ENABLE_DRAW_DEBUG

#if WITH_EDITOR
void UPoseSearchFeatureChannel_Position::FillWeights(TArrayView<float> Weights) const
{
	for (int32 i = 0; i < ChannelCardinality; ++i)
	{
		Weights[ChannelDataOffset + i] = Weight;
	}
}

bool UPoseSearchFeatureChannel_Position::IndexAsset(UE::PoseSearch::FAssetIndexer& Indexer) const
{
	using namespace UE::PoseSearch;

	FVector BonePosition;
	for (int32 SampleIdx = Indexer.GetBeginSampleIdx(); SampleIdx != Indexer.GetEndSampleIdx(); ++SampleIdx)
	{
		if (Indexer.GetSamplePosition(BonePosition, SampleTimeOffset, OriginTimeOffset, SampleIdx, SchemaBoneIdx, SchemaOriginBoneIdx, PermutationTimeType, SamplingAttributeId))
		{
			FFeatureVectorHelper::EncodeVector(Indexer.GetPoseVector(SampleIdx), ChannelDataOffset, BonePosition, ComponentStripping);
		}
		else
		{
			return false;
		}
	}
	return true;
}

UE::PoseSearch::TLabelBuilder& UPoseSearchFeatureChannel_Position::GetLabel(UE::PoseSearch::TLabelBuilder& LabelBuilder, UE::PoseSearch::ELabelFormat LabelFormat) const
{
	using namespace UE::PoseSearch;

	GetOuterLabel(LabelBuilder, LabelFormat);
	AppendLabelSeparator(LabelBuilder, LabelFormat);

	LabelBuilder.Append(TEXT("Pos"));

	if (ComponentStripping == EComponentStrippingVector::StripXY)
	{
		LabelBuilder.Append(TEXT("_z"));
	}
	else if (ComponentStripping == EComponentStrippingVector::StripZ)
	{
		LabelBuilder.Append(TEXT("_xy"));
	}

	const UPoseSearchSchema* Schema = GetSchema();
	check(Schema);
	if (SchemaBoneIdx != RootSchemaBoneIdx)
	{
		LabelBuilder.Append(TEXT("_"));
		LabelBuilder.Append(Schema->BoneReferences[SchemaBoneIdx].BoneName.ToString());
	}

	if (SchemaOriginBoneIdx != RootSchemaBoneIdx)
	{
		LabelBuilder.Append(TEXT("_"));
		LabelBuilder.Append(Schema->BoneReferences[SchemaOriginBoneIdx].BoneName.ToString());
	}

	AppendLabelSeparator(LabelBuilder, LabelFormat, true);

	LabelBuilder.Appendf(TEXT("%.2f"), SampleTimeOffset);

	if (!FMath::IsNearlyZero(OriginTimeOffset))
	{
		LabelBuilder.Appendf(TEXT("-%.2f"), OriginTimeOffset);
	}

	return LabelBuilder;
}
#endif