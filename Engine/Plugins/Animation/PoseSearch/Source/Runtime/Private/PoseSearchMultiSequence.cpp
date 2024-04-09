// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchMultiSequence.h"
#include "Animation/AnimSequenceBase.h"

bool UPoseSearchMultiSequence::IsLooping() const
{
	float CommonPlayLength = -1.f;
	for (const FPoseSearchMultiSequenceItem& Item : Items)
	{
		if (const UAnimSequenceBase* Sequence = Item.Sequence.Get())
		{
			if (!Sequence->bLoop)
			{
				return false;
			}

			if (CommonPlayLength < 0.f)
			{
				CommonPlayLength = Sequence->GetPlayLength();
			}
			else if (!FMath::IsNearlyEqual(CommonPlayLength, Sequence->GetPlayLength()))
			{
				return false;
			}
		}
	}
	return true;
}

bool UPoseSearchMultiSequence::HasRootMotion() const
{
	bool bHasAtLeastOneValidItem = false;
	bool bHasRootMotion = true;

	for (const FPoseSearchMultiSequenceItem& Item : Items)
	{
		if (const UAnimSequenceBase* Sequence = Item.Sequence.Get())
		{
			bHasRootMotion &= Sequence->HasRootMotion();
			bHasAtLeastOneValidItem = true;
		}
	}

	return bHasAtLeastOneValidItem && bHasRootMotion;
}

float UPoseSearchMultiSequence::GetPlayLength() const
{
	float MaxPlayLength = 0.f;
	for (const FPoseSearchMultiSequenceItem& Item : Items)
	{
		if (const UAnimSequenceBase* Sequence = Item.Sequence.Get())
		{
			MaxPlayLength = FMath::Max(MaxPlayLength, Sequence->GetPlayLength());
		}
	}
	return MaxPlayLength;
}

#if WITH_EDITOR
int32 UPoseSearchMultiSequence::GetFrameAtTime(float Time) const
{
	const UAnimSequenceBase* MaxPlayLengthAnim = nullptr;
	float MaxPlayLength = -1.f;
	for (const FPoseSearchMultiSequenceItem& Item : Items)
	{
		if (const UAnimSequenceBase* Sequence = Item.Sequence.Get())
		{
			const float PlayLength = Sequence->GetPlayLength();
			if (PlayLength > MaxPlayLength)
			{
				MaxPlayLength = PlayLength;
				MaxPlayLengthAnim = Sequence;
			}
		}
	}
	
	if (MaxPlayLengthAnim)
	{
		return MaxPlayLengthAnim->GetFrameAtTime(Time);
	}

	return 0;
}
#endif // WITH_EDITOR

UAnimSequenceBase* UPoseSearchMultiSequence::GetSequence(const UE::PoseSearch::FRole& Role) const
{
	for (const FPoseSearchMultiSequenceItem& Item : Items)
	{
		if (Item.Role == Role)
		{
			return Item.Sequence;
		}
	}
	return nullptr;
}

const FTransform& UPoseSearchMultiSequence::GetOrigin(const UE::PoseSearch::FRole& Role) const
{
	for (const FPoseSearchMultiSequenceItem& Item : Items)
	{
		if (Item.Role == Role)
		{
			return Item.Origin;
		}
	}
	return FTransform::Identity;
}