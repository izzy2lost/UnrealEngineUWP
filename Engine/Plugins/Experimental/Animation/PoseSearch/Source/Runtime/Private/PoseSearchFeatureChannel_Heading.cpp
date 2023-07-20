// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchFeatureChannel_Heading.h"
#include "PoseSearch/PoseSearchAssetIndexer.h"
#include "PoseSearch/PoseSearchAssetSampler.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearchFeatureChannel_Position.h"

void UPoseSearchFeatureChannel_Heading::FindOrAddToSchema(UPoseSearchSchema* Schema, float SampleTimeOffset, const FName& BoneName, EHeadingAxis HeadingAxis)
{
	if (!Schema->FindChannel([SampleTimeOffset, &BoneName, HeadingAxis](const UPoseSearchFeatureChannel* Channel) -> const UPoseSearchFeatureChannel_Heading*
		{
			if (const UPoseSearchFeatureChannel_Heading* Heading = Cast<UPoseSearchFeatureChannel_Heading>(Channel))
			{
				if (Heading->Bone.BoneName == BoneName && Heading->SampleTimeOffset == SampleTimeOffset && Heading->OriginTimeOffset == 0.f && Heading->HeadingAxis == HeadingAxis)
				{
					return Heading;
				}
			}
			return nullptr;
		}))
	{
		UPoseSearchFeatureChannel_Heading* Heading = NewObject<UPoseSearchFeatureChannel_Heading>(Schema, NAME_None, RF_Transient);
		Heading->Bone.BoneName = BoneName;
		Heading->Weight = 0.f;
		Heading->SampleTimeOffset = SampleTimeOffset;
		Heading->HeadingAxis = HeadingAxis;
		// @todo: perhaps add a tunable color for injected channels
		Heading->DebugColor = FLinearColor::Gray;
		Schema->AddTemporaryChannel(Heading);
	}
}

void UPoseSearchFeatureChannel_Heading::Finalize(UPoseSearchSchema* Schema)
{
	ChannelDataOffset = Schema->SchemaCardinality;
	ChannelCardinality = UE::PoseSearch::FFeatureVectorHelper::GetVectorCardinality(ComponentStripping);
	Schema->SchemaCardinality += ChannelCardinality;
	SchemaBoneIdx = Schema->AddBoneReference(Bone);
}

void UPoseSearchFeatureChannel_Heading::AddDependentChannels(UPoseSearchSchema* Schema) const
{
	if (Schema->bInjectAdditionalDebugChannels)
	{
		UPoseSearchFeatureChannel_Position::FindOrAddToSchema(Schema, SampleTimeOffset, Bone.BoneName);
		if (!FMath::IsNearlyZero(OriginTimeOffset))
		{
			// adding the position OriginTimeOffset seconds ahead
			UPoseSearchFeatureChannel_Position::FindOrAddToSchema(Schema, OriginTimeOffset, Bone.BoneName);
			
			// adding the rotation (X, Y axis) OriginTimeOffset seconds ahead
			UPoseSearchFeatureChannel_Heading::FindOrAddToSchema(Schema, OriginTimeOffset, Bone.BoneName, EHeadingAxis::X);
			UPoseSearchFeatureChannel_Heading::FindOrAddToSchema(Schema, OriginTimeOffset, Bone.BoneName, EHeadingAxis::Y);
		}
	}
}

FVector UPoseSearchFeatureChannel_Heading::GetAxis(const FQuat& Rotation) const
{
	switch (HeadingAxis)
	{
	case EHeadingAxis::X:
		return Rotation.GetAxisX();
	case EHeadingAxis::Y:
		return Rotation.GetAxisY();
	case EHeadingAxis::Z:
		return Rotation.GetAxisZ();
	}

	checkNoEntry();
	return FVector::XAxisVector;
}

void UPoseSearchFeatureChannel_Heading::BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, UE::PoseSearch::FFeatureVectorBuilder& InOutQuery) const
{
	using namespace UE::PoseSearch;

	const bool bIsCurrentResultValid = SearchContext.GetCurrentResult().IsValid() && SearchContext.GetCurrentResult().Database->Schema == InOutQuery.GetSchema();
	const bool bSkip = InputQueryPose != EInputQueryPose::UseCharacterPose && bIsCurrentResultValid;
	const bool bIsRootBone = InOutQuery.GetSchema()->IsRootBone(SchemaBoneIdx);
	if (bSkip || (!SearchContext.IsHistoryValid() && !bIsRootBone))
	{
		if (bIsCurrentResultValid)
		{
			FFeatureVectorHelper::Copy(InOutQuery.EditValues(), ChannelDataOffset, ChannelCardinality, SearchContext.GetCurrentResultPoseVector());
		}
		else
		{
			// we leave the InOutQuery set to zero since the SearchContext.History is invalid and it'll fail if we continue
			UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchFeatureChannel_Heading::BuildQuery - Failed because Pose History Node is missing."));
		}
	}
	else
	{
		// calculating the BoneRotation in component space for the bone indexed by SchemaBoneIdx
		const FQuat BoneRotation = SearchContext.GetSampleRotation(SampleTimeOffset, OriginTimeOffset, InOutQuery.GetSchema(), SchemaBoneIdx, RootSchemaBoneIdx, !bIsRootBone);
		FFeatureVectorHelper::EncodeVector(InOutQuery.EditValues(), ChannelDataOffset, GetAxis(BoneRotation), ComponentStripping);
	}
}

#if ENABLE_DRAW_DEBUG
void UPoseSearchFeatureChannel_Heading::DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const
{
	using namespace UE::PoseSearch;

	const FColor Color = DebugColor.ToFColor(true);
	const FVector BoneHeading = DrawParams.ExtractRotation(PoseVector, OriginTimeOffset).RotateVector(FFeatureVectorHelper::DecodeVector(PoseVector, ChannelDataOffset, ComponentStripping));
	const FVector BonePos = DrawParams.ExtractPosition(PoseVector, SampleTimeOffset, SchemaBoneIdx);

	DrawParams.DrawPoint(BonePos, Color, 3.f);
	DrawParams.DrawLine(BonePos + BoneHeading * 4.f, BonePos + BoneHeading * 15.f, Color);
}
#endif // ENABLE_DRAW_DEBUG

#if WITH_EDITOR
void UPoseSearchFeatureChannel_Heading::FillWeights(TArrayView<float> Weights) const
{
	for (int32 i = 0; i < ChannelCardinality; ++i)
	{
		Weights[ChannelDataOffset + i] = Weight;
	}
}

void UPoseSearchFeatureChannel_Heading::IndexAsset(UE::PoseSearch::FAssetIndexer& Indexer) const
{
	using namespace UE::PoseSearch;

	for (int32 SampleIdx = Indexer.GetBeginSampleIdx(); SampleIdx != Indexer.GetEndSampleIdx(); ++SampleIdx)
	{
		const FVector Heading = GetAxis(Indexer.GetSampleRotation(SampleTimeOffset, OriginTimeOffset, SampleIdx, SchemaBoneIdx));
		FFeatureVectorHelper::EncodeVector(Indexer.GetPoseVector(SampleIdx), ChannelDataOffset, Heading, ComponentStripping);
	}
}

FString UPoseSearchFeatureChannel_Heading::GetLabel() const
{
	TStringBuilder<256> Label;
	if (const UPoseSearchFeatureChannel* OuterChannel = Cast<UPoseSearchFeatureChannel>(GetOuter()))
	{
		Label.Append(OuterChannel->GetLabel());
		Label.Append(TEXT("_"));
	}

	Label.Append(TEXT("Head"));
	switch (HeadingAxis)
	{
	case EHeadingAxis::X:
		Label.Append(TEXT("X"));
		break;
	case EHeadingAxis::Y:
		Label.Append(TEXT("Y"));
		break;
	case EHeadingAxis::Z:
		Label.Append(TEXT("Z"));
		break;
	}

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
	if (!Schema->IsRootBone(SchemaBoneIdx))
	{
		Label.Append(TEXT("_"));
		Label.Append(Schema->BoneReferences[SchemaBoneIdx].BoneName.ToString());
	}

	Label.Appendf(TEXT(" %.2f"), SampleTimeOffset);

	if (!FMath::IsNearlyZero(OriginTimeOffset))
	{
		Label.Appendf(TEXT("-%.2f"), OriginTimeOffset);
	}

	return Label.ToString();
}
#endif