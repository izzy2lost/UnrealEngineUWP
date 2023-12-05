// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigSchematicModel.h"
#include "ControlRig.h"

void FControlRigSchematicModel::OnSetObjectBeingDebugged(UObject* InObject)
{
	if(ControlRigBeingDebuggedPtr.Get() == InObject)
	{
		return;
	}

	if(ControlRigBeingDebuggedPtr.IsValid())
	{
		if(UControlRig* ControlRigBeingDebugged = ControlRigBeingDebuggedPtr.Get())
		{
			if(!ControlRigBeingDebugged->HasAnyFlags(RF_BeginDestroyed))
			{
				ControlRigBeingDebugged->GetHierarchy()->OnModified().RemoveAll(this);
			}
		}
	}

	ControlRigBeingDebuggedPtr.Reset();
	
	if(UControlRig* ControlRig = Cast<UControlRig>(InObject))
	{
		ControlRigBeingDebuggedPtr = ControlRig;
		if(URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
		{
			Hierarchy->OnModified().RemoveAll(this);
			Hierarchy->OnModified().AddRaw(this, &FControlRigSchematicModel::OnHierarchyModified);
		}
	}
}

void FControlRigSchematicModel::OnHierarchyModified(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement)
{
	switch (InNotif)
	{
		case ERigHierarchyNotification::ElementAdded:
		{
			if (InElement)
			{
				if (InElement->GetType() == ERigElementType::Socket)
				{
					SchematicGraph.AddNode(InElement->GetName());
				}
			}
			break;
		}
		case ERigHierarchyNotification::ElementRenamed:
		{
			if (InElement)
			{
				if (InElement->GetType() == ERigElementType::Socket)
				{
					const FString OldNameStr = InHierarchy->GetPreviousName(InElement->GetKey()).ToString();
					SchematicGraph.RenameNode(OldNameStr, InElement->GetName());
				}
			}
			break;
		}
		case ERigHierarchyNotification::ElementRemoved:
		{
			if (InElement)
			{
				if (InElement->GetType() == ERigElementType::Socket)
				{
					SchematicGraph.RemoveNode(InElement->GetName());
				}
			}
			break;
		}
		case ERigHierarchyNotification::HierarchyReset:
		{
			SchematicGraph.Reset();
			TArray<FRigSocketElement*> Sockets = InHierarchy->GetElementsOfType<FRigSocketElement>();
			for (FRigSocketElement* Socket : Sockets)
			{
				SchematicGraph.AddNode(Socket->GetFName().ToString());
			}
			break;
		}
	}
}
