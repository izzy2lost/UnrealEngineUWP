// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSimulationPlugin.h"
#include "Dataflow/DataflowSimulationNodes.h"
#include "Dataflow/DataflowGraph.h"
#include "Dataflow/DataflowSimulationManager.h"
#include "CoreMinimal.h"

#define LOCTEXT_NAMESPACE "DataflowSimulation"

void IDataflowSimulationPlugin::StartupModule()
{
	Dataflow::RegisterDataflowSimulationNodes();

	Dataflow::RegisterNodeFilter(FDataflowSimulationNode::StaticType());
	Dataflow::RegisterNodeFilter(FDataflowInvalidNode::StaticType());
	Dataflow::RegisterNodeFilter(FDataflowExecutionNode::StaticType());

	UDataflowSimulationManager::OnStartup();
}

void IDataflowSimulationPlugin::ShutdownModule()
{
	UDataflowSimulationManager::OnShutdown();
}

IMPLEMENT_MODULE(IDataflowSimulationPlugin, DataflowSimulation)

#undef LOCTEXT_NAMESPACE
