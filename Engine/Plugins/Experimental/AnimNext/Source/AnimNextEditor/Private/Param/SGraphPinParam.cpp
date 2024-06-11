// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGraphPinParam.h"
#include "EditorUtils.h"
#include "SParameterPickerCombo.h"
#include "ScopedTransaction.h"
#include "UncookedOnlyUtils.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "Module/AnimNextModule.h"
#include "Module/AnimNextModule_Controller.h"
#include "Param/ParamCompatibility.h"
#include "Param/ParamUtils.h"
#include "Param/RigVMDispatch_GetLayerParameter.h"
#include "Param/RigVMDispatch_GetParameter.h"
#include "Param/RigVMDispatch_GetScopedParameter.h"
#include "Param/RigVMDispatch_SetLayerParameter.h"
#include "RigVMModel/Nodes/RigVMTemplateNode.h"
#include "RigVMModel/RigVMPin.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMModel/RigVMController.h"
#include "Param/AnimNextParam.h"
#include "Param/AnimNextEditorParam.h"

#define LOCTEXT_NAMESPACE "SGraphPinParam"

namespace UE::AnimNext::Editor
{

void SGraphPinParam::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	ModelPin = InArgs._ModelPin;
	Node = InArgs._GraphNode;
	FilterType = InArgs._FilterType;
	ParamStruct = CastChecked<UScriptStruct>(InPin->PinType.PinSubCategoryObject.Get());

	if(ModelPin && ModelPin->GetGraph())
	{
		ModelPin->GetGraph()->OnModified().AddSP(this, &SGraphPinParam::HandleGraphModified);
	}
	
	SGraphPin::Construct(SGraphPin::FArguments(), InPin);
}

TSharedRef<SWidget> SGraphPinParam::GetDefaultValueWidget()
{
	FParameterPickerArgs Args;
	Args.bMultiSelect = false;

	UpdateCachedParam();

	Args.OnParameterPicked = FOnParameterPicked::CreateLambda([this](const FParameterBindingReference& InParameterBinding)
	{
		{
			FScopedTransaction Transaction(LOCTEXT("SelectParameter", "Select Parameter"));

			if(Node && ModelPin)
			{
				UAnimNextModule_Controller* Controller = CastChecked<UAnimNextModule_Controller>(Node->GetController());
				Controller->SetAnimNextParameterNode(Node->GetModelNode(), InParameterBinding.Parameter, InParameterBinding.Type.GetValueType(), InParameterBinding.Type.GetContainerType(), InParameterBinding.Type.GetValueTypeObject(), InParameterBinding.InstanceId);
			}
			else
			{
				FString ValueAsString;
				if(ParamStruct == FAnimNextEditorParam::StaticStruct())
				{
					FAnimNextEditorParam ParamValue(*InParameterBinding.Parameter.ToString(), InParameterBinding.Type, InParameterBinding.InstanceId);
					FAnimNextEditorParam::StaticStruct()->ExportText(ValueAsString, &ParamValue, nullptr, nullptr, PPF_None, nullptr);
				}
				else if(ParamStruct == FAnimNextParam::StaticStruct())
				{
					FAnimNextParam ParamValue(*InParameterBinding.Parameter.ToString(), InParameterBinding.Type, InParameterBinding.InstanceId);
					FAnimNextParam::StaticStruct()->ExportText(ValueAsString, &ParamValue, nullptr, nullptr, PPF_None, nullptr);
				}

				GraphPinObj->Modify();
				GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, ValueAsString);
			}
		}

		UpdateCachedParam();
	});
	
	Args.OnFilterParameterType = FOnFilterParameterType::CreateLambda([this](const FAnimNextParamType& InParamType)-> EFilterParameterResult
	{
		if(FilterType.IsValid())
		{
			if(!FParamUtils::GetCompatibility(FilterType, InParamType).IsCompatible())
			{
				return EFilterParameterResult::Exclude;
			}
		}

		if(InParamType.IsValid())
		{
			const FRigVMTemplateArgumentType RigVMType = InParamType.ToRigVMTemplateArgument();
			if(!RigVMType.IsValid() || FRigVMRegistry::Get().GetTypeIndex(RigVMType) == INDEX_NONE)
			{
				return EFilterParameterResult::Exclude;
			}
		}

		if(Node && ModelPin->IsLinked())
		{
			const FAnimNextParamType Type = FAnimNextParamType::FromRigVMTemplateArgument(ModelPin->GetTemplateArgumentType());
			if(!Type.IsValid() || !FParamUtils::GetCompatibility(Type, InParamType).IsCompatible())
			{
				return EFilterParameterResult::Exclude;
			}
		}

		return EFilterParameterResult::Include;
	});
	Args.NewParameterType = FilterType;

	Args.OnInstanceIdChanged = FOnInstanceIdChanged::CreateLambda([this](const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId)
	{
		{
			FScopedTransaction Transaction(LOCTEXT("SelectParameterScope", "Select Parameter Scope"));
			
			if(Node && ModelPin)
			{
				UAnimNextModule_Controller* Controller = CastChecked<UAnimNextModule_Controller>(Node->GetController());
				Controller->SetAnimNextParameterNode(Node->GetModelNode(), NAME_None, EPropertyBagPropertyType::None, EPropertyBagContainerType::None, nullptr, InInstanceId);
			}
			else
			{
				FString ValueAsString;
				if(ParamStruct == FAnimNextEditorParam::StaticStruct())
				{
					FAnimNextEditorParam ParamValue(NAME_None, FAnimNextParamType(), InInstanceId);
					FAnimNextEditorParam::StaticStruct()->ExportText(ValueAsString, &ParamValue, nullptr, nullptr, PPF_None, nullptr);
				}
				else if(ParamStruct == FAnimNextParam::StaticStruct())
				{
					FAnimNextParam ParamValue(NAME_None, FAnimNextParamType(), InInstanceId);
					FAnimNextParam::StaticStruct()->ExportText(ValueAsString, &ParamValue, nullptr, nullptr, PPF_None, nullptr);
				}

				GraphPinObj->Modify();
				GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, ValueAsString);
			}
		}

		UpdateCachedParam();
	});

	Args.InstanceId = CachedParam.InstanceId;

	return SAssignNew(PickerCombo, SParameterPickerCombo)
		.PickerArgs(Args)
		.OnGetParameterName_Lambda([this]()
		{
			return CachedParam.Name;
		})
		.OnGetParameterType_Lambda([this]()
		{
			return CachedParam.Type;
		})
		.OnGetParameterInstanceId_Lambda([this]()
		{
			return CachedParam.InstanceId;
		});
}

void SGraphPinParam::UpdateCachedParam()
{
	TStringBuilder<256> DefaultValueBuilder;
	if(ModelPin)
	{
		DefaultValueBuilder.Append(ModelPin->GetDefaultValue());
	}
	else if(GraphPinObj)
	{
		DefaultValueBuilder.Append(GraphPinObj->DefaultValue);
	}

	if(DefaultValueBuilder.Len() > 0)
	{
		if(ParamStruct == FAnimNextEditorParam::StaticStruct())
		{
			FAnimNextEditorParam::StaticStruct()->ImportText(DefaultValueBuilder.ToString(), &CachedParam, nullptr, PPF_None, nullptr, FAnimNextEditorParam::StaticStruct()->GetName());
		}
		else if(ParamStruct == FAnimNextParam::StaticStruct())
		{
			FAnimNextParam Param;
			FAnimNextParam::StaticStruct()->ImportText(DefaultValueBuilder.ToString(), &Param, nullptr, PPF_None, nullptr, FAnimNextParam::StaticStruct()->GetName());
			CachedParam = FAnimNextEditorParam(Param);
		}
	}

	if(PickerCombo.IsValid())
	{
		PickerCombo->RequestRefresh();
	}
}

void SGraphPinParam::HandleGraphModified(ERigVMGraphNotifType InType, URigVMGraph* InGraph, UObject* InSubject)
{
	switch (InType)
	{
	case ERigVMGraphNotifType::PinDefaultValueChanged:
		UpdateCachedParam();
		break;
	default:
		break;
	}
}

}

#undef LOCTEXT_NAMESPACE