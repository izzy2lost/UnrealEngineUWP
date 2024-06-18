// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceLayout.h"

#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeLayoutBlocks.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


mu::Ptr<mu::NodeLayout> GenerateMutableSourceLayout(const UEdGraphPin * Pin, FMutableGraphGenerationContext & GenerationContext, bool bLinkedToExtendMaterial)
{
	check(Pin)
	RETURN_ON_CYCLE(*Pin, GenerationContext)

	CheckNumOutputs(*Pin, GenerationContext);

	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());

	const FGeneratedKey Key(reinterpret_cast<void*>(&GenerateMutableSourceLayout), *Pin, *Node, GenerationContext, true);
	if (const FGeneratedData* Generated = GenerationContext.Generated.Find(Key))
	{
		return static_cast<mu::NodeLayout*>(Generated->Node.get());
	}

	mu::Ptr<mu::NodeLayout> Result;
	
	if (const UCustomizableObjectNodeLayoutBlocks* TypedNodeBlocks = Cast<UCustomizableObjectNodeLayoutBlocks>(Node))
	{
		if (UCustomizableObjectNodeSkeletalMesh* SkeletalMeshNode = Cast<UCustomizableObjectNodeSkeletalMesh>(FollowOutputPin(*TypedNodeBlocks->OutputPin())->GetOwningNode()))
		{
			int32 LayoutIndex;
			FString MaterialName;

			if (!SkeletalMeshNode->CheckIsValidLayout(Pin, LayoutIndex, MaterialName))
			{
				FString msg = "Layouts ";
				for (int32 i = 0; i < LayoutIndex; ++i)
				{
					msg += "UV" + FString::FromInt(i);
					if (i < LayoutIndex - 1)
					{
						msg += ", ";
					}
				}
				msg += " of " + MaterialName + " must be also connected to a Layout Blocks Node. ";
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node, EMessageSeverity::Error);
				return nullptr;
			}
		}

		mu::Ptr<mu::NodeLayout> LayoutNode = new mu::NodeLayout;
		Result = LayoutNode;

		LayoutNode->Layout->SetGridSize(TypedNodeBlocks->Layout->GetGridSize().X, TypedNodeBlocks->Layout->GetGridSize().Y);
		LayoutNode->Layout->SetMaxGridSize(TypedNodeBlocks->Layout->GetMaxGridSize().X, TypedNodeBlocks->Layout->GetMaxGridSize().Y);
		LayoutNode->Layout->SetBlockCount(TypedNodeBlocks->Layout->Blocks.Num() ? TypedNodeBlocks->Layout->Blocks.Num() : 1);

		mu::EPackStrategy PackStrategy = ConvertLayoutStrategy(TypedNodeBlocks->Layout->GetPackingStrategy());
		LayoutNode->Layout->SetLayoutPackingStrategy(PackStrategy);

		LayoutNode->Layout->ReductionMethod = (TypedNodeBlocks->Layout->GetBlockReductionMethod() == ECustomizableObjectLayoutBlockReductionMethod::Halve ? mu::EReductionMethod::HALVE_REDUCTION : mu::EReductionMethod::UNITARY_REDUCTION);

		if (bLinkedToExtendMaterial)
		{
			// Layout warnings can be safely ignored in this case. Vertices that do not belong to any layout block will be removed (Extend Materials only)
			LayoutNode->FirstLODToIgnoreWarnings = 0;
		}
		else
		{
			LayoutNode->FirstLODToIgnoreWarnings = TypedNodeBlocks->Layout->GetIgnoreVertexLayoutWarnings() ? TypedNodeBlocks->Layout->GetFirstLODToIgnoreWarnings() : -1;
		}

		if (TypedNodeBlocks->Layout->Blocks.Num())
		{
			for (int BlockIndex = 0; BlockIndex < TypedNodeBlocks->Layout->Blocks.Num(); ++BlockIndex)
			{
				LayoutNode->Layout->Blocks[BlockIndex]= ToMutable(TypedNodeBlocks->Layout->Blocks[BlockIndex]);
			}
		}
		else
		{
			FString msg = "Layout without any block found. A grid sized block will be used instead.";
			GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node, EMessageSeverity::Warning);

			LayoutNode->Layout->Blocks[0].Min = { 0,0 };
			LayoutNode->Layout->Blocks[0].Size = { uint16(TypedNodeBlocks->Layout->GetGridSize().X), uint16(TypedNodeBlocks->Layout->GetGridSize().Y) };
			LayoutNode->Layout->Blocks[0].Priority = 0;
			LayoutNode->Layout->Blocks[0].bReduceBothAxes = false;
			LayoutNode->Layout->Blocks[0].bReduceByTwo = false;
		}
	}
	
	else
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), Node);
	}

	GenerationContext.Generated.Add(Key, FGeneratedData(Node, Result));
	GenerationContext.GeneratedNodes.Add(Node);

	
	if (Result)
	{
		Result->SetMessageContext(Node);
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE

