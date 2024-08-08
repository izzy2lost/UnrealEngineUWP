// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceLayout.h"

#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeLayoutBlocks.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


mu::Ptr<mu::NodeLayout> GenerateMutableSourceLayout(const UEdGraphPin * Pin, FMutableGraphGenerationContext& GenerationContext, bool bIgnoreLayoutWarning)
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

		bool bWasEmpty = false;
		Result = CreateMutableLayoutNode(GenerationContext, TypedNodeBlocks->Layout, bIgnoreLayoutWarning,bWasEmpty);
		if (bWasEmpty)
		{
			FString msg = "Layout without any block found. A grid sized block will be used instead.";
			GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node, EMessageSeverity::Warning);
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


mu::Ptr<mu::NodeLayout> CreateMutableLayoutNode(FMutableGraphGenerationContext& GenerationContext, const UCustomizableObjectLayout* UnrealLayout, bool bIgnoreLayoutWarnings, bool& bWasEmpty )
{
	bWasEmpty = false;
	mu::Ptr<mu::NodeLayout> LayoutNode = new mu::NodeLayout;

	LayoutNode->Size = UE::Math::TIntVector2<uint16>(UnrealLayout->GetGridSize().X, UnrealLayout->GetGridSize().Y);
	LayoutNode->MaxSize = UE::Math::TIntVector2<uint16>(UnrealLayout->GetMaxGridSize().X, UnrealLayout->GetMaxGridSize().Y);
	LayoutNode->Blocks.SetNum(UnrealLayout->Blocks.Num() ? UnrealLayout->Blocks.Num() : 1);

	mu::EPackStrategy PackStrategy = ConvertLayoutStrategy(UnrealLayout->GetPackingStrategy());
	LayoutNode->Strategy = PackStrategy;

	LayoutNode->ReductionMethod = (UnrealLayout->GetBlockReductionMethod() == ECustomizableObjectLayoutBlockReductionMethod::Halve ? mu::EReductionMethod::Halve : mu::EReductionMethod::Unitary);

	if (bIgnoreLayoutWarnings)
	{
		// Layout warnings can be safely ignored in this case. Vertices that do not belong to any layout block will be removed (Extend Materials only)
		LayoutNode->FirstLODToIgnoreWarnings = 0;
	}
	else
	{
		LayoutNode->FirstLODToIgnoreWarnings = UnrealLayout->GetIgnoreVertexLayoutWarnings() ? UnrealLayout->GetFirstLODToIgnoreWarnings() : -1;
	}

	if (UnrealLayout->Blocks.Num())
	{
		for (int BlockIndex = 0; BlockIndex < UnrealLayout->Blocks.Num(); ++BlockIndex)
		{
			LayoutNode->Blocks[BlockIndex] = ToMutable(GenerationContext, UnrealLayout->Blocks[BlockIndex]);
		}
	}
	else
	{
		bWasEmpty = true;
		LayoutNode->Blocks[0].Min = { 0,0 };
		LayoutNode->Blocks[0].Size = { uint16(UnrealLayout->GetGridSize().X), uint16(UnrealLayout->GetGridSize().Y) };
		LayoutNode->Blocks[0].Priority = 0;
		LayoutNode->Blocks[0].bReduceBothAxes = false;
		LayoutNode->Blocks[0].bReduceByTwo = false;
	}

	return LayoutNode;
}


mu::FSourceLayoutBlock ToMutable(FMutableGraphGenerationContext& GenerationContext, const FCustomizableObjectLayoutBlock& UnrealBlock)
{
	mu::FSourceLayoutBlock MutableBlock;

	MutableBlock.Min = { uint16(UnrealBlock.Min.X), uint16(UnrealBlock.Min.Y) };
	FIntPoint Size = UnrealBlock.Max - UnrealBlock.Min;
	MutableBlock.Size = { uint16(Size.X), uint16(Size.Y) };

	MutableBlock.Priority = UnrealBlock.Priority;
	MutableBlock.bReduceBothAxes = UnrealBlock.bReduceBothAxes;
	MutableBlock.bReduceByTwo = UnrealBlock.bReduceByTwo;

	if (UnrealBlock.Mask)
	{
		GenerationContext.AddParticipatingObject(*UnrealBlock.Mask);

		// In the editor the src data can be directly accessed
		mu::Ptr<mu::Image> MaskImage = new mu::Image();

		FMutableSourceTextureData Tex(*UnrealBlock.Mask);
		EUnrealToMutableConversionError Error = ConvertTextureUnrealSourceToMutable(MaskImage.get(), Tex, 0);
		if (Error != EUnrealToMutableConversionError::Success)
		{
			// This should never happen, so details are not necessary.
			UE_LOG(LogMutable, Warning, TEXT("Failed to convert layout block mask texture."));
		}
		else
		{
			MutableBlock.Mask = MaskImage;
		}
	}

	return MutableBlock;
}


#undef LOCTEXT_NAMESPACE

