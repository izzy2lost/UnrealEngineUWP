// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SchematicGraphPanel/SSchematicGraphPanel.h"
#include "Rigs/RigHierarchyDefines.h"

class UControlRig;
class URigHierarchy;
struct FRigBaseElement;

/** Model for the schematic views */
struct FControlRigSchematicModel 
{
	FSchematicGraph SchematicGraph;
	TWeakObjectPtr<UControlRig> ControlRigBeingDebuggedPtr;

	void OnSetObjectBeingDebugged(UObject* InObject);
	void OnHierarchyModified(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement);
};