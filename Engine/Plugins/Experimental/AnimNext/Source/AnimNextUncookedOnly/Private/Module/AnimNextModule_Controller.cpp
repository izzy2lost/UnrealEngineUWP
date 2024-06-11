// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/AnimNextModule_Controller.h"

#include "AnimNextUnitNode.h"
#include "Graph/RigDecorator_AnimNextCppTrait.h"
#include "Param/AnimNextEditorParam.h"
#include "Param/AnimNextParam.h"
#include "Param/AnimNextParamInstanceIdentifier.h"
#include "Param/ParamType.h"
#include "Param/RigVMDispatch_GetLayerParameter.h"
#include "Param/RigVMDispatch_GetParameter.h"
#include "Param/RigVMDispatch_GetScopedParameter.h"
#include "Param/RigVMDispatch_SetLayerParameter.h"
#include "RigVMModel/RigVMControllerActions.h"
#include "TraitCore/Trait.h"
#include "TraitCore/TraitRegistry.h"
#include "TraitCore/TraitUID.h"

FName UAnimNextModule_Controller::AddTraitByName(FName InNodeName, FName InNewTraitTypeName, int32 InPinIndex, const FString& InNewTraitDefaultValue, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if (!IsValidGraph())
	{
		return NAME_None;
	}

	if (!IsTransacting() && !IsGraphEditable())
	{
		return NAME_None;
	}

	if (InNodeName == NAME_None)
	{
		ReportError(TEXT("Invalid node name."));
		return NAME_None;
	}

	const URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMNode * Node = Graph->FindNodeByName(InNodeName);
	if (Node == nullptr)
	{
		ReportError(TEXT("This graph does not contain a node with the provided name."));
		return NAME_None;
	}

	UE::AnimNext::FTraitRegistry& TraitRegistry = UE::AnimNext::FTraitRegistry::Get();

	const UE::AnimNext::FTrait* Trait = TraitRegistry.Find(InNewTraitTypeName);
	if (Trait == nullptr)
	{
		ReportError(TEXT("Unknown Trait Type."));
		return NAME_None;
	}

	const UScriptStruct* CppDecoratorStruct = FRigDecorator_AnimNextCppDecorator::StaticStruct();
	UScriptStruct* ScriptStruct = Trait->GetTraitSharedDataStruct();

	FString DefaultValue = InNewTraitDefaultValue;
	if (DefaultValue.IsEmpty())
	{
		const FRigDecorator_AnimNextCppDecorator DefaultCppDecoratorStructInstance;
		FRigDecorator_AnimNextCppDecorator CppDecoratorStructInstance;
		CppDecoratorStructInstance.DecoratorSharedDataStruct = ScriptStruct;

		if (!CppDecoratorStructInstance.CanBeAddedToNode(Node, nullptr))
		{
			ReportError(TEXT("Trait is not supported by the Node."));
			return NAME_None;	// This trait isn't supported on this node
		}

		const FProperty* Prop = FAnimNextCppDecoratorWrapper::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_STRING_CHECKED(FAnimNextCppDecoratorWrapper, CppDecorator));
		Prop->ExportText_Direct(DefaultValue, &CppDecoratorStructInstance, &DefaultCppDecoratorStructInstance, nullptr, PPF_SerializedAsImportText);
	}

	// Avoid multiple VM recompilations for internal operations
	FRigVMControllerCompileBracketScope CompileScope(this);

	const FName TraitName = InNewTraitTypeName;

	return AddTrait(InNodeName, *CppDecoratorStruct->GetPathName(), TraitName, DefaultValue, InPinIndex, bSetupUndoRedo, bPrintPythonCommand);
}

bool UAnimNextModule_Controller::RemoveTraitByName(FName InNodeName, FName InTraitInstanceName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if (!IsValidGraph())
	{
		return false;
	}

	if (!IsTransacting() && !IsGraphEditable())
	{
		return false;
	}

	if (InNodeName == NAME_None)
	{
		ReportError(TEXT("Invalid node name."));
		return false;
	}

	// Avoid multiple VM recompilations for internal operations
	FRigVMControllerCompileBracketScope CompileScope(this);
	return RemoveTrait(InNodeName, InTraitInstanceName, bSetupUndoRedo, bPrintPythonCommand);
}

FName UAnimNextModule_Controller::SwapTraitByName(FName InNodeName, FName InTraitInstanceName, int32 InCurrentTraitPinIndex, FName InNewTraitTypeName, const FString& InNewTraitDefaultValue, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if (!IsValidGraph())
	{
		return NAME_None;
	}

	if (!IsTransacting() && !IsGraphEditable())
	{
		return NAME_None;
	}

	if (InNodeName == NAME_None)
	{
		ReportError(TEXT("Invalid node name."));
		return NAME_None;
	}

	// Avoid multiple VM recompilations, for each operation
	FRigVMControllerCompileBracketScope CompileScope(this);
	if (RemoveTraitByName(InNodeName, InTraitInstanceName, bSetupUndoRedo, bPrintPythonCommand))
	{
		return AddTraitByName(InNodeName, InNewTraitTypeName, InCurrentTraitPinIndex, InNewTraitDefaultValue, bSetupUndoRedo, bPrintPythonCommand);
	}

	return NAME_None;
}

bool UAnimNextModule_Controller::SetTraitPinIndex(FName InNodeName, FName InTraitInstanceName, int32 InNewPinIndex, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if (!IsValidGraph())
	{
		return false;
	}

	if (!IsTransacting() && !IsGraphEditable())
	{
		return false;
	}

	if (InNodeName == NAME_None)
	{
		ReportError(TEXT("Invalid node name."));
		return false;
	}

	const URigVMGraph* Graph = GetGraph();
	check(Graph);

	URigVMNode* Node = Graph->FindNodeByName(InNodeName);
	if (Node == nullptr)
	{
		ReportError(TEXT("This graph does not contain a node with the provided name."));
		return false;
	}

	if (!IsTransacting() && !IsGraphEditable())
	{
		return false;
	}

	URigVMPin* TraitPin = Node->FindTrait(InTraitInstanceName);
	if (TraitPin == nullptr)
	{
		ReportError(TEXT("The node does not contain a Trait with the provided name."));
		return false;
	}
	
	// Save current pin data for later
	const FString TraitDefaultValue = TraitPin->GetDefaultValue();

	// TODO zzz : Is there a better way to get a Trait* from a TraitPin ?
	if (TSharedPtr<FStructOnScope> ScopedTrait = Node->GetTraitInstance(TraitPin->GetFName()))
	{
		const FRigVMTrait* Trait = (FRigVMTrait*)ScopedTrait->GetStructMemory();
		if (const UScriptStruct* TraitSharedInstanceData = Trait->GetTraitSharedDataStruct())
		{
			UE::AnimNext::FTraitRegistry& TraitRegistry = UE::AnimNext::FTraitRegistry::Get();
			if (const UE::AnimNext::FTrait* AnimNextTrait = TraitRegistry.Find(TraitSharedInstanceData))
			{
				// Avoid multiple VM recompilations, for each operation
				FRigVMControllerCompileBracketScope CompileScope(this);
				if (RemoveTraitByName(InNodeName, InTraitInstanceName, bSetupUndoRedo, bPrintPythonCommand))
				{
					// TOOO zzz : Why TraitName is a string ?
					if (AddTraitByName(InNodeName, FName(AnimNextTrait->GetTraitName()), InNewPinIndex, TraitDefaultValue, bSetupUndoRedo, bPrintPythonCommand) != NAME_None)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

URigVMUnitNode* UAnimNextModule_Controller::AddUnitNodeWithPins(UScriptStruct* InScriptStruct, const FRigVMPinInfoArray& PinArray, const FName& InMethodName, const FVector2D& InPosition, const FString& InNodeName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	const bool bHasDynamicPins = PinArray.Num() != 0;

	if (bHasDynamicPins)
	{
		OpenUndoBracket(TEXT("Add unit node with pins"));
	}

	URigVMUnitNode* Node = AddUnitNode(InScriptStruct, UAnimNextUnitNode::StaticClass(), InMethodName, InPosition, InNodeName, bSetupUndoRedo, bPrintPythonCommand);

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

bool UAnimNextModule_Controller::SetAnimNextParameterNode(URigVMNode* ParameterNode, FName ParameterName, EPropertyBagPropertyType ValueType, EPropertyBagContainerType ContainerType, const UObject* ValueTypeObject, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return SetAnimNextParameterNode(ParameterNode, ParameterName, FAnimNextParamType(ValueType, ContainerType, ValueTypeObject), InstanceId, bSetupUndoRedo, bPrintPythonCommand);
}

bool UAnimNextModule_Controller::SetAnimNextParameterNode(URigVMNode* ParameterNode, FName ParameterName, const FAnimNextParamType& Type, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
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

URigVMNode* UAnimNextModule_Controller::AddGetAnimNextParameterNode(const FVector2D& Position, FName ParameterName, EPropertyBagPropertyType ValueType, EPropertyBagContainerType ContainerType, const UObject* ValueTypeObject, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return AddGetAnimNextParameterNode(Position, ParameterName, FAnimNextParamType(ValueType, ContainerType, ValueTypeObject), InstanceId, bSetupUndoRedo, bPrintPythonCommand);
}

URigVMNode* UAnimNextModule_Controller::AddGetAnimNextParameterNode(const FVector2D& Position, FName ParameterName, const FAnimNextParamType& Type, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return AddGetAnimNextParameterNodeInternal(FRigVMDispatch_GetScopedParameter::StaticStruct(), Position, ParameterName, Type, InstanceId, bSetupUndoRedo, bPrintPythonCommand);
}

URigVMNode* UAnimNextModule_Controller::AddGetAnimNextGraphParameterNode(const FVector2D& Position, FName ParameterName, EPropertyBagPropertyType ValueType, EPropertyBagContainerType ContainerType, const UObject* ValueTypeObject, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return AddGetAnimNextGraphParameterNode(Position, ParameterName, FAnimNextParamType(ValueType, ContainerType, ValueTypeObject), InstanceId, bSetupUndoRedo, bPrintPythonCommand);
}

URigVMNode* UAnimNextModule_Controller::AddGetAnimNextGraphParameterNode(const FVector2D& Position, FName ParameterName, const FAnimNextParamType& Type, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
return AddGetAnimNextParameterNodeInternal(FRigVMDispatch_GetLayerParameter::StaticStruct(), Position, ParameterName, Type, InstanceId, bSetupUndoRedo, bPrintPythonCommand);
}

URigVMNode* UAnimNextModule_Controller::AddSetAnimNextGraphParameterNode(const FVector2D& Position, FName ParameterName, EPropertyBagPropertyType ValueType, EPropertyBagContainerType ContainerType, const UObject* ValueTypeObject, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return AddSetAnimNextGraphParameterNode(Position, ParameterName, FAnimNextParamType(ValueType, ContainerType, ValueTypeObject), InstanceId, bSetupUndoRedo, bPrintPythonCommand);
}

URigVMNode* UAnimNextModule_Controller::AddSetAnimNextGraphParameterNode(const FVector2D& Position, FName ParameterName, const FAnimNextParamType& Type, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return AddGetAnimNextParameterNodeInternal(FRigVMDispatch_SetLayerParameter::StaticStruct(), Position, ParameterName, Type, InstanceId, bSetupUndoRedo, bPrintPythonCommand);
}

URigVMNode* UAnimNextModule_Controller::AddGetAnimNextParameterNodeInternal(UScriptStruct* NodeStruct, const FVector2D& Position, FName ParameterName, const FAnimNextParamType& Type, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId, bool bSetupUndoRedo, bool bPrintPythonCommand)
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