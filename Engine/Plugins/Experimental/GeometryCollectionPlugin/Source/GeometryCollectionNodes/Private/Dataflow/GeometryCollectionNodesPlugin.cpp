// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionNodesPlugin.h"

#include "CoreMinimal.h"

#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/GeometryCollectionNodes.h"
#include "Dataflow/Nodes/GeometryCollectionAssetNodes.h"
#include "Dataflow/GeometryCollectionProcessingNodes.h"
#include "Dataflow/GeometryCollectionSkeletalMeshToCollectionNode.h"
#include "Dataflow/GeometryCollectionSkeletonToCollectionNode.h"
#include "Dataflow/GeometryCollectionSelectionNodes.h"
#include "Dataflow/GeometryCollectionMeshNodes.h"
#include "Dataflow/GeometryCollectionClusteringNodes.h"
#include "Dataflow/GeometryCollectionFracturingNodes.h"
#include "Dataflow/GeometryCollectionEditNodes.h"
#include "Dataflow/GeometryCollectionUtilityNodes.h"
#include "Dataflow/GeometryCollectionMaterialNodes.h"
#include "Dataflow/GeometryCollectionFieldNodes.h"
#include "Dataflow/GeometryCollectionOverrideNodes.h"
#include "Dataflow/GeometryCollectionMakeNodes.h"
#include "Dataflow/GeometryCollectionMathNodes.h"
#include "Dataflow/GeometryCollectionConversionNodes.h"
#include "Dataflow/GeometryCollectionVerticesNodes.h"
#include "Dataflow/GeometryCollectionArrayNodes.h"
#include "Dataflow/GeometryCollectionDebugNodes.h"
#include "Dataflow/GeometryCollectionVertexScalarToVertexIndicesNode.h"
#include "Dataflow/GeometryCollectionTransferVertexScalarAttributeNode.h"
#include "Dataflow/GeometryCollectionTriangleBoundaryIndicesNode.h"

#define LOCTEXT_NAMESPACE "DataflowNodes"


void IGeometryCollectionNodesPlugin::StartupModule()
{
	Dataflow::GeometryCollectionEngineNodes();
	Dataflow::GeometryCollectionEngineAssetNodes();
	Dataflow::GeometryCollectionProcessingNodes();
	Dataflow::GeometryCollectionSelectionNodes();
	Dataflow::GeometryCollectionMeshNodes();
	Dataflow::GeometryCollectionClusteringNodes();
	Dataflow::GeometryCollectionFracturingNodes();
	Dataflow::GeometryCollectionEditNodes();
	Dataflow::GeometryCollectionUtilityNodes();
	Dataflow::GeometryCollectionMaterialNodes();
	Dataflow::GeometryCollectionFieldNodes();
	Dataflow::GeometryCollectionOverrideNodes();
	Dataflow::GeometryCollectionMakeNodes();
	Dataflow::GeometryCollectionMathNodes();
	Dataflow::GeometryCollectionConversionNodes();
	Dataflow::GeometryCollectionVerticesNodes();
	Dataflow::GeometryCollectionArrayNodes();
	Dataflow::GeometryCollectionDebugNodes();
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGeometryCollectionVertexScalarToVertexIndicesNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSkeletonToCollectionDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSkeletalMeshToCollectionDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGeometryCollectionTransferVertexScalarAttributeNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FTriangleBoundaryIndicesNode);
}

void IGeometryCollectionNodesPlugin::ShutdownModule()
{
}


IMPLEMENT_MODULE(IGeometryCollectionNodesPlugin, GeometryCollectionNodes)


#undef LOCTEXT_NAMESPACE
