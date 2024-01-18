// Copyright Epic Games, Inc. All Rights Reserved.
#include "Expressions/Input/TG_Expression_Graph.h"

#include "TG_Graph.h"
#include "TextureGraph.h"

//////////////////////////////////////////////////////////////////////////
//// Generic subgraph
//////////////////////////////////////////////////////////////////////////

void UTG_Expression_Graph::NotifyGraphChanged()
{
	UE_LOG(LogTextureGraph, Log, TEXT("Graph Expression Reset Signature."));

	// Signature is reset, notify the owning node / graph to update itself
	NotifySignatureChanged();
}

FTG_SignaturePtr UTG_Expression_Graph::BuildSignatureDynamically() const
{
	// Initialize the signature from the expression class itself 
	FTG_Signature::FInit SignatureInit = GetSignatureInitArgsFromClass();

	// Then add the signature from the embedded Graph if exist
	if (GetGraph())
	{
		// Append the Graph params signature 
		FTG_Arguments GraphArgs;
		GetGraph()->AppendParamsSignature(GraphArgs, InParamIds, OutParamIds);

		for (auto& Arg : GraphArgs)
		{
			if (Arg.IsInput() || Arg.IsSetting())
			{
				Arg.SetPersistentSelfVar();
			}
			SignatureInit.Arguments.Add(Arg);
		}
	}
	return MakeShared<FTG_Signature>(SignatureInit);
}

void UTG_Expression_Graph::SetupAndEvaluate(FTG_EvaluationContext* InContext)
{
	//Super::SetupAndEvaluate(InContext);
	if (InContext->bDoLog)
	{
		LogEvaluation(InContext);
	}
	Evaluate(InContext);
}

void UTG_Expression_Graph::Evaluate(FTG_EvaluationContext* InContext)
{
	auto Graph = GetGraph();

	if (Graph)
	{
		InContext->GraphDepth++;
		if (Graph->Validate(InContext->Cycle))
		{
			Graph->Evaluate(InContext);
		}
		else
		{
			auto ErrorType = static_cast<int32>(ETextureGraphErrorType::SUBGRAPH_INTERNAL_ERROR);
			const FString ErrorMsg = FString::Format(TEXT("Internal error in {0} node."), {GetTitleName().ToString()} );
			TextureGraphEngine::GetErrorReporter(InContext->Cycle->GetMix())->ReportError(ErrorType, ErrorMsg, GetParentNode());
		}
		
		InContext->GraphDepth--;	
	}
}

//////////////////////////////////////////////////////////////////////////
// TextureTextureGraph Expression
//////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
void UTG_Expression_TextureGraph::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// if Graph changes catch it first
	if (PropertyChangedEvent.GetPropertyName() == FName(TEXT("TextureGraph")))
	{
		UE_LOG(LogTextureGraph, Log, TEXT("TextureGraph Expression PostEditChangeProperty."));
		SetTextureGraphInternal(TextureGraph);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UTG_Expression_TextureGraph::Initialize()
{
	if (TextureGraph && !RuntimeGraph)
	{
		RuntimeGraph = DuplicateObject(TextureGraph->Graph(), this);
	}
}

void UTG_Expression_TextureGraph::SetTextureGraphInternal(UTextureGraph* InTextureGraph)
{
	Modify();
	
	if (!InTextureGraph)
	{
		TextureGraph = nullptr;
		RuntimeGraph = nullptr;
	}
	else
	{
		// This is a valid TextureGraph
		TextureGraph = InTextureGraph;
		RuntimeGraph = DuplicateObject(TextureGraph->Graph(), this);
	}

	// Graph is reset, notify the owning node / graph to update itself
	NotifyGraphChanged();
}

void UTG_Expression_TextureGraph::SetTextureGraph(UTextureGraph* InTextureGraph)
{
	// This is the public setter of the TextureGraph, 
	// This is NOT called if the TextureGraph is modified from the detail panel!!!
	// We catch that case in PostEditChangeProperty, which will call SetTextureGraphInternal 

	// If it is the same TextureGraph then avoid anymore work, we should be good to go
	if (InTextureGraph == TextureGraph)
	{
		return;
	}

	SetTextureGraphInternal(InTextureGraph);
}

UTG_Graph* UTG_Expression_TextureGraph::GetGraph() const
{
	return RuntimeGraph;
}

void UTG_Expression_TextureGraph::CopyVarGeneric(const FTG_Argument& Arg, FTG_Var* InVar, bool CopyVarToArg)
{
	// Look into the subgraph parameter pins for arguments of the TextureTextureGraph expression 
	if (GetGraph())
	{
		auto InnerParamId = GetGraph()->FindParamPinId(Arg.GetName());
		if (InnerParamId.IsValid())
		{
			auto InnerVar = GetGraph()->GetVar(InnerParamId);

			if (CopyVarToArg)
			{
				InVar->CopyTo(InnerVar);
			}
			else
			{
				InVar->CopyFrom(InnerVar);
			}
		}
	}
}


void UTG_Expression_TextureGraph::Evaluate(FTG_EvaluationContext* InContext)
{
	if(TextureGraph)
	{
		InContext->Cycle->PushMix(TextureGraph);
		Super::Evaluate(InContext);
		InContext->Cycle->PopMix();	
	}
}

bool UTG_Expression_TextureGraph::Validate(MixUpdateCyclePtr Cycle)
{
	// if TextureGraph is already in execution list then
	if(TextureGraph && Cycle->ContainsMix(TextureGraph))
	{
		auto ErrorType = static_cast<int32>(ETextureGraphErrorType::RECURSIVE_CALL);

		const FString ErrorMsg = FString::Format(TEXT("Recursive Texture Graph call found in '{0}' asset."), {Cycle->TopMix()->GetName()} );
		TextureGraphEngine::GetErrorReporter(Cycle->GetMix())->ReportError(ErrorType, ErrorMsg, GetParentNode());
		return false;
	}

	return true;
}

bool UTG_Expression_TextureGraph::CanHandleAsset(UObject* Asset)
{
	const UTextureGraph* TSTextureGraph = Cast<UTextureGraph>(Asset);
	
	return TSTextureGraph != nullptr;
}

void UTG_Expression_TextureGraph::SetAsset(UObject* Asset)
{
	if(UTextureGraph* TSAsset = Cast<UTextureGraph>(Asset); TSAsset != nullptr)
	{
		SetTextureGraphInternal(TSAsset);
	}
}
