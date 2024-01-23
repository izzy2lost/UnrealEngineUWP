// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureGraph.h"
#include "Expressions/Output/TG_Expression_Output.h"
#include "TG_Graph.h"
#include "TG_GraphEvaluation.h"
#include "TG_CustomVersion.h"

#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"

#include "2D/TextureHelper.h"
#include "Expressions/Input/TG_Expression_Graph.h"
#include "FxMat/MaterialManager.h"
#include "Model/Mix/MixManager.h"
#include "Model/Mix/MixSettings.h"
#include "Model/Mix/MixUpdateCycle.h"
#include "Model/ModelObject.h"
#include "Model/Mix/ViewportSettings.h"
#include "Transform/Mix/T_InvalidateTiles.h"
#include "Transform/Mix/T_UpdateTargets.h"


bool UTextureGraph::CheckCyclicDependency(const UTextureGraph* InTextureGraph) const
{
	TArray<UTextureGraph*> DependentGraphs;
	GatherAllDependentGraphs(DependentGraphs);

	// if after exhausting all nodes and their dependent graphs recursively, we found our source graph, we have a dependency
	return DependentGraphs.ContainsByPredicate([&InTextureGraph](const UTextureGraph* CurrentTextureGraph)
	{
		const UPackage* Package = CurrentTextureGraph->GetPackage();  
		const UPackage* SecondPackage = InTextureGraph->GetPackage();  
		const bool bIsTransientPackage = Package->HasAnyFlags(RF_Transient) || Package == GetTransientPackage();
		return CurrentTextureGraph->GetOutermostObject() == InTextureGraph->GetOutermostObject() ||
				(Package == SecondPackage && !bIsTransientPackage);
	});
}
void UTextureGraph::GatherAllDependentGraphs(TArray<UTextureGraph*>& DependentGraphs) const
{
	Graph()->ForEachNodes(
			[&](const UTG_Node* Node, uint32 Index)
		{
				if (Node)
				{
					if (UTG_Expression_TextureGraph* TextureGraphExpr = Cast<UTG_Expression_TextureGraph>(Node->GetExpression()))
					{
						const auto OriginalAssetForGraphInExpression = TextureGraphExpr->TextureGraph;

						if (OriginalAssetForGraphInExpression != nullptr)
						{
							// save it in the list
							if(!DependentGraphs.Contains(OriginalAssetForGraphInExpression))
								DependentGraphs.Add(OriginalAssetForGraphInExpression);

							// recursively gather graphs for all GraphExpressions encountered
							OriginalAssetForGraphInExpression->GatherAllDependentGraphs(DependentGraphs);
						}
					}
				}
		});
}
bool UTextureGraph::IsDependent(const UTextureGraph* InTextureGraph) const
{
	
	// check if we're trying to assign our own TextureGraph to this expression
	
	if (this->GetOutermostObject() == InTextureGraph->GetOutermostObject())
	{
		return true;
	}
			
	// check for cyclic dependency
	if (CheckCyclicDependency(InTextureGraph))
	{
		return true;
	}

	// default to false (Not dependent)
	return false;
}

void UTextureGraph::Construct(FString InName)
{
	// On the first new texture script we set the engine in run mode
	TextureGraphEngine::SetRunEngine();

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
	TextureGraphEngine::SetRunEngine();

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
		else // the TG doesn't have any output? that's weird but could happen, let's simply create the material regardless targeting a name
		{
			Settings->GetViewportSettings().InitDefaultSettings("Output");
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

	TextureGraphEngine::GetMixManager()->InvalidateMix(this, InvalidateInfo.Details);
}

void UTextureGraph::InvalidateAll()
{
	FInvalidationDetails Details = FInvalidationDetails().All();
	Details.Mix = this;

	InvalidationFrameId = TextureGraphEngine::GetFrameId();

	TextureGraphEngine::GetMixManager()->InvalidateMix(this, Details);
}

void UTextureGraph::Log() const
{
	TextureGraph->Log();
}
