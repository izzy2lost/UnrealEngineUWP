// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSource.h"

#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMesh.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceSurface.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceFloat.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/MutableUtils.h"
#include "MuCOE/Nodes/CustomizableObjectNodeComponentMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeComponentSwitch.h"
#include "MuCOE/Nodes/CustomizableObjectNodeComponentVariation.h"
#include "MuT/NodeComponent.h"
#include "MuT/NodeComponentNew.h"
#include "MuT/NodeComponentSwitch.h"
#include "MuT/NodeComponentVariation.h"
#include "MuT/NodeLOD.h"
#include "MuT/NodeSurfaceNew.h"
#include "MuT/NodeMeshConstant.h"

#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

mu::Ptr<mu::NodeComponent> GenerateMutableSourceComponent(const UEdGraphPin * Pin, FMutableGraphGenerationContext& GenerationContext)
{
	check(Pin)
	RETURN_ON_CYCLE(*Pin, GenerationContext)

	CheckNumOutputs(*Pin, GenerationContext);
	
	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());

	const FGeneratedKey Key(reinterpret_cast<void*>(&GenerateMutableSourceComponent), *Pin, *Node, GenerationContext, true);
	if (const FGeneratedData* Generated = GenerationContext.Generated.Find(Key))
	{
		return static_cast<mu::NodeComponent*>(Generated->Node.get());
	}
	
	mu::Ptr<mu::NodeComponent> Result;
	
	if (const UCustomizableObjectNodeComponentMesh* TypedComponentMesh = Cast<UCustomizableObjectNodeComponentMesh>(Node))
	{
		GenerationContext.CurrentMeshComponent = TypedComponentMesh->ComponentName;

		if (TypedComponentMesh->ComponentName.IsNone())
		{
			FString Msg = FString::Printf(TEXT("Invalid Component Name."));
			GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), TypedComponentMesh, EMessageSeverity::Warning);
			return nullptr;
		}

		if (!TypedComponentMesh->Mesh.IsValid())
		{
			FString Msg = FString::Printf(TEXT("No mesh set for component node."));
			GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), TypedComponentMesh, EMessageSeverity::Warning);
			return nullptr;
		}

		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(TypedComponentMesh->Mesh.TryLoad());
		if (!SkeletalMesh)
		{
			FString Msg = FString::Printf(TEXT("Only SkeletalMeshes are supported in this node, for now."));
			GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), TypedComponentMesh, EMessageSeverity::Warning);
			return nullptr;
		}

		// Create the referenced mesh node.
		mu::Ptr<mu::NodeMeshConstant> MeshNode;
		{
			MeshNode = new mu::NodeMeshConstant();

			FString MeshUniqueTags;
			constexpr bool bIsReference = true;
			TSoftClassPtr<UAnimInstance> AnimInstance;
			mu::Ptr<mu::Mesh> MutableMesh = GenerateMutableMesh(SkeletalMesh, AnimInstance, 0, 0, 0, 0, MeshUniqueTags, 
																GenerationContext, TypedComponentMesh, nullptr, bIsReference);

			MeshNode->SetValue(MutableMesh);
		}

		// Create the component node
		mu::Ptr<mu::NodeComponentNew> ComponentNode = new mu::NodeComponentNew;
		ComponentNode->Id = GenerationContext.NumMeshComponentsInRoot - 1 + GenerationContext.NumExplicitMeshComponents; // Last root component id + Num explicit components

		// Create a LOD for each pass-through mesh LOD.
		const FSkeletalMeshModel* Model = SkeletalMesh->GetImportedModel();
		int32 SkeletalMeshLODCount = Model->LODModels.Num();
		for (int32 LODIndex=0; LODIndex<SkeletalMeshLODCount; ++LODIndex)
		{
			mu::Ptr<mu::NodeLOD> LODNode = new mu::NodeLOD;
			ComponentNode->LODs.Add(LODNode);

			const FSkeletalMeshLODModel& LODModel = Model->LODModels[LODIndex];
			int32 SectionCount = LODModel.Sections.Num();
			for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
			{
				// Is there a pin in the unreal node for this section?
				if (UEdGraphPin* InMaterialPin = TypedComponentMesh->GetMaterialPin(LODIndex,SectionIndex))
				{
					if (UEdGraphPin* ConnectedMaterialPin = FollowInputPin(*InMaterialPin))
					{
						GenerationContext.ComponentMeshOverride = MeshNode;
					
						mu::Ptr<mu::NodeSurface> SurfaceNode = GenerateMutableSourceSurface(ConnectedMaterialPin, GenerationContext);
						LODNode->Surfaces.Add(SurfaceNode);

						GenerationContext.ComponentMeshOverride = nullptr;
					}
					else
					{
						// Add an empty surface node anyway.
						mu::Ptr<mu::NodeSurfaceNew> SurfaceNode = new mu::NodeSurfaceNew;
						SurfaceNode->Mesh = MeshNode;
						LODNode->Surfaces.Add(SurfaceNode);
					}
				}
			}
		}

		GenerationContext.CurrentMeshComponent = FName();
		Result = ComponentNode;
	}


	else if (const UCustomizableObjectNodeComponentSwitch* TypedNodeSwitch = Cast<UCustomizableObjectNodeComponentSwitch>(Node))
	{
		// Using a lambda so control flow is easier to manage.
		Result = [&]()
			{
				const UEdGraphPin* SwitchParameter = TypedNodeSwitch->SwitchParameter();

				// Check Switch Parameter arity preconditions.
				if (const UEdGraphPin* EnumPin = FollowInputPin(*SwitchParameter))
				{
					mu::Ptr<mu::NodeScalar> SwitchParam = GenerateMutableSourceFloat(EnumPin, GenerationContext);

					// Switch Param not generated
					if (!SwitchParam)
					{
						// Warn about a failure.
						if (EnumPin)
						{
							const FText Message = LOCTEXT("FailedToGenerateSwitchParam", "Could not generate switch enum parameter. Please refesh the switch node and connect an enum.");
							GenerationContext.Compiler->CompilerLog(Message, Node);
						}

						return Result;
					}

					if (SwitchParam->GetType() != mu::NodeScalarEnumParameter::GetStaticType())
					{
						const FText Message = LOCTEXT("WrongSwitchParamType", "Switch parameter of incorrect type.");
						GenerationContext.Compiler->CompilerLog(Message, Node);

						return Result;
					}

					const int32 NumSwitchOptions = TypedNodeSwitch->GetNumElements();

					mu::NodeScalarEnumParameter* EnumParameter = static_cast<mu::NodeScalarEnumParameter*>(SwitchParam.get());
					if (NumSwitchOptions != EnumParameter->GetValueCount())
					{
						const FText Message = LOCTEXT("MismatchedSwitch", "Switch enum and switch node have different number of options. Please refresh the switch node to make sure the outcomes are labeled properly.");
						GenerationContext.Compiler->CompilerLog(Message, Node);
					}

					mu::Ptr<mu::NodeComponentSwitch> SwitchNode = new mu::NodeComponentSwitch;
					SwitchNode->Parameter = SwitchParam;
					SwitchNode->Options.SetNum(NumSwitchOptions);

					for (int32 SelectorIndex = 0; SelectorIndex < NumSwitchOptions; ++SelectorIndex)
					{
						if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeSwitch->GetElementPin(SelectorIndex)))
						{
							mu::Ptr<mu::NodeComponent> ChildNode = GenerateMutableSourceComponent(ConnectedPin, GenerationContext);
							if (ChildNode)
							{
								SwitchNode->Options[SelectorIndex] = ChildNode;
							}
							else
							{
								// Probably ok
							}
						}
					}

					Result = SwitchNode;
					return Result;
				}
				else
				{
					GenerationContext.Compiler->CompilerLog(LOCTEXT("NoEnumParamInSwitch", "Switch nodes must have an enum switch parameter. Please connect an enum and refesh the switch node."), Node);
					return Result;
				}
			}(); // invoke lambda.
	}

	else if (const UCustomizableObjectNodeComponentVariation* TypedNodeVar = Cast<UCustomizableObjectNodeComponentVariation>(Node))
	{
		mu::Ptr<mu::NodeComponentVariation> SurfNode = new mu::NodeComponentVariation();
		Result = SurfNode;

		for (const UEdGraphPin* ConnectedPin : FollowInputPinArray(*TypedNodeVar->DefaultPin()))
		{
			mu::Ptr<mu::NodeComponent> ChildNode = GenerateMutableSourceComponent(ConnectedPin, GenerationContext);
			if (ChildNode)
			{
				SurfNode->DefaultComponent = ChildNode;
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("ComponentFailed", "Component generation failed."), Node);
			}
		}

		const int32 NumVariations = TypedNodeVar->GetNumVariations();
		SurfNode->Variations.SetNum(NumVariations);
		for (int VariationIndex = 0; VariationIndex < NumVariations; ++VariationIndex)
		{
			mu::NodeSurfacePtr VariationSurfaceNode;

			if (UEdGraphPin* VariationPin = TypedNodeVar->VariationPin(VariationIndex))
			{
				SurfNode->Variations[VariationIndex].Tag = TypedNodeVar->GetVariation(VariationIndex).Tag;
				for (const UEdGraphPin* ConnectedPin : FollowInputPinArray(*VariationPin))
				{
					// Is it a modifier?
					mu::Ptr<mu::NodeComponent> ChildNode = GenerateMutableSourceComponent(ConnectedPin, GenerationContext);
					if (ChildNode)
					{
						SurfNode->Variations[VariationIndex].Component = ChildNode;
					}
					else
					{
						GenerationContext.Compiler->CompilerLog(LOCTEXT("ComponentFailed", "Component generation failed."), Node);
					}
				}
			}
		}
	}

	else
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), Node);
		ensure(false);
	}

	GenerationContext.Generated.Add(Key, FGeneratedData(Node, Result));
	GenerationContext.GeneratedNodes.Add(Node);
	
	return Result;
}

#undef LOCTEXT_NAMESPACE

