// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "SchematicGraphPanel/SchematicGraphTag.h"
#include "SchematicGraphPanel/SchematicGraphNode.h"

#define LOCTEXT_NAMESPACE "SchematicGraphTag"

const FSchematicGraphNode* FSchematicGraphTag::GetNode() const
{
	if(Node.IsValid())
	{
		return Node.Pin().Get();
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE

#endif