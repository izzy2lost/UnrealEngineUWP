// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureGraph.h"
#include "Expressions/Output/TG_Expression_Output.h"
#include "TG_Graph.h"
#include "TG_GraphEvaluation.h"
#include "TG_CustomVersion.h"

#include "UObject/ObjectSaveContext.h"

#include "2D/TextureHelper.h"
#include "FxMat/MaterialManager.h"
#include "Model/Mix/MixManager.h"
#include "Model/Mix/MixSettings.h"
#include "Model/Mix/MixUpdateCycle.h"
#include "Model/ModelObject.h"
#include "Model/Mix/ViewportSettings.h"
#include "Transform/Mix/T_InvalidateTiles.h"
#include "Transform/Mix/T_UpdateTargets.h"

void UTextureGraph::Construct(FString InName)
{
	// On the first new texture script we set the engine in run mode
	MixerEngine::SetRunEngine();

	Settings = NewObject<UMixSettings>(this);
	bInvalidateTextures = false;
	
	TextureGraph = NewObject<UTG_Graph>(this, NAME_None,RF_Transactional);
	TextureGraph->Construct(InName);

	const UTG_Node* OutputNode = TextureGraph->CreateExpressionNode(UTG_Expression_Output::StaticClass());

	Settings->GetViewportSettings().InitDefaultSettings(OutputNode->GetNodeName());

	OutputSettingsSet = NewObject<UTG_OutputSettingsSet>(this, NAME_None, RF_Transactional);
	OutputSettingsSet->AddOutputSetting(this, OutputNode->GetNodeName(), Cast<UTG_Expression_Output>(OutputNode->GetExpression()));
}


void UTextureGraph::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FTG_CustomVersion::GUID);

	int32 Version = Ar.CustomVer(FTG_CustomVersion::GUID);

	UE_LOG(LogTextureGraph, Log, TEXT("%s TextureGraph: %s >>>> %s"),
		(Ar.IsSaving() ? TEXT("Saved") : TEXT("Loaded")),
		*GetName(),
		*FString::FromInt(Version));
}

void UTextureGraph::PostLoad()
{
	// On the first script load we set the engine in run mode as well
	MixerEngine::SetRunEngine();

	Super::PostLoad();
	bInvalidateTextures = false;

	// Output Settings Set must exist in case it wasn't saved properly
	if (!OutputSettingsSet)
	{
		OutputSettingsSet = NewObject<UTG_OutputSettingsSet>(this, NAME_None, RF_Transactional);
		TextureGraph->ForEachParams([&](const UTG_Pin* ParamPin, uint32 Index)
		{
			if (ParamPin->IsOutput())
			{
				UTG_Node* OutputNode = ParamPin->GetNodePtr();
				UTG_Expression_Output* OutputExpression = Cast<UTG_Expression_Output>(OutputNode->GetExpression());
				if (OutputExpression)
					OutputSettingsSet->AddOutputSetting(this, OutputNode->GetNodeName(), Cast<UTG_Expression_Output>(OutputNode->GetExpression()));
			}
		});
	}

	// Settings must exist in case it wasn't saved properly
	if (!Settings)
	{
		Settings = NewObject<UMixSettings>(this);
	}

	// Fallback to default material.
	if(!Settings->GetViewportSettings().Material)
	{
		FTG_Ids OutputIds = TextureGraph->GetOutputParamIds();
		if (!OutputIds.IsEmpty())
		{
			Settings->GetViewportSettings().InitDefaultSettings(TextureGraph->GetNode(OutputIds[0])->GetNodeName());
		}
	}
}

void UTextureGraph::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);
	UE_LOG(LogTextureGraph, Log, TEXT("PreSave Script: %s"), *GetName());
}

void UTextureGraph::Update(MixUpdateCyclePtr InCycle)
{
	// Graph Evaluate
	SceneTargetUpdatePtr Target = std::make_shared<MixTargetUpdate>(InCycle->GetMix(), 0);
	InCycle->AddTarget(Target);
	
	T_InvalidateTiles::Create(InCycle, 0);

	// Now Evaluate the Graph!
	FTG_EvaluationContext EvaluationContext;
	EvaluationContext.Cycle = InCycle;

	EvaluationContext.Cycle->PushMix(this);
	if (Graph()->Validate(InCycle))
		Graph()->Evaluate(&EvaluationContext);

	//TODO: fetch the outputs and assign textures to the target texture set here
	// if(Output.IsTexture() && Output.GetTexture())
	// {
	// 	InContext->Cycle->GetTarget(0)->GetLastRender().SetTexture(GetTitleName(), Output.GetTexture().RasterBlob);
	// }
	// else if (Output.IsColor())
	// {
	// 	BufferDescriptor DesiredDesc = T_FlatColorTexture::GetFlatColorDesc("OutputFlat");			
	// 	auto OutputFlatTexture = Source.GetTexture(InContext, FTG_Texture::GetBlack(), &DesiredDesc);
	// 	InContext->Cycle->GetTarget(0)->GetLastRender().SetTexture(GetTitleName(), OutputFlatTexture.RasterBlob);
	// }
	
	EvaluationContext.Cycle->PopMix();
	
	/// This will be the final result of the rendering
	T_UpdateTargets::Create(InCycle, 0, true);
}

void UTextureGraph::PostMeshLoad()
{
	FModelInvalidateInfo InvalidateInfo;
	Invalidate(InvalidateInfo);
}

void UTextureGraph::TriggerUpdate(bool Tweaking)
{
	FModelInvalidateInfo InvalidateInfo;

	InvalidateInfo.Details.All();
	InvalidateInfo.Details.bTweaking = Tweaking;
	InvalidateInfo.Details.Mix = this;

	MixerEngine::GetMixManager()->InvalidateMix(this, InvalidateInfo.Details);
}

void UTextureGraph::InvalidateAll()
{
	FInvalidationDetails Details = FInvalidationDetails().All();
	Details.Mix = this;

	InvalidationFrameId = MixerEngine::GetFrameId();

	MixerEngine::GetMixManager()->InvalidateMix(this, Details);
}

void UTextureGraph::Log() const
{
	TextureGraph->Log();
}
