// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimationWarpingLibrary.h"
#include "BoneControllers/AnimNode_OffsetRootBone.h"

FTransform UAnimationWarpingLibrary::GetOffsetRootTransform(const FAnimNodeReference& Node)
{
	FTransform Transform(FTransform::Identity);

	if (FAnimNode_OffsetRootBone* OffsetRoot = Node.GetAnimNodePtr<FAnimNode_OffsetRootBone>())
	{
		OffsetRoot->GetOffsetRootTransform(Transform);
	}
	
	return Transform;
}
