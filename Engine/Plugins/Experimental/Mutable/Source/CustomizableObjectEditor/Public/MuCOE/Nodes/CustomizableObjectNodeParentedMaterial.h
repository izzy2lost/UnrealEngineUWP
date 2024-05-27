// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "MuCOE/Nodes/CustomizableObjectNodeParentedNode.h"

class UCustomizableObjectNode;
class UCustomizableObjectNodeMaterialBase;


/** Node Material specialization of ICustomizableObjectNodeParentedNode. */
class FCustomizableObjectNodeParentedMaterial : public ICustomizableObjectNodeParentedNode
{
public:
	// Own interface
	/** Return the parent material node. */
	UCustomizableObjectNodeMaterialBase* GetParentMaterialNode() const;
	
	/** Return all possible parent material nodes of the node. */
	TArray<UCustomizableObjectNodeMaterialBase*> GetPossibleParentMaterialNodes() const;

	/** Returns the parent material node if there exist a path to it. */
	UCustomizableObjectNodeMaterialBase* GetParentMaterialNodeIfPath() const;
	
protected:
	/** Return the node which this interface belongs to. */
	virtual UCustomizableObjectNode& GetNode() = 0;
	const UCustomizableObjectNode& GetNode() const;
};

