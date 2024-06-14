// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSource.h"

#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMesh.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceSurface.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/MutableUtils.h"
#include "MuCOE/Nodes/CustomizableObjectNodeComponentMesh.h"
#include "MuT/NodeComponent.h"
#include "MuT/NodeComponentNew.h"
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
			mu::Ptr<mu::Mesh> MutableMesh = GenerateMutableMesh(SkeletalMesh, AnimInstance, 0, 0, 0, 0, MeshUniqueTags, GenerationContext, TypedComponentMesh, bIsReference);

			MeshNode->SetValue(MutableMesh);
		}

		// Create the component node
		mu::Ptr<mu::NodeComponentNew> ComponentNode = new mu::NodeComponentNew;
		ComponentNode->Id = GenerationContext.CurrentMeshComponent;

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
				}
			}
		}

		Result = ComponentNode;
	}

	else
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), Node);
	}

	GenerationContext.Generated.Add(Key, FGeneratedData(Node, Result));
	GenerationContext.GeneratedNodes.Add(Node);
	
	return Result;
}

#undef LOCTEXT_NAMESPACE

