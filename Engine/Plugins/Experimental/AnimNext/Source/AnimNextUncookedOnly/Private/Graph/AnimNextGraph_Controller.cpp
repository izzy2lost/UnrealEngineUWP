// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph_Controller.h"
#include "Graph/AnimNextGraph_UnitNode.h"
#include "Param/AnimNextEditorParam.h"
#include "Param/AnimNextParam.h"
#include "Param/AnimNextParamInstanceIdentifier.h"
#include "Param/ParamType.h"
#include "Param/RigVMDispatch_GetLayerParameter.h"
#include "Param/RigVMDispatch_GetParameter.h"
#include "Param/RigVMDispatch_GetScopedParameter.h"
#include "Param/RigVMDispatch_SetLayerParameter.h"

URigVMUnitNode* UAnimNextGraph_Controller::AddUnitNodeWithPins(UScriptStruct* InScriptStruct, const FRigVMPinInfoArray& PinArray, const FName& InMethodName, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	const bool bHasDynamicPins = PinArray.Num() != 0;

	if (bHasDynamicPins)
	{
		OpenUndoBracket(TEXT("Add unit node with pins"));
	}

	URigVMUnitNode* Node = AddUnitNode(InScriptStruct, UAnimNextGraph_UnitNode::StaticClass(), InMethodName, InPosition, InNodeName, bSetupUndoRedo, bPrintPythonCommand);

	if (Node == nullptr)
	{
		if (bHasDynamicPins)
		{
			CancelUndoBracket();
		}

		return nullptr;
	}

	if (bHasDynamicPins)
	{
		const FRigVMRegistry& Registry = FRigVMRegistry::Get();
		const FRigVMPinInfoArray PreviousPins(Node, this);

		for (int32 PinIndex = 0; PinIndex < PinArray.Num(); ++PinIndex)
		{
			const FString& PinPath = PinArray.GetPinPath(PinIndex);
			FString ParentPinPath, PinName;
			UObject* OuterForPin = Node;
			if (URigVMPin::SplitPinPathAtEnd(PinPath, ParentPinPath, PinName))
			{
				OuterForPin = Node->FindPin(ParentPinPath);
			}

			CreatePinFromPinInfo(Registry, PreviousPins, PinArray[PinIndex], PinPath, OuterForPin);
		}

		CloseUndoBracket();
	}

	return Node;
}

bool UAnimNextGraph_Controller::SetAnimNextParameterNode(URigVMNode* ParameterNode, FName ParameterName, EPropertyBagPropertyType ValueType, EPropertyBagContainerType ContainerType, const UObject* ValueTypeObject, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return SetAnimNextParameterNode(ParameterNode, ParameterName, FAnimNextParamType(ValueType, ContainerType, ValueTypeObject), InstanceId, bSetupUndoRedo, bPrintPythonCommand);
}

bool UAnimNextGraph_Controller::SetAnimNextParameterNode(URigVMNode* ParameterNode, FName ParameterName, const FAnimNextParamType& Type, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(!IsValidGraph())
	{
		return false;
	}

	if (!IsTransacting() && !IsGraphEditable())
	{
		return false;
	}

	if(ParameterNode == nullptr)
	{
		ReportError(TEXT("Invalid node."));
		return false;
	}

	const URigVMTemplateNode* TemplateNode = Cast<URigVMTemplateNode>(ParameterNode);
	if (TemplateNode == nullptr)
	{
		ReportError(TEXT("Not a template node."));
		return false;
	}

	const FRigVMDispatchFactory* GetParameterFactory = FRigVMRegistry::Get().FindOrAddDispatchFactory(FRigVMDispatch_GetParameter::StaticStruct());
	const FName GetParameterNotation = GetParameterFactory->GetTemplate()->GetNotation();
	const FRigVMDispatchFactory* GetScopedParameterFactory = FRigVMRegistry::Get().FindOrAddDispatchFactory(FRigVMDispatch_GetScopedParameter::StaticStruct());
	const FName GetScopedParameterNotation = GetScopedParameterFactory->GetTemplate()->GetNotation();
	const FRigVMDispatchFactory* GetLayerParameterFactory = FRigVMRegistry::Get().FindOrAddDispatchFactory(FRigVMDispatch_GetLayerParameter::StaticStruct());
	const FName GetLayerParameterNotation = GetLayerParameterFactory->GetTemplate()->GetNotation();
	const FRigVMDispatchFactory* SetLayerParameterFactory = FRigVMRegistry::Get().FindOrAddDispatchFactory(FRigVMDispatch_SetLayerParameter::StaticStruct());
	const FName SetLayerParameterNotation = SetLayerParameterFactory->GetTemplate()->GetNotation();

	if( TemplateNode->GetNotation() != GetParameterNotation &&
		TemplateNode->GetNotation() != GetScopedParameterNotation &&
		TemplateNode->GetNotation() != GetLayerParameterNotation &&
		TemplateNode->GetNotation() != SetLayerParameterNotation)
	{
		ReportError(TEXT("Not a parameter node."));
		return false;
	}

	const bool bIsNamedParam = TemplateNode->GetNotation() == GetLayerParameterNotation || TemplateNode->GetNotation() == SetLayerParameterNotation;

	if(ParameterName != NAME_None)
	{
		const FSoftObjectPath SoftObjectPath(ParameterName.ToString());
		if(!SoftObjectPath.GetAssetPath().IsValid() || SoftObjectPath.GetSubPathString().Len() == 0)
		{
			ReportError(TEXT("InParameterName is an invalid format. Should be /AssetOrFieldPath/ClassOrAsset.ClassOrAsset:FieldOrParameter."));
			return false;
		}
	}

	if (!Type.IsNone() && !Type.IsValid())
	{
		ReportError(TEXT("Type is invalid."));
		return false;
	}

	URigVMPin* ParameterPin = ParameterNode->FindPin(FRigVMDispatch_GetParameter::ParameterName.ToString());
	if(ParameterPin == nullptr)
	{
		ReportError(TEXT("Parameter pin not found."));
		return false;
	}

	if(bSetupUndoRedo)
	{
		OpenUndoBracket(TEXT("Set parameter"));
	}

	FString ValueAsString;
	if(bIsNamedParam)
	{
		ValueAsString = ParameterName.ToString();
	}
	else
	{
		FAnimNextParam ParamValue(ParameterName, Type, InstanceId);
		FAnimNextParam::StaticStruct()->ExportText(ValueAsString, &ParamValue, nullptr, nullptr, PPF_None, nullptr);
	}

	if(!SetPinDefaultValue(ParameterPin->GetPinPath(), ValueAsString, true, bSetupUndoRedo, true, bPrintPythonCommand))
	{
		if(bSetupUndoRedo)
		{
			CancelUndoBracket();
		}
		return false;
	}

	FRigVMTemplateArgumentType RigVMType = Type.ToRigVMTemplateArgument();
	URigVMPin* OutputPin = ParameterNode->FindPin(FRigVMDispatch_GetParameter::ValueName.ToString());
	if(RigVMType.IsValid() && OutputPin)
	{
		// Re-resolve the node's output
		TArray<URigVMLink*> Links = OutputPin->GetLinks();
		if(!TemplateNode->IsFullyUnresolved())
		{
			if(!UnresolveTemplateNodes({ ParameterNode->GetFName() }, bSetupUndoRedo, bPrintPythonCommand))
			{
				if(bSetupUndoRedo)
				{
					CloseUndoBracket();
				}
				return true;
			}
		}

		if(TemplateNode->IsFullyUnresolved())
		{
			if(!ResolveWildCardPin(OutputPin, RigVMType, bSetupUndoRedo, bPrintPythonCommand))
			{
				if(bSetupUndoRedo)
				{
					CloseUndoBracket();
				}
				return true;
			}
		}

		// Try to restore links
		for(URigVMLink* Link : Links)
		{
			if(!AddLink(OutputPin, Link->GetOppositePin(OutputPin), bSetupUndoRedo))
			{
				if(bSetupUndoRedo)
				{
					CloseUndoBracket();
				}
				return true;
			}
		}
	}

	if(bSetupUndoRedo)
	{
		CloseUndoBracket();
	}

	return true;
}

URigVMNode* UAnimNextGraph_Controller::AddGetAnimNextParameterNode(const FVector2D& Position, FName ParameterName, EPropertyBagPropertyType ValueType, EPropertyBagContainerType ContainerType, const UObject* ValueTypeObject, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return AddGetAnimNextParameterNode(Position, ParameterName, FAnimNextParamType(ValueType, ContainerType, ValueTypeObject), InstanceId, bSetupUndoRedo, bPrintPythonCommand);
}

URigVMNode* UAnimNextGraph_Controller::AddGetAnimNextParameterNode(const FVector2D& Position, FName ParameterName, const FAnimNextParamType& Type, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return AddGetAnimNextParameterNodeInternal(FRigVMDispatch_GetScopedParameter::StaticStruct(), Position, ParameterName, Type, InstanceId, bSetupUndoRedo, bPrintPythonCommand);
}

URigVMNode* UAnimNextGraph_Controller::AddGetAnimNextGraphParameterNode(const FVector2D& Position, FName ParameterName, EPropertyBagPropertyType ValueType, EPropertyBagContainerType ContainerType, const UObject* ValueTypeObject, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return AddGetAnimNextGraphParameterNode(Position, ParameterName, FAnimNextParamType(ValueType, ContainerType, ValueTypeObject), InstanceId, bSetupUndoRedo, bPrintPythonCommand);
}

URigVMNode* UAnimNextGraph_Controller::AddGetAnimNextGraphParameterNode(const FVector2D& Position, FName ParameterName, const FAnimNextParamType& Type, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
return AddGetAnimNextParameterNodeInternal(FRigVMDispatch_GetLayerParameter::StaticStruct(), Position, ParameterName, Type, InstanceId, bSetupUndoRedo, bPrintPythonCommand);
}

URigVMNode* UAnimNextGraph_Controller::AddSetAnimNextGraphParameterNode(const FVector2D& Position, FName ParameterName, EPropertyBagPropertyType ValueType, EPropertyBagContainerType ContainerType, const UObject* ValueTypeObject, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return AddSetAnimNextGraphParameterNode(Position, ParameterName, FAnimNextParamType(ValueType, ContainerType, ValueTypeObject), InstanceId, bSetupUndoRedo, bPrintPythonCommand);
}

URigVMNode* UAnimNextGraph_Controller::AddSetAnimNextGraphParameterNode(const FVector2D& Position, FName ParameterName, const FAnimNextParamType& Type, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return AddGetAnimNextParameterNodeInternal(FRigVMDispatch_SetLayerParameter::StaticStruct(), Position, ParameterName, Type, InstanceId, bSetupUndoRedo, bPrintPythonCommand);
}

URigVMNode* UAnimNextGraph_Controller::AddGetAnimNextParameterNodeInternal(UScriptStruct* NodeStruct, const FVector2D& Position, FName ParameterName, const FAnimNextParamType& Type, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(!IsValidGraph())
	{
		return nullptr;
	}

	URigVMGraph* Graph = GetGraph();
	check(Graph);

	if(bSetupUndoRedo)
	{
		OpenUndoBracket(TEXT("Add parameter node"));
	}

	const FString Name = GetSchema()->GetValidNodeName(Graph, TEXT("ParameterNode"));
	const FRigVMDispatchFactory* Factory = FRigVMRegistry::Get().FindOrAddDispatchFactory(NodeStruct);
	URigVMNode* Node = AddTemplateNode(Factory->GetTemplate()->GetNotation(), Position, Name, bSetupUndoRedo, bPrintPythonCommand);
	if(Node)
	{
		if(!SetAnimNextParameterNode(Node, ParameterName, Type, InstanceId, bSetupUndoRedo, bPrintPythonCommand))
		{
			if(bSetupUndoRedo)
			{
				CancelUndoBracket();
			}
			return nullptr;
		}
	}
	else if(bSetupUndoRedo)
	{
		CancelUndoBracket();
	}

	if(bSetupUndoRedo)
	{
		CloseUndoBracket();
	}

	return Node;
}