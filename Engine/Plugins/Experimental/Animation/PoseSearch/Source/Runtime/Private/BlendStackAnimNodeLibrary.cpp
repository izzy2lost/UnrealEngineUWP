// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/BlendStackAnimNodeLibrary.h"
#include "PoseSearch/AnimNode_BlendStack.h"

FBlendStackAnimNodeReference UBlendStackAnimNodeLibrary::ConvertToBlendStackNode(const FAnimNodeReference& Node, EAnimNodeReferenceConversionResult& Result)
{
	return FAnimNodeReference::ConvertToType<FBlendStackAnimNodeReference>(Node, Result);
}

void UBlendStackAnimNodeLibrary::BlendTo(const FAnimUpdateContext& Context, 
										const FBlendStackAnimNodeReference& BlendStackNode, 
										UAnimationAsset* AnimationAsset,
										float AnimationTime,
										bool bLoop,
										bool bMirrored,
										float BlendTime,
										FVector BlendParameters,
										float WantedPlayRate)
{
	if (FAnimNode_BlendStack* BlendStackNodePtr = BlendStackNode.GetAnimNodePtr<FAnimNode_BlendStack>())
	{
		if (const FAnimationUpdateContext* AnimationUpdateContext = Context.GetContext())
		{
			if (AnimationAsset == nullptr)
			{
				UE_LOG(LogPoseSearch, Warning, TEXT("UBlendStackAnimNodeLibrary::BlendTo called with null animation asset."));
				return;
			}

			BlendStackNodePtr->BlendTo(
				*AnimationUpdateContext, 
				AnimationAsset, 
				AnimationTime, 
				bLoop,
				bMirrored,
				BlendStackNodePtr->MirrorDataTable,
				BlendTime,
				BlendStackNodePtr->RootBoneBlendTime,
				BlendStackNodePtr->BlendProfile,
				BlendStackNodePtr->BlendOption,
				BlendStackNodePtr->bUseInertialBlend,
				BlendParameters,
				WantedPlayRate);
		}
		else
		{
			UE_LOG(LogPoseSearch, Warning, TEXT("UBlendStackAnimNodeLibrary::BlendTo called with an invalid context."));
		}
	}
	else
	{
		UE_LOG(LogPoseSearch, Warning, TEXT("UBlendStackAnimNodeLibrary::BlendTo called with an invalid type."));
	}
}

void UBlendStackAnimNodeLibrary::ForceBlendNextUpdate(const FBlendStackAnimNodeReference& BlendStackNode)
{
	if (FAnimNode_BlendStack* BlendStackNodePtr = BlendStackNode.GetAnimNodePtr<FAnimNode_BlendStack>())
	{
		BlendStackNodePtr->ForceBlendNextUpdate();
	}
	else
	{
		UE_LOG(LogPoseSearch, Warning, TEXT("UBlendStackAnimNodeLibrary::ForceBlendNextUpdate called with an invalid type."));
	}
}