// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimGraph/AnimNode_AnimNextParameters.h"
#include "Param/ParamStack.h"
#include "Param/IAnimNextParameterSourceInterface.h"
#include "AnimGraphParamStackScope.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_AnimNextParameters)

FAnimNode_AnimNextParameters::FAnimNode_AnimNextParameters(const FAnimNode_AnimNextParameters& InOther)
	: Source(InOther.Source)
	, Parameters(InOther.Parameters)
	, PreviousParameters(nullptr)
	, ParamLayerHandle()
{
}

FAnimNode_AnimNextParameters& FAnimNode_AnimNextParameters::operator=(const FAnimNode_AnimNextParameters& InOther)
{
	Source = InOther.Source;
	Parameters = InOther.Parameters;
	PreviousParameters = nullptr;
	ParamLayerHandle.Invalidate();
	return *this;
}

FAnimNode_AnimNextParameters::FAnimNode_AnimNextParameters(FAnimNode_AnimNextParameters&& InOther) noexcept
{
	Source = InOther.Source;
	Parameters = InOther.Parameters;
	PreviousParameters = nullptr;
	ParamLayerHandle.Invalidate();
}

FAnimNode_AnimNextParameters& FAnimNode_AnimNextParameters::operator=(FAnimNode_AnimNextParameters&& InOther) noexcept
{
	Source = InOther.Source;
	Parameters = InOther.Parameters;
	PreviousParameters = nullptr;
	ParamLayerHandle.Invalidate();
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

	IAnimNextParameterSourceInterface* CurrentParameters = Parameters ? Parameters.GetInterface() : nullptr;

	// Reconstruct param block's cached layer if required
	if (CurrentParameters != PreviousParameters || !ParamLayerHandle.IsValid())
	{
		ParamLayerHandle.Invalidate();

		if (CurrentParameters)
		{
			ParamLayerHandle = CurrentParameters->CacheLayer();
		}

		PreviousParameters = CurrentParameters;
	}

	{
		FAnimGraphParamStackScope Scope(Context);

		FParamStack& ParamStack = FParamStack::Get();
		FParamStack::FPushedLayerHandle PushedLayerHandle;
		if (CurrentParameters && ParamLayerHandle.IsValid())
		{
			CurrentParameters->UpdateLayer(ParamLayerHandle);
			PushedLayerHandle = ParamStack.PushLayer(ParamLayerHandle);
		}

		Source.Update(Context);

		if (PushedLayerHandle.IsValid())
		{
			ParamStack.PopLayer(PushedLayerHandle);
		}
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
