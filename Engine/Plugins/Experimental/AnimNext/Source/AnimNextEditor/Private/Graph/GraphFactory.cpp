// Copyright Epic Games, Inc. All Rights Reserved.

#include "GraphFactory.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraph_EditorData.h"
#include "Graph/AnimNextGraph_EdGraph.h"
#include "UncookedOnlyUtils.h"
#include "Graph/RigUnit_AnimNextGraphRoot.h"
#include "RigVMModel/RigVMController.h"
#include "Units/RigUnit.h"

UAnimNextGraphFactory::UAnimNextGraphFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UAnimNextGraph::StaticClass();
}

bool UAnimNextGraphFactory::ConfigureProperties()
{
	return true;
}

UObject* UAnimNextGraphFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	UAnimNextGraph* NewGraph = NewObject<UAnimNextGraph>(InParent, Class, Name, Flags | RF_Public | RF_Standalone | RF_Transactional | RF_LoadCompleted);

	// Create internal editor data
	UAnimNextGraph_EditorData* EditorData = NewObject<UAnimNextGraph_EditorData>(NewGraph, TEXT("EditorData"));
	NewGraph->EditorData = EditorData;
	EditorData->Initialize(/*bRecompileVM*/false);

	// Add root graph
	EditorData->AddGraph(TEXT("Root"));
	check(EditorData->Graphs.Num() > 0);

	// Compile the initial skeleton
	UE::AnimNext::UncookedOnly::FUtils::Compile(NewGraph);
	check(!EditorData->bErrorsDuringCompilation);

	return NewGraph;
}