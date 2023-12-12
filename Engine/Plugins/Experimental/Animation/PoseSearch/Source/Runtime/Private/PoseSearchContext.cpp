// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchContext.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimationRuntime.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearch/PoseSearchTrajectoryTypes.h"
#include "PoseSearchFeatureChannel_Position.h"
#include "PoseSearchFeatureChannel_Heading.h"

namespace UE::PoseSearch
{
	
#if ENABLE_DRAW_DEBUG
//////////////////////////////////////////////////////////////////////////
// FDebugDrawParams
FDebugDrawParams::FDebugDrawParams(FAnimInstanceProxy* InAnimInstanceProxy, const FTransform& InRootMotionTransform, const UPoseSearchDatabase* InDatabase, EDebugDrawFlags InFlags)
: AnimInstanceProxy(InAnimInstanceProxy)
, World(nullptr)
, Mesh(nullptr)
, RootMotionTransform(&InRootMotionTransform)
, Database(InDatabase)
, Flags(InFlags)
{
}

FDebugDrawParams::FDebugDrawParams(const UWorld* InWorld, const USkinnedMeshComponent* InMesh, const FTransform& InRootMotionTransform, const UPoseSearchDatabase* InDatabase, EDebugDrawFlags InFlags)
: AnimInstanceProxy(nullptr)
, World(InWorld)
, Mesh(InMesh)
, RootMotionTransform(&InRootMotionTransform)
, Database(InDatabase)
, Flags(InFlags)
{
}

bool FDebugDrawParams::CanDraw() const
{
	return (AnimInstanceProxy || World) && Database && Database->Schema && Database->Schema->IsValid();
}

const FSearchIndex* FDebugDrawParams::GetSearchIndex() const
{
	return Database ? &Database->GetSearchIndex() : nullptr;
}

const UPoseSearchSchema* FDebugDrawParams::GetSchema() const
{
	return Database ? Database->Schema : nullptr;
}

FVector FDebugDrawParams::ExtractPosition(TConstArrayView<float> PoseVector, const UPoseSearchFeatureChannel_Position* Position) const
{
	check(Position);
	const FVector BonePosition = FFeatureVectorHelper::DecodeVector(PoseVector, Position->GetChannelDataOffset(), Position->ComponentStripping);
	const FVector WorldBonePosition = GetRootTransform().TransformPosition(BonePosition);
	return WorldBonePosition;
}

FVector FDebugDrawParams::ExtractPosition(TConstArrayView<float> PoseVector, float SampleTimeOffset, int8 SchemaBoneIdx, EPermutationTimeType PermutationTimeType, int32 SamplingAttributeId) const
{
	// we don't wanna ask for a SchemaOriginBoneIdx in the future or past
	check(PermutationTimeType != EPermutationTimeType::UsePermutationTime);
	if (const UPoseSearchSchema* Schema = GetSchema())
	{
		// looking for a UPoseSearchFeatureChannel_Position that matches the TimeOffset and SchemaBoneIdx,
		// with SchemaOriginBoneIdx to be the root bone and the appropriate PermutationTimeType 
		if (const UPoseSearchFeatureChannel_Position* FoundPosition = static_cast<const UPoseSearchFeatureChannel_Position*>(
			Schema->FindChannel([SampleTimeOffset, SchemaBoneIdx, PermutationTimeType, SamplingAttributeId, Schema](const UPoseSearchFeatureChannel* Channel) -> const UPoseSearchFeatureChannel_Position*
				{
					if (const UPoseSearchFeatureChannel_Position* Position = Cast<UPoseSearchFeatureChannel_Position>(Channel))
					{
						if (Position->SchemaBoneIdx == SchemaBoneIdx &&
							Position->SampleTimeOffset == SampleTimeOffset &&
							Position->PermutationTimeType == PermutationTimeType &&
							Position->SamplingAttributeId == SamplingAttributeId &&
							Position->SchemaOriginBoneIdx == RootSchemaBoneIdx)
						{
							return Position;
						}
					}
					return nullptr;
				})))
		{
			return ExtractPosition(PoseVector, FoundPosition);
		}

		if (Mesh && SchemaBoneIdx > RootSchemaBoneIdx)
		{
			return Mesh->GetSocketTransform(Schema->BoneReferences[SchemaBoneIdx].BoneName).GetTranslation();
		}
	}
	return GetRootTransform().GetTranslation();
}

FQuat FDebugDrawParams::ExtractRotation(TConstArrayView<float> PoseVector, float SampleTimeOffset, int8 SchemaBoneIdx) const
{
	if (const UPoseSearchSchema* Schema = GetSchema())
	{
		int32 HeadingAxisFoundNum = 0;
		const UPoseSearchFeatureChannel_Heading* FoundHeading[int32(EHeadingAxis::Num)];
		FVector DecodedHeading[int32(EHeadingAxis::Num)];
		for (int32 HeadingAxis = 0; HeadingAxis < int32(EHeadingAxis::Num); ++HeadingAxis)
		{
			// looking for a UPoseSearchFeatureChannel_Heading that matches the SampleTimeOffset, SchemaBoneIdx, and with OriginTimeOffset as zero.
			// the features data associated to this channel would be a heading vector in GetRootTransform space (since OriginTimeOffset is zero)), 
			// so by finding at least two with differnt axis we'll be able to compose a delta rotation from OriginTimeOffset (zero) to SampleTimeOffset
			FoundHeading[HeadingAxis] = static_cast<const UPoseSearchFeatureChannel_Heading*>(
				Schema->FindChannel([SampleTimeOffset, SchemaBoneIdx, HeadingAxis](const UPoseSearchFeatureChannel* Channel) -> const UPoseSearchFeatureChannel_Heading*
					{
						if (const UPoseSearchFeatureChannel_Heading* Heading = Cast<UPoseSearchFeatureChannel_Heading>(Channel))
						{
							if (Heading->OriginTimeOffset == 0.f &&
								Heading->SchemaBoneIdx == SchemaBoneIdx &&
								Heading->SampleTimeOffset == SampleTimeOffset &&
								int32(Heading->HeadingAxis) == HeadingAxis)
							{
								return Heading;
							}
						}
						return nullptr;
					}));
			if (FoundHeading[HeadingAxis])
			{
				DecodedHeading[HeadingAxis] = FFeatureVectorHelper::DecodeVector(PoseVector, FoundHeading[HeadingAxis]->GetChannelDataOffset(), FoundHeading[HeadingAxis]->ComponentStripping);

				++HeadingAxisFoundNum;
				if (HeadingAxisFoundNum == 2)
				{
					// we've found enough heading axis to compose a rotation
					break;
				}
			}
		}

		if (HeadingAxisFoundNum > 0)
		{
			bool bAbleToReconstructMissingAxis = true;
			if (HeadingAxisFoundNum == 2)
			{
				// reconstructing the missing axis
				if (!FoundHeading[int32(EHeadingAxis::X)])
				{
					DecodedHeading[int32(EHeadingAxis::X)] = FVector::CrossProduct(DecodedHeading[int32(EHeadingAxis::Y)], DecodedHeading[int32(EHeadingAxis::Z)]);
				}
				else if (!FoundHeading[int32(EHeadingAxis::Y)])
				{
					DecodedHeading[int32(EHeadingAxis::Y)] = FVector::CrossProduct(DecodedHeading[int32(EHeadingAxis::Z)], DecodedHeading[int32(EHeadingAxis::X)]);
				}
				else // if (!FoundHeading[int32(EHeadingAxis::Z)])
				{
					DecodedHeading[int32(EHeadingAxis::Z)] = FVector::CrossProduct(DecodedHeading[int32(EHeadingAxis::X)], DecodedHeading[int32(EHeadingAxis::Y)]);
				}
			}
			else 
			{
				check(HeadingAxisFoundNum == 1);
			
				// reconstructing the two missing axis
				if (FoundHeading[int32(EHeadingAxis::X)])
				{
					DecodedHeading[int32(EHeadingAxis::Y)] = FVector::CrossProduct(FVector::ZAxisVector, DecodedHeading[int32(EHeadingAxis::X)]);
					bAbleToReconstructMissingAxis &= DecodedHeading[int32(EHeadingAxis::Y)].Normalize();
					DecodedHeading[int32(EHeadingAxis::Z)] = FVector::CrossProduct(DecodedHeading[int32(EHeadingAxis::X)], DecodedHeading[int32(EHeadingAxis::Y)]);
				}
				else if (FoundHeading[int32(EHeadingAxis::Y)])
				{
					DecodedHeading[int32(EHeadingAxis::X)] = FVector::CrossProduct(DecodedHeading[int32(EHeadingAxis::Y)], FVector::ZAxisVector);
					bAbleToReconstructMissingAxis &= DecodedHeading[int32(EHeadingAxis::X)].Normalize();
					DecodedHeading[int32(EHeadingAxis::Z)] = FVector::CrossProduct(DecodedHeading[int32(EHeadingAxis::X)], DecodedHeading[int32(EHeadingAxis::Y)]);
				}
				else // if (FoundHeading[int32(EHeadingAxis::Z)])
				{
					DecodedHeading[int32(EHeadingAxis::X)] = FVector::CrossProduct(FVector::YAxisVector, DecodedHeading[int32(EHeadingAxis::Z)]);
					bAbleToReconstructMissingAxis &= DecodedHeading[int32(EHeadingAxis::X)].Normalize();
					DecodedHeading[int32(EHeadingAxis::Y)] = FVector::CrossProduct(DecodedHeading[int32(EHeadingAxis::Z)], DecodedHeading[int32(EHeadingAxis::X)]);
				}
			}

			if (bAbleToReconstructMissingAxis)
			{
				// RotMatrix is the rotation matrix from time zero (OriginTimeOffset) to time SampleTimeOffset, so by composing it with GetRootTransform().GetRotation(),
				// world rotation associated to the time zero, we can calcualte the world rotation at time SampleTimeOffset
				const FMatrix RotMatrix(DecodedHeading[int32(EHeadingAxis::X)], DecodedHeading[int32(EHeadingAxis::Y)], DecodedHeading[int32(EHeadingAxis::Z)], FVector::ZeroVector);
				const FQuat RotQuat(RotMatrix);
				const FQuat RotQuatWorld = RotQuat * GetRootTransform().GetRotation();
				return RotQuatWorld;
			}
		}

		if (Mesh && SchemaBoneIdx > RootSchemaBoneIdx)
		{
			return Mesh->GetSocketTransform(Schema->BoneReferences[SchemaBoneIdx].BoneName).GetRotation();
		}
	}

	return GetRootTransform().GetRotation();
}

const FTransform& FDebugDrawParams::GetRootTransform() const
{
	check(RootMotionTransform);
	return *RootMotionTransform;
}

void FDebugDrawParams::DrawLine(const FVector& LineStart, const FVector& LineEnd, const FColor& Color, float Thickness) const
{
	if (Color.A > 0)
	{
		if (AnimInstanceProxy)
		{
			AnimInstanceProxy->AnimDrawDebugLine(LineStart, LineEnd, Color, false, 0.f, Thickness, SDPG_Foreground);
		}
		else if (World)
		{
			DrawDebugLine(World, LineStart, LineEnd, Color, false, 0.f, SDPG_Foreground, Thickness);
		}
	}
}

void FDebugDrawParams::DrawPoint(const FVector& Position, const FColor& Color, float Thickness) const
{
	if (Color.A > 0)
	{
		if (AnimInstanceProxy)
		{
			AnimInstanceProxy->AnimDrawDebugPoint(Position, Thickness, Color, false, 0.f, SDPG_Foreground);
		}
		else if (World)
		{
			DrawDebugPoint(World, Position, Thickness, Color, false, 0.f, SDPG_Foreground);
		}
	}
}

void FDebugDrawParams::DrawCircle(const FMatrix& TransformMatrix, float Radius, int32 Segments, const FColor& Color, float Thickness) const
{
	if (Color.A > 0)
	{
		if (AnimInstanceProxy)
		{
			AnimInstanceProxy->AnimDrawDebugCircle(TransformMatrix.GetOrigin(), Radius, Segments, Color, TransformMatrix.GetScaledAxis(EAxis::X), false, 0.f, SDPG_Foreground, Thickness);
		}
		else if (World)
		{
			// @todo: use the DrawDebugCircle API with the up vector to communize with the AnimInstanceProxy call
			DrawDebugCircle(World, TransformMatrix, Radius, Segments, Color, false, 0.f, SDPG_Foreground, Thickness);
		}
	}
}

void FDebugDrawParams::DrawCentripetalCatmullRomSpline(TConstArrayView<FVector> Points, TConstArrayView<FColor> Colors, float Alpha, int32 NumSamplesPerSegment, float Thickness) const
{
	const int32 NumPoints = Points.Num();
	const int32 NumColors = Colors.Num();
	if (NumPoints > 1)
	{
		auto GetT = [](float T, float Alpha, const FVector& P0, const FVector& P1)
		{
			const FVector P1P0 = P1 - P0;
			const float Dot = P1P0 | P1P0;
			const float Pow = FMath::Pow(Dot, Alpha * .5f);
			return Pow + T;
		};

		auto LerpColor = [](FColor A, FColor B, float T) -> FColor
		{
			return FColor(
				FMath::RoundToInt(float(A.R) * (1.f - T) + float(B.R) * T),
				FMath::RoundToInt(float(A.G) * (1.f - T) + float(B.G) * T),
				FMath::RoundToInt(float(A.B) * (1.f - T) + float(B.B) * T),
				FMath::RoundToInt(float(A.A) * (1.f - T) + float(B.A) * T));
		};

		FVector PrevPoint = Points[0];
		for (int32 i = 0; i < NumPoints - 1; ++i)
		{
			const FVector& P0 = Points[FMath::Max(i - 1, 0)];
			const FVector& P1 = Points[i];
			const FVector& P2 = Points[i + 1];
			const FVector& P3 = Points[FMath::Min(i + 2, NumPoints - 1)];

			const float T0 = 0.0f;
			const float T1 = GetT(T0, Alpha, P0, P1);
			const float T2 = GetT(T1, Alpha, P1, P2);
			const float T3 = GetT(T2, Alpha, P2, P3);

			const float T1T0 = T1 - T0;
			const float T2T1 = T2 - T1;
			const float T3T2 = T3 - T2;
			const float T2T0 = T2 - T0;
			const float T3T1 = T3 - T1;

			const bool bIsNearlyZeroT1T0 = FMath::IsNearlyZero(T1T0, UE_KINDA_SMALL_NUMBER);
			const bool bIsNearlyZeroT2T1 = FMath::IsNearlyZero(T2T1, UE_KINDA_SMALL_NUMBER);
			const bool bIsNearlyZeroT3T2 = FMath::IsNearlyZero(T3T2, UE_KINDA_SMALL_NUMBER);
			const bool bIsNearlyZeroT2T0 = FMath::IsNearlyZero(T2T0, UE_KINDA_SMALL_NUMBER);
			const bool bIsNearlyZeroT3T1 = FMath::IsNearlyZero(T3T1, UE_KINDA_SMALL_NUMBER);

			const FColor Color1 = Colors[FMath::Min(i, NumColors - 1)];
			const FColor Color2 = Colors[FMath::Min(i + 1, NumColors - 1)];

			for (int32 SampleIndex = 1; SampleIndex < NumSamplesPerSegment; ++SampleIndex)
			{
				const float ParametricDistance = float(SampleIndex) / float(NumSamplesPerSegment - 1);

				const float T = FMath::Lerp(T1, T2, ParametricDistance);

				const FVector A1 = bIsNearlyZeroT1T0 ? P0 : (T1 - T) / T1T0 * P0 + (T - T0) / T1T0 * P1;
				const FVector A2 = bIsNearlyZeroT2T1 ? P1 : (T2 - T) / T2T1 * P1 + (T - T1) / T2T1 * P2;
				const FVector A3 = bIsNearlyZeroT3T2 ? P2 : (T3 - T) / T3T2 * P2 + (T - T2) / T3T2 * P3;
				const FVector B1 = bIsNearlyZeroT2T0 ? A1 : (T2 - T) / T2T0 * A1 + (T - T0) / T2T0 * A2;
				const FVector B2 = bIsNearlyZeroT3T1 ? A2 : (T3 - T) / T3T1 * A2 + (T - T1) / T3T1 * A3;
				const FVector Point = bIsNearlyZeroT2T1 ? B1 : (T2 - T) / T2T1 * B1 + (T - T1) / T2T1 * B2;

				DrawLine(PrevPoint, Point, LerpColor(Color1, Color2, ParametricDistance));

				PrevPoint = Point;
			}
		}
	}
}

void FDebugDrawParams::DrawFeatureVector(TConstArrayView<float> PoseVector)
{
	if (CanDraw())
	{
		const UPoseSearchSchema* Schema = GetSchema();
		check(Schema);

		if (PoseVector.Num() == Schema->SchemaCardinality)
		{
			for (const TObjectPtr<UPoseSearchFeatureChannel>& ChannelPtr : Schema->GetChannels())
			{
				ChannelPtr->DebugDraw(*this, PoseVector);
			}
		}
	}
}

void FDebugDrawParams::DrawFeatureVector(int32 PoseIdx)
{
	if (CanDraw())
	{
		DrawFeatureVector(GetSearchIndex()->GetPoseValuesSafe(PoseIdx));
	}
}
#endif // ENABLE_DRAW_DEBUG

//////////////////////////////////////////////////////////////////////////
// FCachedQuery
FCachedQuery::FCachedQuery(const UPoseSearchSchema* InSchema)
{
	check(InSchema && InSchema->IsValid());
	Schema = InSchema;
	Values.SetNumZeroed(Schema->SchemaCardinality);
}

//////////////////////////////////////////////////////////////////////////
// FSearchContext
FSearchContext::FSearchContext(const UAnimInstance* InAnimInstance, const IPoseHistory* InHistory, TConstArrayView<const UAnimationAsset*> InAnimationsToConsider,
		float InDesiredPermutationTimeOffset, const FPoseIndicesHistory* InPoseIndicesHistory,
		const FSearchResult& InCurrentResult, const FFloatInterval& InPoseJumpThresholdTime, bool bInUseCachedChannelData)
: AnimInstance(InAnimInstance)
, History(InHistory)
, AnimationsToConsider(InAnimationsToConsider)
, DesiredPermutationTimeOffset(InDesiredPermutationTimeOffset)
, PoseIndicesHistory(InPoseIndicesHistory)
, CurrentResult(InCurrentResult)
, PoseJumpThresholdTime(InPoseJumpThresholdTime)
, bUseCachedChannelData(bInUseCachedChannelData)
{
	check(AnimInstance);
	UpdateCurrentResultPoseVector();
}

void FSearchContext::UpdateCurrentResultPoseVector()
{
	if (CurrentResult.IsValid())
	{
		const FSearchIndex& SearchIndex = CurrentResult.Database->GetSearchIndex();
		if (SearchIndex.IsValuesEmpty())
		{
			const int32 NumDimensions = CurrentResult.Database->Schema->SchemaCardinality;
			CurrentResultPoseVectorData.AddUninitialized(NumDimensions);
			CurrentResultPoseVector = SearchIndex.GetReconstructedPoseValues(CurrentResult.PoseIdx, MakeArrayView(CurrentResultPoseVectorData.GetData() + NumDimensions, NumDimensions));
		}
		else
		{
			CurrentResultPoseVector = SearchIndex.GetPoseValues(CurrentResult.PoseIdx);
		}
	}
}

FQuat FSearchContext::GetSampleRotation(float SampleTimeOffset, float OriginTimeOffset, int8 SchemaSampleBoneIdx, int8 SchemaOriginBoneIdx, EPermutationTimeType PermutationTimeType, const FQuat* SampleBoneRotationWorldOverride)
{
	float PermutationSampleTimeOffset = 0.f;
	float PermutationOriginTimeOffset = 0.f;
	UPoseSearchFeatureChannel::GetPermutationTimeOffsets(PermutationTimeType, DesiredPermutationTimeOffset, PermutationSampleTimeOffset, PermutationOriginTimeOffset);

	const float SampleTime = SampleTimeOffset + PermutationSampleTimeOffset;
	const float OriginTime = OriginTimeOffset + PermutationOriginTimeOffset;

	return GetSampleRotationInternal(SampleTime, OriginTime, SchemaSampleBoneIdx, SchemaOriginBoneIdx, SampleBoneRotationWorldOverride);
}

FVector FSearchContext::GetSamplePosition(float SampleTimeOffset, float OriginTimeOffset, int8 SchemaSampleBoneIdx, int8 SchemaOriginBoneIdx, EPermutationTimeType PermutationTimeType, const FVector* SampleBonePositionWorldOverride)
{
	float PermutationSampleTimeOffset = 0.f;
	float PermutationOriginTimeOffset = 0.f;
	UPoseSearchFeatureChannel::GetPermutationTimeOffsets(PermutationTimeType, DesiredPermutationTimeOffset, PermutationSampleTimeOffset, PermutationOriginTimeOffset);

	const float SampleTime = SampleTimeOffset + PermutationSampleTimeOffset;
	const float OriginTime = OriginTimeOffset + PermutationOriginTimeOffset;
	return GetSamplePositionInternal(SampleTime, OriginTime, SchemaSampleBoneIdx, SchemaOriginBoneIdx, SampleBonePositionWorldOverride);
}

FVector FSearchContext::GetSampleVelocity(float SampleTimeOffset, float OriginTimeOffset, int8 SchemaSampleBoneIdx, int8 SchemaOriginBoneIdx, bool bUseCharacterSpaceVelocities, EPermutationTimeType PermutationTimeType, const FVector* SampleBoneVelocityWorldOverride)
{
	using namespace UE::PoseSearch;

	float PermutationSampleTimeOffset = 0.f;
	float PermutationOriginTimeOffset = 0.f;
	UPoseSearchFeatureChannel::GetPermutationTimeOffsets(PermutationTimeType, DesiredPermutationTimeOffset, PermutationSampleTimeOffset, PermutationOriginTimeOffset);

	const float SampleTime = SampleTimeOffset + PermutationSampleTimeOffset;
	const float OriginTime = OriginTimeOffset + PermutationOriginTimeOffset;

	if (SampleBoneVelocityWorldOverride)
	{
		const FTransform RootBoneTransform = GetWorldBoneTransformAtTime(OriginTime, RootSchemaBoneIdx);
		return RootBoneTransform.InverseTransformVector(*SampleBoneVelocityWorldOverride);
	}

	// calculating the local Position for the bone indexed by SchemaSampleBoneIdx
	const FVector PreviousTranslation = GetSamplePositionInternal(SampleTime - FiniteDelta, bUseCharacterSpaceVelocities ? OriginTime - FiniteDelta : OriginTime, SchemaSampleBoneIdx, SchemaOriginBoneIdx);
	const FVector CurrentTranslation = GetSamplePositionInternal(SampleTime, OriginTime, SchemaSampleBoneIdx, SchemaOriginBoneIdx);

	const FVector LinearVelocity = (CurrentTranslation - PreviousTranslation) / FiniteDelta;
	return LinearVelocity;
}

FTransform FSearchContext::GetWorldRootBoneTransformAtTime(float SampleTime) const
{
	check(!CachedQueries.IsEmpty());
	const UPoseSearchSchema* Schema = CachedQueries.Last().GetSchema();
	check(Schema);

	#if WITH_EDITOR
	if (!History)
	{
		UE_LOG(LogPoseSearch, Error, TEXT("FSearchContext::GetWorldRootBoneTransformAtTime - Couldn't search for world space root boneTransform by %s, because no IPoseHistory has been found!"), *Schema->GetName());
	}
	else
	#endif // WITH_EDITOR
	{
		FTransform WorldRootBoneTransform;
		if (ensure(History) && History->GetTransformAtTime(SampleTime, WorldRootBoneTransform, Schema->Skeleton, RootBoneIndexType, WorldSpaceIndexType))
		{
			return WorldRootBoneTransform;
		}
	}

	if (AnimInstance && AnimInstance->CurrentSkeleton)
	{
		const FTransform& RootBoneTransform = AnimInstance->CurrentSkeleton->GetReferenceSkeleton().GetRefBonePose()[RootSchemaBoneIdx];
		const FTransform& ComponentToWorldTransform = AnimInstance->GetSkelMeshComponent()->GetComponentTransform();
		return RootBoneTransform * ComponentToWorldTransform;
	}

	return FTransform::Identity;
}

FTransform FSearchContext::GetWorldBoneTransformAtTime(float SampleTime, int8 SchemaBoneIdx)
{
	// CachedQueries.Last is the query we're building 
	check(!CachedQueries.IsEmpty());
	const UPoseSearchSchema* Schema = CachedQueries.Last().GetSchema();
	check(Schema);

	const FBoneIndexType BoneIndexType = Schema->GetBoneIndexType(SchemaBoneIdx);
	if (const FCachedTransform<FTransform>* CachedTransform = CachedTransforms.Find(SampleTime, BoneIndexType))
	{
		return CachedTransform->Transform;
	}

	FTransform WorldBoneTransform;
	if (BoneIndexType == RootBoneIndexType)
	{
		// we already tried querying the CachedTransforms so, let's search in Trajectory
		WorldBoneTransform = GetWorldRootBoneTransformAtTime(SampleTime);
	}
	else // if (BoneIndexType != RootBoneIndexType)
	{
		// searching for RootBoneIndexType in CachedTransforms
		if (const FCachedTransform<FTransform>* CachedTransform = CachedTransforms.Find(SampleTime, RootBoneIndexType))
		{
			WorldBoneTransform = CachedTransform->Transform;
		}
		else
		{
			WorldBoneTransform = GetWorldRootBoneTransformAtTime(SampleTime);
		}

		// collecting the local bone transforms from the IPoseHistory
		#if WITH_EDITOR
		if (!History)
		{
			UE_LOG(LogPoseSearch, Error, TEXT("FSearchContext::GetWorldBoneTransformAtTime - Couldn't search for bones requested by %s, because no IPoseHistory has been found!"), *Schema->GetName());
		}
		else
		#endif // WITH_EDITOR
		{
			check(History);

			FTransform LocalBoneTransform;
			if (!History->GetTransformAtTime(SampleTime, LocalBoneTransform, Schema->Skeleton, BoneIndexType, RootBoneIndexType))
			{
				if (const USkeleton* Skeleton = Schema->Skeleton)
				{
					if (!History->IsEmpty())
					{
						UE_LOG(LogPoseSearch, Warning, TEXT("FSearchContext::GetWorldBoneTransformAtTime - Couldn't find BoneIndexType %d (%s) requested by %s"), BoneIndexType, *Skeleton->GetReferenceSkeleton().GetBoneName(BoneIndexType).ToString(), *Schema->GetName());
					}
				}
				else
				{
					UE_LOG(LogPoseSearch, Warning, TEXT("FSearchContext::GetWorldBoneTransformAtTime - Schema '%s' Skeleton is not properly set"), *Schema->GetName());
				}
			}

			WorldBoneTransform = LocalBoneTransform * WorldBoneTransform;
		}
	}

	CachedTransforms.Add(SampleTime, BoneIndexType, WorldBoneTransform);
	return WorldBoneTransform;
}

FVector FSearchContext::GetSamplePositionInternal(float SampleTime, float OriginTime, int8 SchemaSampleBoneIdx, int8 SchemaOriginBoneIdx, const FVector* SampleBonePositionWorldOverride)
{
	if (SampleBonePositionWorldOverride)
	{
		const FTransform RootBoneTransform = GetWorldBoneTransformAtTime(OriginTime, RootSchemaBoneIdx);
		if (SchemaOriginBoneIdx == RootSchemaBoneIdx)
		{
			return RootBoneTransform.InverseTransformPosition(*SampleBonePositionWorldOverride);
		}

		// @todo: validate this still works for when root bone is not Identity
		const FTransform OriginBoneTransform = GetWorldBoneTransformAtTime(OriginTime, SchemaOriginBoneIdx);
		const FVector DeltaBoneTranslation = *SampleBonePositionWorldOverride - OriginBoneTransform.GetTranslation();
		return RootBoneTransform.InverseTransformVector(DeltaBoneTranslation);
	}

	const FTransform RootBoneTransform = GetWorldBoneTransformAtTime(OriginTime, RootSchemaBoneIdx);
	const FTransform SampleBoneTransform = GetWorldBoneTransformAtTime(SampleTime, SchemaSampleBoneIdx);
	if (SchemaOriginBoneIdx == RootSchemaBoneIdx)
	{
		return RootBoneTransform.InverseTransformPosition(SampleBoneTransform.GetTranslation());
	}

	const FTransform OriginBoneTransform = GetWorldBoneTransformAtTime(OriginTime, SchemaOriginBoneIdx);
	const FVector DeltaBoneTranslation = SampleBoneTransform.GetTranslation() - OriginBoneTransform.GetTranslation();
	return RootBoneTransform.InverseTransformVector(DeltaBoneTranslation);
}

FQuat FSearchContext::GetSampleRotationInternal(float SampleTime, float OriginTime, int8 SchemaSampleBoneIdx, int8 SchemaOriginBoneIdx, const FQuat* SampleBoneRotationWorldOverride)
{
	if (SampleBoneRotationWorldOverride)
	{
		const FTransform RootBoneTransform = GetWorldBoneTransformAtTime(OriginTime, RootSchemaBoneIdx);
		if (SchemaOriginBoneIdx == RootSchemaBoneIdx)
		{
			return RootBoneTransform.InverseTransformRotation(*SampleBoneRotationWorldOverride);
		}

		const FTransform OriginBoneTransform = GetWorldBoneTransformAtTime(OriginTime, SchemaOriginBoneIdx);
		const FQuat DeltaBoneRotation = OriginBoneTransform.InverseTransformRotation(*SampleBoneRotationWorldOverride);
		return RootBoneTransform.InverseTransformRotation(DeltaBoneRotation);
	}

	const FTransform RootBoneTransform = GetWorldBoneTransformAtTime(OriginTime, RootSchemaBoneIdx);
	const FTransform SampleBoneTransform = GetWorldBoneTransformAtTime(SampleTime, SchemaSampleBoneIdx);
	return RootBoneTransform.InverseTransformRotation(SampleBoneTransform.GetRotation());
}

TArrayView<float> FSearchContext::EditFeatureVector()
{
	// CachedQueries.Last is the query we're building 
	check(!CachedQueries.IsEmpty());
	return CachedQueries.Last().EditValues();
}

const UPoseSearchFeatureChannel* FSearchContext::GetCachedChannelData(uint32 ChannelUniqueIdentifier, const UPoseSearchFeatureChannel* Channel, TConstArrayView<float>& CachedChannelData)
{
	// searching CachedChannels for the ChannelUniqueIdentifier as representation of Channel
	FCachedChannel& CachedChannel = CachedChannels.FindOrAdd(ChannelUniqueIdentifier);
	if (CachedChannel.Channel)
	{
		// we found CachedChannel.Channel, a channel from a different schema (CachedQueries[CachedChannel.CachedQueryIndex].GetSchema()) compatible with Channel.
		// let's collect the associated data to CachedChannel.Channel 
		CachedChannelData = CachedQueries[CachedChannel.CachedQueryIndex].GetValues().Slice(CachedChannel.Channel->GetChannelDataOffset(), CachedChannel.Channel->GetChannelCardinality());
		return CachedChannel.Channel;
	}
	
	// we couldn't find the cached channel, so let's add the pair ChannelUniqueIdentifier / Channel to CachedChannels.
	// the associated CachedQueries[CachedQueries.Num() - 1].GetValues() data will be filled up by the end of Channel BuildQuery
	CachedChannel.CachedQueryIndex = CachedQueries.Num() - 1;
	CachedChannel.Channel = Channel;
	
	CachedChannelData = TConstArrayView<float>();
	return nullptr;
}

void FSearchContext::ResetCurrentBestCost()
{
	CurrentBestTotalCost = MAX_flt;
}

void FSearchContext::UpdateCurrentBestCost(const FPoseSearchCost& PoseSearchCost)
{
	if (PoseSearchCost.GetTotalCost() < CurrentBestTotalCost)
	{
		CurrentBestTotalCost = PoseSearchCost.GetTotalCost();
	};
}

TConstArrayView<float> FSearchContext::GetCachedQuery(const UPoseSearchSchema* Schema) const
{
	if (const FCachedQuery* FoundCachedQuery = CachedQueries.FindByPredicate(
		[Schema](const FCachedQuery& CachedQuery)
		{
			return CachedQuery.GetSchema() == Schema;
		}))
	{
		return FoundCachedQuery->GetValues();
	}
	return TConstArrayView<float>();
}

TConstArrayView<float> FSearchContext::GetOrBuildQuery(const UPoseSearchSchema* Schema)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_GetOrBuildQuery);

	check(Schema && Schema->IsValid());
	if (const FCachedQuery* FoundCachedQuery = CachedQueries.FindByPredicate(
		[Schema](const FCachedQuery& CachedQuery)
		{
			return CachedQuery.GetSchema() == Schema;
		}))
	{
		return FoundCachedQuery->GetValues();
	}
	
	return Schema->BuildQuery(*this);
}

bool FSearchContext::IsCurrentResultFromDatabase(const UPoseSearchDatabase* Database) const
{
	return CurrentResult.IsValid() && CurrentResult.Database == Database;
}

bool FSearchContext::CanUseCurrentResult() const
{
	// CachedQueries.Last is the query we're building 
	check(!CachedQueries.IsEmpty());
	return CurrentResult.IsValid() && CurrentResult.Database->Schema == CachedQueries.Last().GetSchema();
}


} // namespace UE::PoseSearch