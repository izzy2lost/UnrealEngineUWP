// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionDeprecatedNodesPlugin.h"

#include "CoreMinimal.h"

#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/SetVertexColorFromFloatArrayDepNode.h"
#include "Dataflow/SetVertexColorFromVertexSelectionDepNode.h"



#define LOCTEXT_NAMESPACE "DataflowNodes"


void IGeometryCollectionDeprecatedNodesPlugin::StartupModule()
{
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetVertexColorInCollectionFromVertexSelectionDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetVertexColorInCollectionFromFloatArrayDataflowNode);
}

void IGeometryCollectionDeprecatedNodesPlugin::ShutdownModule()
{
}


IMPLEMENT_MODULE(IGeometryCollectionDeprecatedNodesPlugin, GeometryCollectionDeprecatedNodes)


#undef LOCTEXT_NAMESPACE
