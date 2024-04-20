// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshNodesPlugin.h"


#include "Dataflow/ChaosFleshAuthorSceneCollisionCandidatesNode.h"
#include "Dataflow/ChaosFleshCalculateTetrehedralMetricsNode.h"
#include "Dataflow/ChaosFleshCollisionBodyConstraintNode.h"
#include "Dataflow/ChaosFleshCreateTetrahedronNode.h"
#include "Dataflow/ChaosFleshEngineAssetNodes.h"
#include "Dataflow/ChaosFleshImportGEO.h"
#include "Dataflow/ChaosFleshKinematicConstraintNode.h"
#include "Dataflow/ChaosFleshKinematicOriginInsertionInitializationNode.h"
#include "Dataflow/ChaosFleshKinematicTetrahedralConstraintNode.h"
#include "Dataflow/ChaosFleshRadialTetrahedronNode.h"
#include "Dataflow/ChaosFleshPositionTargetInitializationNodes.h"
#include "Dataflow/ChaosFleshTriangleMeshSimulationPropertiesNode.h"
#include "Dataflow/ChaosFleshSkeletalMeshConstraintNode.h"
#include "Dataflow/ChaosFleshSkinSimulationPropertiesNode.h"
#include "Dataflow/ChaosFleshSetFleshDefaultPropertiesNode.h"
#include "Dataflow/ChaosFleshVertexConstraintNode.h"
#include "Dataflow/GeometryCollectionAppendCollectionTransformNode.h"
#include "Modules/ModuleManager.h"

#include "Dataflow/ChaosFleshAppendTetrahedralCollectionNode.h"
#include "Dataflow/ChaosFleshBindForRenderToSkeletalMeshNode.h"
#include "Dataflow/ChaosFleshGenerateSkeletalBindingsNode.h"
#include "Dataflow/ChaosFleshGenerateSurfaceBindingsNode.h"
#include "Dataflow/ChaosFleshGenerateFiberDirectionsNode.h"


#define LOCTEXT_NAMESPACE "ChaosFleshNodes"


void IChaosFleshNodesPlugin::StartupModule()
{
	Dataflow::RegisterChaosFleshEngineAssetNodes();
	Dataflow::RegisterChaosFleshPositionTargetInitializationNodes();
	Dataflow::RegisterChaosFleshImportGEONodes();
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FAppendTetrahedralCollectionDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBindForRenderToSkeletalMeshDataflowNode); // todo delete
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGenerateFiberDirectionsDataflowNode); // todo delete
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGenerateSkeletalBindings);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGenerateSurfaceBindings);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCreateTetrahedronDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FTriangleMeshSimulationPropertiesDataflowNodes);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSkinSimulationPropertiesDataflowNodes);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetFleshDefaultPropertiesNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FKinematicSkeletalMeshInitializationDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FKinematicBodySetupInitializationDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FKinematicInitializationDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FKinematicOriginInsertionInitializationDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FKinematicTetrahedralBindingsDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSetVerticesKinematicDataflowNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FAuthorSceneCollisionCandidates);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FAppendToCollectionTransformAttributeDataflowNode); 
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCalculateTetMetrics);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRadialTetrahedronDataflowNodes);
}

void IChaosFleshNodesPlugin::ShutdownModule()
{
}


IMPLEMENT_MODULE(IChaosFleshNodesPlugin, ChaosFleshNodes)


#undef LOCTEXT_NAMESPACE
