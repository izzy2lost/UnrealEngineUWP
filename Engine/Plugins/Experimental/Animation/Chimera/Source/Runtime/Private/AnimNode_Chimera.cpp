// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chimera/AnimNode_Chimera.h"
#include "Animation/AnimInertializationSyncScope.h"
#include "Animation/AnimInstanceProxy.h"
#include "Chimera/ChimeraAsset.h"
#include "Chimera/ChimeraDefines.h"
#include "Chimera/ChimeraLibrary.h"
#include "PoseSearch/PoseHistoryProvider.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "HAL/IConsoleManager.h"

#if ENABLE_ANIM_DEBUG
TAutoConsoleVariable<bool> CVarAnimNodeChimeraDebug(TEXT("a.AnimNode.Chimera.Debug"), false, TEXT("Turn on visualization debugging for AnimNode Chimera"));
#endif // ENABLE_ANIM_DEBUG

void FAnimNode_Chimera::GatherDebugData(FNodeDebugData& DebugData)
{
	Super::GatherDebugData(DebugData);

	FString DebugLine = DebugData.GetNodeName(this);
#if ENABLE_ANIM_DEBUG
	static bool bActive = true;
	DebugLine += FString::Printf(TEXT("\n - Active: (%s)"), bActive);
#endif
	DebugData.AddDebugItem(DebugLine);
}

void FAnimNode_Chimera::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	Super::Initialize_AnyThread(Context);
	Source.Initialize(Context);
}

void FAnimNode_Chimera::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Super::CacheBones_AnyThread(Context);
	Source.CacheBones(Context);
}

void FAnimNode_Chimera::Reset()
{
	Super::Reset();
	BlendLerp = 0.f;
	TranslationWarpLerp = 0.f;
	RotationWarpLerp = 0.f;
	bWasInteracting = false;
}

void FAnimNode_Chimera::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	using namespace UE::PoseSearch;

	if (NeedsReset(Context))
	{
		Reset();
	}

	UpdateCounter.SynchronizeWith(Context.AnimInstanceProxy->GetUpdateCounter());

	GetEvaluateGraphExposedInputs().Execute(Context);

	bool bBlendToExecuted = ConditionalBlendTo(Context);

	bool bIsInteracting = false;
	const float DeltaTime = Context.GetDeltaTime();
	FPoseHistoryProvider* PoseHistoryProvider = Context.GetMessage<FPoseHistoryProvider>();
	if (!PoseHistoryProvider)
	{
		UE_LOG(LogChimera, Error, TEXT("FAnimNode_Chimera::Update_AnyThread couldn't find the FPoseHistoryProvider"));
	}
	else
	{
		check(Context.AnimInstanceProxy);
		const FChimeraBlueprintResult Result = UChimeraLibrary::ChimeraQuery(Availabilities, Context.AnimInstanceProxy->GetAnimInstanceObject(), PoseHistoryProvider->GetHistoryCollector(), bValidateResultAgainstAvailabilities);

		if (const UMultiAnimAsset* MultiAnimAsset = Cast<UMultiAnimAsset>(Result.SelectedAnimation))
		{
			check(Result.SelectedDatabase != nullptr);

			UAnimationAsset* RoledAnimationAsset = MultiAnimAsset->GetAnimationAsset(Result.Role);
			check(RoledAnimationAsset);

			bIsInteracting = true;

			bool bExecuteBlendTo = false;
			bool bUpdatePropertiesFromResult = false;
			if (!bWasInteracting || AnimPlayers.IsEmpty())
			{
				bExecuteBlendTo = true;
				bUpdatePropertiesFromResult = true;
			}
			else if (EvaluationMode == EChimeraEvaluationMode::ContinuousReselection)
			{
				const FBlendStackAnimPlayer& MainAnimPlayer = AnimPlayers[0];
				const UAnimationAsset* PlayingAnimationAsset = MainAnimPlayer.GetAnimationAsset();

				if (RoledAnimationAsset != PlayingAnimationAsset ||
					Result.bIsMirrored != MainAnimPlayer.GetMirror() ||
					Result.BlendParameters != MainAnimPlayer.GetBlendParameters() ||
					!Result.bIsContinuingPoseSearch)
				{
					bExecuteBlendTo = true;
				}

				bUpdatePropertiesFromResult = true;
			}
			else if (RoledAnimationAsset == AnimPlayers[0].GetAnimationAsset() && Result.bIsContinuingPoseSearch)
			{
				// we don't update FullAlignedActorRootBoneTransform since we're not planning to blend into the newly selected animation here
				bUpdatePropertiesFromResult = true;
			}

			if (bUpdatePropertiesFromResult)
			{
				FullAlignedActorRootBoneTransform = Result.FullAlignedActorRootBoneTransform;
				WantedPlayRate = Result.WantedPlayRate;
				BlendParameters = Result.BlendParameters;
			}

			if (bExecuteBlendTo)
			{
				const FPoseSearchRoledSkeleton* RoledSkeleton = Result.SelectedDatabase->Schema->GetRoledSkeleton(Result.Role);
				check(RoledSkeleton);

				BlendTo(Context, RoledAnimationAsset, Result.SelectedTime, Result.bLoop, Result.bIsMirrored, RoledSkeleton->MirrorDataTable.Get(),
					BlendTime, BlendProfile, BlendOption, bUseInertialBlend, BlendParameters, WantedPlayRate);

				bBlendToExecuted = true;
			}

		}
	}

	const bool bDidBlendToRequestAnInertialBlend = bBlendToExecuted && bUseInertialBlend;
	UE::Anim::TOptionalScopedGraphMessage<UE::Anim::FAnimInertializationSyncScope> InertializationSync(bDidBlendToRequestAnInertialBlend, Context);
	
	UpdatePlayRate(WantedPlayRate);
	UpdateBlendspaceParameters(BlendspaceUpdateMode, BlendParameters);

	// calculating the translation and rotation warp lerps, used to warp the root transform towards the last computed FullAlignedActorRootBoneTransform
	const float Sign = bIsInteracting ? 1.f : -1.f;
	if (BlendTime > UE_KINDA_SMALL_NUMBER)
	{
		BlendLerp = FMath::Clamp(BlendLerp + (Sign * DeltaTime / BlendTime), 0.f, 1.f);
	}
	if (InitialTranslationWarpTime > UE_KINDA_SMALL_NUMBER)
	{
		TranslationWarpLerp = FMath::Clamp(TranslationWarpLerp + (Sign * DeltaTime / InitialTranslationWarpTime), 0.f, 1.f);
	}
	if (InitialRotationWarpTime > UE_KINDA_SMALL_NUMBER)
	{
		RotationWarpLerp = FMath::Clamp(RotationWarpLerp + (Sign * DeltaTime / InitialRotationWarpTime), 0.f, 1.f);
	}

	// updating source
	const float SourceBlendLerp = 1.f - BlendLerp;
	FAnimationUpdateContext SourceContext = Context.FractionalWeightAndRootMotion(SourceBlendLerp, SourceBlendLerp);
	if (BlendLerp > UE_KINDA_SMALL_NUMBER)
	{
		SourceContext = SourceContext.AsInactive();
	}
	Source.Update(SourceContext);
	
	if (Source.GetLinkNode())
	{
		// updating blend stack
		if (BlendLerp < UE_KINDA_SMALL_NUMBER && DeltaTime > UE_KINDA_SMALL_NUMBER)
		{
			// resetting the blendstack if there's no BlendLerp weight to it
			Reset();
		}

		FAnimationUpdateContext BlendStackContext = Context.FractionalWeightAndRootMotion(BlendLerp, BlendLerp);

		// bypassing FAnimNode_BlendStack::UpdateAssetPlayer, since we overridden its behaviour
		FAnimNode_BlendStack_Standalone::UpdateAssetPlayer(BlendStackContext);

	}
	else
	{
		// bypassing FAnimNode_BlendStack::UpdateAssetPlayer, since we overridden its behaviour
		FAnimNode_BlendStack_Standalone::UpdateAssetPlayer(Context);
	}

#if ENABLE_ANIM_DEBUG
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(AnimationChannel))
	{
		TRACE_ANIM_NODE_VALUE(Context, *FString("WasInteracting"), bWasInteracting);
		TRACE_ANIM_NODE_VALUE(Context, *FString("IsInteracting"), bIsInteracting);
		TRACE_ANIM_NODE_VALUE(Context, *FString("BlendToExecuted"), bBlendToExecuted);
		TRACE_ANIM_NODE_VALUE(Context, *FString("BlendLerp"), BlendLerp);
		TRACE_ANIM_NODE_VALUE(Context, *FString("TranslationWarpLerp"), TranslationWarpLerp);
		TRACE_ANIM_NODE_VALUE(Context, *FString("RotationWarpLerp"), RotationWarpLerp);
	}
#endif // ENABLE_ANIM_DEBUG

	bWasInteracting |= bIsInteracting;
}

void FAnimNode_Chimera::Evaluate_AnyThread(FPoseContext& Output)
{
	check(Output.AnimInstanceProxy);

	// @todo: do we still need to evaluate Source if blendstack weight is 100%?
	// evaluating Source to get the base pose 
	Source.Evaluate(Output);

	if (!AnimPlayers.IsEmpty())
	{
		Super::Evaluate_AnyThread(Output);
	}

	if (TranslationWarpLerp > UE_KINDA_SMALL_NUMBER || RotationWarpLerp > UE_KINDA_SMALL_NUMBER)
	{
		const FTransform ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();
		const FTransform FullAlignedActorRootBoneLocalTransform = FullAlignedActorRootBoneTransform.GetRelativeTransform(ComponentTransform);

#if ENABLE_ANIM_DEBUG
		if (CVarAnimNodeChimeraDebug.GetValueOnAnyThread())
		{
			Output.AnimInstanceProxy->AnimDrawDebugCoordinateSystem(FullAlignedActorRootBoneTransform.GetLocation(), FullAlignedActorRootBoneTransform.Rotator(), 10.0f, false, 0.f, 0.f, SDPG_Foreground);
			Output.AnimInstanceProxy->AnimDrawDebugCoordinateSystem(ComponentTransform.GetLocation(), ComponentTransform.Rotator(), 10.0f, false, 0.f, 0.f, SDPG_Foreground);
		}
#endif // ENABLE_ANIM_DEBUG

		const FCompactPoseBoneIndex RootBoneIndex(0);
		FTransform& RootBoneTransform = Output.Pose[RootBoneIndex];
		RootBoneTransform.SetTranslation(FMath::Lerp(RootBoneTransform.GetTranslation(), FullAlignedActorRootBoneLocalTransform.GetTranslation(), TranslationWarpLerp));
		RootBoneTransform.SetRotation(FQuat::Slerp(RootBoneTransform.GetRotation(), FullAlignedActorRootBoneLocalTransform.GetRotation(), RotationWarpLerp));
	}
}
