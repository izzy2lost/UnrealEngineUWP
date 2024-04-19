// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshNodesPlugin.h"


#include "Dataflow/ChaosFleshAuthorSceneCollisionCandidatesNode.h"
#include "Dataflow/ChaosFleshBindingsNodes.h"
#include "Dataflow/ChaosFleshCollisionBodyConstraintNode.h"
#include "Dataflow/ChaosFleshCoreNodes.h"
#include "Dataflow/ChaosFleshCreateTetrahedronNode.h"
#include "Dataflow/ChaosFleshEngineAssetNodes.h"
#include "Dataflow/ChaosFleshFiberDirectionInitializationNodes.h"
#include "Dataflow/ChaosFleshImportGEO.h"
#include "Dataflow/ChaosFleshKinematicConstraintNode.h"
#include "Dataflow/ChaosFleshKinematicOriginInsertionInitializationNode.h"
#include "Dataflow/ChaosFleshKinematicTetrahedralConstraintNode.h"
#include "Dataflow/ChaosFleshRadialTetrahedronNodes.h"
#include "Dataflow/ChaosFleshRenderInitializationNodes.h"
#include "Dataflow/ChaosFleshPositionTargetInitializationNodes.h"
#include "Dataflow/ChaosFleshSkeletalBindingsNode.h"
#include "Dataflow/ChaosFleshTetrahedralNodes.h"
#include "Dataflow/ChaosFleshTriangleMeshSimulationPropertiesNode.h"
#include "Dataflow/ChaosFleshSkeletalMeshConstraintNode.h"
#include "Dataflow/ChaosFleshSkinSimulationPropertiesNode.h"
#include "Dataflow/ChaosFleshSetFleshDefaultPropertiesNode.h"
#include "Dataflow/ChaosFleshVertexConstraintNode.h"
#include "Dataflow/GeometryCollectionAppendCollectionTransformNode.h"
#include "Modules/ModuleManager.h"


#define LOCTEXT_NAMESPACE "ChaosFleshNodes"


void IChaosFleshNodesPlugin::StartupModule()
{
	Dataflow::ChaosFleshBindingsNodes();
	Dataflow::RegisterChaosFleshEngineAssetNodes();
	Dataflow::RegisterChaosFleshCoreNodes();
	Dataflow::ChaosFleshFiberDirectionInitializationNodes();
	Dataflow::ChaosFleshRenderInitializationNodes();
	Dataflow::RegisterChaosFleshPositionTargetInitializationNodes();
	Dataflow::ChaosFleshTetrahedralNodes();
	Dataflow::ChaosFleshSkeletalBindingsNode();
	Dataflow::ChaosFleshRadialTetrahedronNodes();
	Dataflow::RegisterChaosFleshImportGEONodes();
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
}

void IChaosFleshNodesPlugin::ShutdownModule()
{
}


IMPLEMENT_MODULE(IChaosFleshNodesPlugin, ChaosFleshNodes)


#undef LOCTEXT_NAMESPACE
