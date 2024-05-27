// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeParentedMaterial.h"

#include "MuCOE/Nodes/CustomizableObjectNodeCopyMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"


UCustomizableObjectNodeMaterialBase* FCustomizableObjectNodeParentedMaterial::GetParentMaterialNode() const
{
	return Cast<UCustomizableObjectNodeMaterialBase>(GetParentNode());
}


TArray<UCustomizableObjectNodeMaterialBase*> FCustomizableObjectNodeParentedMaterial::GetPossibleParentMaterialNodes() const
{
	TArray<UCustomizableObjectNodeMaterialBase*> Result;
	
	const UCustomizableObjectNode& Node = GetNode();
	const int32 LOD = Node.GetLOD();

	if (LOD == -1)
	{
		return Result; // Early exit.
	}
	
	TArray<UCustomizableObjectNodeObject*> ParentObjectNodes = Node.GetParentObjectNodes(LOD);

	ECustomizableObjectAutomaticLODStrategy LODStrategy = ECustomizableObjectAutomaticLODStrategy::Inherited;

	// Iterate backwards, from the Root CO to the parent CO, to propagate the LODStrategy. 
	for (int32 Index = ParentObjectNodes.Num() - 1; Index > -1; --Index)
	{
		const UCustomizableObjectNodeObject* ParentObjectNode = ParentObjectNodes[Index];
		
		if (ParentObjectNode->AutoLODStrategy != ECustomizableObjectAutomaticLODStrategy::Inherited)
		{
			LODStrategy = ParentObjectNode->AutoLODStrategy;
		}
		
		// When using AutomaticFromMesh find all materials within range [0..LOD].
		int32 LODIndex = LODStrategy == ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh ? 0 : LOD;

		// If LODStrategy is set to AutomaticFromMesh, find MaterialNodes belonging to lower LODs.
		for (; LODIndex <= LOD; ++LODIndex)
		{
			const TArray<UCustomizableObjectNodeMaterialBase*> MaterialNodes = ParentObjectNode->GetMaterialNodes(LODIndex);
			Result.Append(MaterialNodes);
		}
	}
	
	return Result;
}


UCustomizableObjectNodeMaterialBase* FCustomizableObjectNodeParentedMaterial::GetParentMaterialNodeIfPath() const
{
	UCustomizableObjectNodeMaterialBase* ParentMaterialNode = GetParentMaterialNode();
	
	if (GetPossibleParentMaterialNodes().Contains(ParentMaterialNode)) // There is a path to the parent material
	{
		return ParentMaterialNode;
	}
	else
	{
		return nullptr;
	}
}


const UCustomizableObjectNode& FCustomizableObjectNodeParentedMaterial::GetNode() const
{
	return const_cast<FCustomizableObjectNodeParentedMaterial*>(this)->GetNode();
}
