// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimGraph/AnimNode_AnimNextParameters.h"
#include "Param/ParamStack.h"
#include "Param/AnimNextParameterBlock.h"
#include "Context.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_AnimNextParameters)

FAnimNode_AnimNextParameters::FAnimNode_AnimNextParameters(const FAnimNode_AnimNextParameters& InOther)
	: Source(InOther.Source)
#if WITH_EDITORONLY_DATA
	, ParameterBlock(InOther.ParameterBlock)
#endif
	, PreviousParameterBlock(nullptr)
	, ParamLayerHandle()
	, PropertyBag()
{
}

FAnimNode_AnimNextParameters& FAnimNode_AnimNextParameters::operator=(const FAnimNode_AnimNextParameters& InOther)
{
	Source = InOther.Source;
#if WITH_EDITORONLY_DATA
	ParameterBlock = InOther.ParameterBlock;
#endif
	PreviousParameterBlock = nullptr;
	ParamLayerHandle.Invalidate();
	PropertyBag.Reset();
	return *this;
}

FAnimNode_AnimNextParameters::FAnimNode_AnimNextParameters(FAnimNode_AnimNextParameters&& InOther)
{
	Source = InOther.Source;
#if WITH_EDITORONLY_DATA
	ParameterBlock = InOther.ParameterBlock;
#endif
	PreviousParameterBlock = nullptr;
	ParamLayerHandle.Invalidate();
	PropertyBag.Reset();
}

FAnimNode_AnimNextParameters& FAnimNode_AnimNextParameters::operator=(FAnimNode_AnimNextParameters&& InOther)
{
	Source = InOther.Source;
#if WITH_EDITORONLY_DATA
	ParameterBlock = InOther.ParameterBlock;
#endif
	PreviousParameterBlock = nullptr;
	ParamLayerHandle.Invalidate();
	PropertyBag.Reset();
	return *this;
}

void FAnimNode_AnimNextParameters::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	Source.Initialize(Context);
}

void FAnimNode_AnimNextParameters::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	using namespace UE::AnimNext;

	GetEvaluateGraphExposedInputs().Execute(Context);

	UAnimNextParameterBlock* CurrentParameterBlock = GetParameterBlock();

	// Reconstruct param block's cached layer if required
	if (CurrentParameterBlock != PreviousParameterBlock || !ParamLayerHandle.IsValid())
	{
		ParamLayerHandle.Invalidate();

		if (CurrentParameterBlock)
		{
			PropertyBag = CurrentParameterBlock->PropertyBag;
			ParamLayerHandle = FParamStack::MakeMutableLayer(PropertyBag);
		}

		PreviousParameterBlock = CurrentParameterBlock;
	}

	FParamStack& ParamStack = FParamStack::Get();
	FParamStack::FPushedLayerHandle PushedLayerHandle;
	if (CurrentParameterBlock && ParamLayerHandle.IsValid())
	{
		PushedLayerHandle = ParamStack.PushLayer(ParamLayerHandle);
		UE::AnimNext::FContext AnimNextContext;
		CurrentParameterBlock->Run(AnimNextContext);
	}

	Source.Update(Context);

	if (PushedLayerHandle.IsValid())
	{
		ParamStack.PopLayer(PushedLayerHandle);
	}
}

void FAnimNode_AnimNextParameters::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Source.CacheBones(Context);
}

void FAnimNode_AnimNextParameters::Evaluate_AnyThread(FPoseContext& Output)
{
	Source.Evaluate(Output);
}

void FAnimNode_AnimNextParameters::GatherDebugData(FNodeDebugData& DebugData)
{
	DebugData.AddDebugItem(DebugData.GetNodeName(this));

	Source.GatherDebugData(DebugData);
}

UAnimNextParameterBlock* FAnimNode_AnimNextParameters::GetParameterBlock() const
{
	return GET_ANIM_NODE_DATA(TObjectPtr<UAnimNextParameterBlock>, ParameterBlock);
}