// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchFeatureChannel_Heading.h"
#include "PoseSearch/PoseSearchAssetIndexer.h"
#include "PoseSearch/PoseSearchAssetSampler.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearchFeatureChannel_Position.h"
#include "Engine/BlueprintGeneratedClass.h"

UPoseSearchFeatureChannel_Heading::UPoseSearchFeatureChannel_Heading()
{
	bUseBlueprintQueryOverride = Cast<UBlueprintGeneratedClass>(GetClass()) != nullptr;
}

void UPoseSearchFeatureChannel_Heading::FindOrAddToSchema(UPoseSearchSchema* Schema, float SampleTimeOffset, const FName& BoneName, EHeadingAxis HeadingAxis, EPermutationTimeType PermutationTimeType)
{
	if (!Schema->FindChannel([SampleTimeOffset, &BoneName, HeadingAxis, PermutationTimeType](const UPoseSearchFeatureChannel* Channel) -> const UPoseSearchFeatureChannel_Heading*
		{
			if (const UPoseSearchFeatureChannel_Heading* Heading = Cast<UPoseSearchFeatureChannel_Heading>(Channel))
			{
				if (Heading->Bone.BoneName == BoneName && Heading->OriginBone.BoneName == NAME_None && Heading->SampleTimeOffset == SampleTimeOffset && Heading->OriginTimeOffset == 0.f && Heading->HeadingAxis == HeadingAxis && Heading->PermutationTimeType == PermutationTimeType)
				{
					return Heading;
				}
			}
			return nullptr;
		}))
	{
		UPoseSearchFeatureChannel_Heading* Heading = NewObject<UPoseSearchFeatureChannel_Heading>(Schema, NAME_None, RF_Transient);
		Heading->Bone.BoneName = BoneName;
#if WITH_EDITORONLY_DATA
		Heading->Weight = 0.f;
		// @todo: perhaps add a tunable color for injected channels
		Heading->DebugColor = FLinearColor::Gray;
#endif // WITH_EDITORONLY_DATA
		Heading->SampleTimeOffset = SampleTimeOffset;
		Heading->HeadingAxis = HeadingAxis;
		Heading->PermutationTimeType = PermutationTimeType;
		Schema->AddTemporaryChannel(Heading);
	}
}

void UPoseSearchFeatureChannel_Heading::Finalize(UPoseSearchSchema* Schema)
{
	ChannelDataOffset = Schema->SchemaCardinality;
	ChannelCardinality = UE::PoseSearch::FFeatureVectorHelper::GetVectorCardinality(ComponentStripping);
	Schema->SchemaCardinality += ChannelCardinality;

	SchemaBoneIdx = Schema->AddBoneReference(Bone);
	SchemaOriginBoneIdx = Schema->AddBoneReference(OriginBone);
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

void UPoseSearchFeatureChannel_Heading::BuildQuery(UE::PoseSearch::FSearchContext& SearchContext) const
{
	using namespace UE::PoseSearch;

	const bool bIsRootBone = SchemaBoneIdx == RootSchemaBoneIdx;
	if (bUseBlueprintQueryOverride)
	{
		const FQuat BoneRotationWorld = BP_GetWorldRotation(SearchContext.GetAnimInstance());
		const FQuat BoneRotation = SearchContext.GetSampleRotation(SampleTimeOffset, OriginTimeOffset, SchemaBoneIdx, SchemaOriginBoneIdx, EPermutationTimeType::UseSampleTime, &BoneRotationWorld);
		FFeatureVectorHelper::EncodeVector(SearchContext.EditFeatureVector(), ChannelDataOffset, GetAxis(BoneRotation), ComponentStripping);
		return;
	}
	
	// trying to get the BuildQuery data from another schema UPoseSearchFeatureChannel_Heading already cached in the SearchContext
	if (SearchContext.IsUseCachedChannelData())
	{
		// composing a unique identifier to specify this channel with all the required properties to be able to share the query data with other channels of the same type
		uint32 UniqueIdentifier = GetClass()->GetUniqueID();
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, ::GetTypeHash(SamplingAttributeId));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, ::GetTypeHash(SampleTimeOffset));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, ::GetTypeHash(OriginTimeOffset));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, ::GetTypeHash(HeadingAxis));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, ::GetTypeHash(SchemaBoneIdx));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, ::GetTypeHash(SchemaOriginBoneIdx));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, ::GetTypeHash(InputQueryPose));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, ::GetTypeHash(ComponentStripping));
		UniqueIdentifier = HashCombineFast(UniqueIdentifier, ::GetTypeHash(PermutationTimeType));

		TConstArrayView<float> CachedChannelData;
		if (const UPoseSearchFeatureChannel* CachedChannel = SearchContext.GetCachedChannelData(UniqueIdentifier, this, CachedChannelData))
		{
#if DO_CHECK
			const UPoseSearchFeatureChannel_Heading* CachedHeadingChannel = Cast<UPoseSearchFeatureChannel_Heading>(CachedChannel);
			check(CachedHeadingChannel);
			check(CachedHeadingChannel->GetChannelCardinality() == ChannelCardinality);
			check(CachedChannelData.Num() == ChannelCardinality);

			// making sure there were no hash collisions
			check(CachedHeadingChannel->SamplingAttributeId == SamplingAttributeId);
			check(CachedHeadingChannel->SampleTimeOffset == SampleTimeOffset);
			check(CachedHeadingChannel->OriginTimeOffset == OriginTimeOffset);
			check(CachedHeadingChannel->HeadingAxis == HeadingAxis);
			check(CachedHeadingChannel->SchemaBoneIdx == SchemaBoneIdx);
			check(CachedHeadingChannel->SchemaOriginBoneIdx == SchemaOriginBoneIdx);
			check(CachedHeadingChannel->InputQueryPose == InputQueryPose);
			check(CachedHeadingChannel->ComponentStripping == ComponentStripping);
			check(CachedHeadingChannel->PermutationTimeType == PermutationTimeType);
#endif //DO_CHECK

			// copying the CachedChannelData into this channel portion of the FeatureVectorBuilder
			FFeatureVectorHelper::Copy(SearchContext.EditFeatureVector().Slice(ChannelDataOffset, ChannelCardinality), 0, ChannelCardinality, CachedChannelData);
			return;
		}
	}

	const bool bCanUseCurrentResult = SearchContext.CanUseCurrentResult();
	const bool bSkip = InputQueryPose != EInputQueryPose::UseCharacterPose && bCanUseCurrentResult;
	if (bSkip || (!SearchContext.IsHistoryValid() && !bIsRootBone))
	{
		if (bCanUseCurrentResult)
		{
			FFeatureVectorHelper::Copy(SearchContext.EditFeatureVector(), ChannelDataOffset, ChannelCardinality, SearchContext.GetCurrentResultPoseVector());
			return;
		}
		
		// we leave the SearchContext.EditFeatureVector() set to zero since the SearchContext.History is invalid and it'll fail if we continue
		UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchFeatureChannel_Heading::BuildQuery - Failed because Pose History Node is missing."));
		return;
	}
	
	// calculating the BoneRotation in component space for the bone indexed by SchemaBoneIdx
	const FQuat BoneRotation = SearchContext.GetSampleRotation(SampleTimeOffset, OriginTimeOffset, SchemaBoneIdx, SchemaOriginBoneIdx, PermutationTimeType);
	FFeatureVectorHelper::EncodeVector(SearchContext.EditFeatureVector(), ChannelDataOffset, GetAxis(BoneRotation), ComponentStripping);
}

#if ENABLE_DRAW_DEBUG
void UPoseSearchFeatureChannel_Heading::DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const
{
	using namespace UE::PoseSearch;

	FColor Color;
#if WITH_EDITORONLY_DATA
	Color = DebugColor.ToFColor(true);
#else // WITH_EDITORONLY_DATA
	Color = FLinearColor::White.ToFColor(true);
#endif // WITH_EDITORONLY_DATA

	const FVector BoneHeading = DrawParams.ExtractRotation(PoseVector, OriginTimeOffset).RotateVector(FFeatureVectorHelper::DecodeVector(PoseVector, ChannelDataOffset, ComponentStripping));
	const FVector BonePos = DrawParams.ExtractPosition(PoseVector, SampleTimeOffset, SchemaBoneIdx, PermutationTimeType, SamplingAttributeId);

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

bool UPoseSearchFeatureChannel_Heading::IndexAsset(UE::PoseSearch::FAssetIndexer& Indexer) const
{
	using namespace UE::PoseSearch;

	FQuat SampleRotation = FQuat::Identity;
	for (int32 SampleIdx = Indexer.GetBeginSampleIdx(); SampleIdx != Indexer.GetEndSampleIdx(); ++SampleIdx)
	{
		if (Indexer.GetSampleRotation(SampleRotation, SampleTimeOffset, OriginTimeOffset, SampleIdx, SchemaBoneIdx, SchemaOriginBoneIdx, PermutationTimeType, SamplingAttributeId))
		{
			FFeatureVectorHelper::EncodeVector(Indexer.GetPoseVector(SampleIdx), ChannelDataOffset, GetAxis(SampleRotation), ComponentStripping);
		}
		else
		{
			return false;
		}
	}
	return true;
}

UE::PoseSearch::TLabelBuilder& UPoseSearchFeatureChannel_Heading::GetLabel(UE::PoseSearch::TLabelBuilder& LabelBuilder, UE::PoseSearch::ELabelFormat LabelFormat) const
{
	using namespace UE::PoseSearch;

	GetOuterLabel(LabelBuilder, LabelFormat);
	AppendLabelSeparator(LabelBuilder, LabelFormat);

	LabelBuilder.Append(TEXT("Head"));
	switch (HeadingAxis)
	{
	case EHeadingAxis::X:
		LabelBuilder.Append(TEXT("X"));
		break;
	case EHeadingAxis::Y:
		LabelBuilder.Append(TEXT("Y"));
		break;
	case EHeadingAxis::Z:
		LabelBuilder.Append(TEXT("Z"));
		break;
	}

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