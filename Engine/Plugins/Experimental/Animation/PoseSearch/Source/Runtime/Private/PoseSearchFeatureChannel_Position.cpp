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

void UPoseSearchFeatureChannel_Position::BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, UE::PoseSearch::FFeatureVectorBuilder& InOutQuery) const
{
	using namespace UE::PoseSearch;

	check(InOutQuery.GetSchema());
	if (bUseBlueprintQueryOverride)
	{
		const FVector BonePositionWorld = BP_GetWorldPosition(SearchContext.GetAnimInstance());
		const FVector BonePosition = SearchContext.GetSamplePosition(SampleTimeOffset, OriginTimeOffset, InOutQuery.GetSchema(), SchemaBoneIdx, SchemaOriginBoneIdx, PermutationTimeType, &BonePositionWorld);
  		FFeatureVectorHelper::EncodeVector(InOutQuery.EditValues(), ChannelDataOffset, BonePosition, ComponentStripping);
	}
	else
	{
		const bool bIsCurrentResultValid = SearchContext.GetCurrentResult().IsValid() && SearchContext.GetCurrentResult().Database->Schema == InOutQuery.GetSchema();
		const bool bSkip = InputQueryPose != EInputQueryPose::UseCharacterPose && bIsCurrentResultValid;
		const bool bIsRootBone = SchemaBoneIdx == RootSchemaBoneIdx;
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
			const FVector BonePosition = SearchContext.GetSamplePosition(SampleTimeOffset, OriginTimeOffset, InOutQuery.GetSchema(), SchemaBoneIdx, SchemaOriginBoneIdx, PermutationTimeType);
			FFeatureVectorHelper::EncodeVector(InOutQuery.EditValues(), ChannelDataOffset, BonePosition, ComponentStripping);
		}
	}
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