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
				if (Position->Bone.BoneName == BoneName && Position->OriginBone.BoneName == NAME_None && Position->SampleTimeOffset == SampleTimeOffset && Position->PermutationTimeType == PermutationTimeType)
				{
					return Position;
				}
			}
			return nullptr;
		}))
	{
		UPoseSearchFeatureChannel_Position* Position = NewObject<UPoseSearchFeatureChannel_Position>(Schema, NAME_None, RF_Transient);
		Position->Bone.BoneName = BoneName;
		Position->Weight = 0.f;
		Position->SampleTimeOffset = SampleTimeOffset;
		// @todo: perhaps add a tunable color for injected channels
		Position->DebugColor = FLinearColor::Gray;
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
	if (Schema->bInjectAdditionalDebugChannels)
	{
		if (SchemaOriginBoneIdx != RootSchemaBoneIdx)
		{
			const EPermutationTimeType DependentChannelsPermutationTimeType = PermutationTimeType == EPermutationTimeType::UsePermutationTime ? EPermutationTimeType::UseSampleToPermutationTime : EPermutationTimeType::UseSampleTime;
			UPoseSearchFeatureChannel_Position::FindOrAddToSchema(Schema, 0.f, OriginBone.BoneName, DependentChannelsPermutationTimeType);
		}
	}
}

void UPoseSearchFeatureChannel_Position::BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, UE::PoseSearch::FFeatureVectorBuilder& InOutQuery) const
{
	using namespace UE::PoseSearch;

	check(InOutQuery.GetSchema());
	const bool bIsRootBone = SchemaBoneIdx == RootSchemaBoneIdx;
	if (bUseBlueprintQueryOverride)
	{
		const FVector BonePositionWorld = BP_GetWorldPosition(SearchContext.GetAnimInstance());
		const FVector BonePosition = SearchContext.GetSamplePosition(SampleTimeOffset, 0.f, InOutQuery.GetSchema(), SchemaBoneIdx, SchemaOriginBoneIdx, /*!bIsRootBone*/ true, PermutationTimeType, &BonePositionWorld);
  		FFeatureVectorHelper::EncodeVector(InOutQuery.EditValues(), ChannelDataOffset, BonePosition, ComponentStripping);
	}
	else
	{
		const bool bIsCurrentResultValid = SearchContext.GetCurrentResult().IsValid() && SearchContext.GetCurrentResult().Database->Schema == InOutQuery.GetSchema();
		const bool bSkip = InputQueryPose != EInputQueryPose::UseCharacterPose && bIsCurrentResultValid;
		if (bSkip || (!SearchContext.IsHistoryValid() && !bIsRootBone))
		{
			if (bIsCurrentResultValid)
			{
				FFeatureVectorHelper::Copy(InOutQuery.EditValues(), ChannelDataOffset, ChannelCardinality, SearchContext.GetCurrentResultPoseVector());
			}
			else
			{
				// we leave the InOutQuery set to zero since the SearchContext.History is invalid and it'll fail if we continue
				UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchFeatureChannel_Position::BuildQuery - Failed because Pose History Node is missing."));
			}
		}
		else
		{
			// calculating the BonePosition in root bone space for the bone indexed by SchemaBoneIdx
			const FVector BonePosition = SearchContext.GetSamplePosition(SampleTimeOffset, 0.f, InOutQuery.GetSchema(), SchemaBoneIdx, SchemaOriginBoneIdx, !bIsRootBone, PermutationTimeType);
			FFeatureVectorHelper::EncodeVector(InOutQuery.EditValues(), ChannelDataOffset, BonePosition, ComponentStripping);
		}
	}
}

#if ENABLE_DRAW_DEBUG
void UPoseSearchFeatureChannel_Position::DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const
{
	using namespace UE::PoseSearch;

	const FColor Color = DebugColor.ToFColor(true);

	const FVector FeaturesVector = FFeatureVectorHelper::DecodeVector(PoseVector, ChannelDataOffset, ComponentStripping);
	if (SchemaOriginBoneIdx == RootSchemaBoneIdx)
	{
		const FVector BonePos = DrawParams.GetRootTransform().TransformPosition(FeaturesVector);
		DrawParams.DrawPoint(BonePos, Color);
	}
	else
	{
		const EPermutationTimeType TimeType = PermutationTimeType == EPermutationTimeType::UsePermutationTime ? EPermutationTimeType::UseSampleToPermutationTime : EPermutationTimeType::UseSampleTime;
		const FVector OriginBonePos = DrawParams.ExtractPosition(PoseVector, SampleTimeOffset, SchemaOriginBoneIdx, TimeType, SamplingAttributeId);
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
		if (Indexer.GetSamplePosition(BonePosition, SampleTimeOffset, 0.f, SampleIdx, SchemaBoneIdx, SchemaOriginBoneIdx, PermutationTimeType, SamplingAttributeId))
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

FString UPoseSearchFeatureChannel_Position::GetLabel() const
{
	TStringBuilder<256> Label;
	if (const UPoseSearchFeatureChannel* OuterChannel = Cast<UPoseSearchFeatureChannel>(GetOuter()))
	{
		Label.Append(OuterChannel->GetLabel());
		Label.Append(TEXT("_"));
	}

	Label.Append(TEXT("Pos"));

	if (ComponentStripping == EComponentStrippingVector::StripXY)
	{
		Label.Append(TEXT("_z"));
	}
	else if (ComponentStripping == EComponentStrippingVector::StripZ)
	{
		Label.Append(TEXT("_xy"));
	}

	const UPoseSearchSchema* Schema = GetSchema();
	check(Schema);
	if (SchemaBoneIdx != RootSchemaBoneIdx)
	{
		Label.Append(TEXT("_"));
		Label.Append(Schema->BoneReferences[SchemaBoneIdx].BoneName.ToString());
	}

	if (SchemaOriginBoneIdx != RootSchemaBoneIdx)
	{
		Label.Append(TEXT("_"));
		Label.Append(Schema->BoneReferences[SchemaOriginBoneIdx].BoneName.ToString());
	}

	Label.Appendf(TEXT(" %.2f"), SampleTimeOffset);
	return Label.ToString();
}
#endif