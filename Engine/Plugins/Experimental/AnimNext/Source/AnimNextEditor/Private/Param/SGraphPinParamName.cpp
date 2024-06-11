// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGraphPinParamName.h"
#include "EditorUtils.h"
#include "SParameterPickerCombo.h"
#include "ScopedTransaction.h"
#include "UncookedOnlyUtils.h"
#include "Param/ParamCompatibility.h"
#include "Param/ParamUtils.h"
#include "Param/RigVMDispatch_GetLayerParameter.h"
#include "Param/RigVMDispatch_GetParameter.h"
#include "Param/RigVMDispatch_SetLayerParameter.h"
#include "RigVMModel/Nodes/RigVMTemplateNode.h"
#include "RigVMModel/RigVMPin.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMModel/RigVMClient.h"
#include "AnimNextRigVMAsset.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "Module/AnimNextModule_Controller.h"
#include "UniversalObjectLocators/AssetLocatorFragment.h"

#define LOCTEXT_NAMESPACE "SGraphPinParamName"

namespace UE::AnimNext::Editor
{

void SGraphPinParamName::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	ModelPin = InArgs._ModelPin;
	Node = InArgs._GraphNode;
	FilterType = InArgs._FilterType;
	InstanceId = TInstancedStruct<FAnimNextParamUniversalObjectLocator>::Make();

	if(ModelPin && ModelPin->GetGraph())
	{
		ModelPin->GetGraph()->OnModified().AddSP(this, &SGraphPinParamName::HandleGraphModified);
	}

	SGraphPin::Construct(SGraphPin::FArguments(), InPin);
}

TSharedRef<SWidget> SGraphPinParamName::GetDefaultValueWidget()
{
	FParameterPickerArgs Args;
	Args.bMultiSelect = false;
	Args.bShowInstanceId = false;

	UpdateCachedParamType();

	// Check whether this is a Set/Get parameter node, and if so only show parameters in the current asset
	if (ModelPin)
	{
		if (const URigVMTemplateNode* TemplateNode = Cast<URigVMTemplateNode>(ModelPin->GetOuter()))
		{
			const FRigVMDispatchFactory* GetLayerParameterFactory = FRigVMRegistry::Get().FindOrAddDispatchFactory(FRigVMDispatch_GetLayerParameter::StaticStruct());
			const FName GetLayerParameterNotation = GetLayerParameterFactory->GetTemplate()->GetNotation();

			const FRigVMDispatchFactory* SetLayerParameterFactory = FRigVMRegistry::Get().FindOrAddDispatchFactory(FRigVMDispatch_SetLayerParameter::StaticStruct());
			const FName SetLayerParameterNotation = SetLayerParameterFactory->GetTemplate()->GetNotation();

			if (TemplateNode->GetNotation() == GetLayerParameterNotation || TemplateNode->GetNotation() == SetLayerParameterNotation)
			{
				AssetData = FAssetData(ModelPin->GetTypedOuter<UAnimNextRigVMAsset>());
				InstanceId.GetMutable().Locator.Reset();
				InstanceId.GetMutable().Locator.AddFragment<FAssetLocatorFragment>(AssetData);
			}
		}
	}

	Args.OnParameterPicked = FOnParameterPicked::CreateLambda([this](const FParameterBindingReference& InParameterBinding)
	{
		FScopedTransaction Transaction(LOCTEXT("SelectParameter", "Select Parameter"));

		if(ModelPin && Node)
		{
			UAnimNextModule_Controller* Controller = CastChecked<UAnimNextModule_Controller>(Node->GetController());
			Controller->SetAnimNextParameterNode(Node->GetModelNode(), InParameterBinding.Parameter, InParameterBinding.Type, InstanceId);
		}
		else
		{
			GraphPinObj->Modify();
			GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, InParameterBinding.Parameter.ToString());
		}

		UpdateCachedParamType();
	});
	
	Args.OnFilterParameterType = FOnFilterParameterType::CreateLambda([this](const FAnimNextParamType& InParamType)-> EFilterParameterResult
	{
		if(FilterType.IsValid())
		{
			return FParamUtils::GetCompatibility(FilterType, InParamType).IsCompatible() ? EFilterParameterResult::Include : EFilterParameterResult::Exclude;
		}
		else if(Node && ModelPin->IsLinked())
		{
			const FAnimNextParamType Type = FAnimNextParamType::FromRigVMTemplateArgument(ModelPin->GetTemplateArgumentType());
			return Type.IsValid() && FParamUtils::GetCompatibility(Type, InParamType).IsCompatible() ? EFilterParameterResult::Include : EFilterParameterResult::Exclude;
		}

		return EFilterParameterResult::Include;
	});

	Args.OnFilterParameter = FOnFilterParameter::CreateLambda([this](const FParameterBindingReference& InParameterBinding)
	{
		if(AssetData.IsValid())
		{
			return InParameterBinding.Graph == AssetData ? EFilterParameterResult::Include : EFilterParameterResult::Exclude;
		}

		return EFilterParameterResult::Include;
	});
	
	Args.NewParameterType = FilterType;
	
	return SAssignNew(PickerCombo, SParameterPickerCombo)
		.PickerArgs(Args)
		.OnGetParameterName_Lambda([this]()
		{
			if(ModelPin && Node)
			{
				return FName(*ModelPin->GetDefaultValue());
			}
			else
			{
				return FName(*GraphPinObj->DefaultValue);
			}
		})
		.OnGetParameterType_Lambda([this]()
		{
			return CachedType;
		})
		.OnGetParameterInstanceId_Lambda([this]()
		{
			return InstanceId;
		});
}

void SGraphPinParamName::UpdateCachedParamType()
{
	if(ModelPin && Node)
	{
		CachedType = UncookedOnly::FUtils::GetParameterTypeFromName(FName(*ModelPin->GetDefaultValue()));
	}
	else
	{
		CachedType = UncookedOnly::FUtils::GetParameterTypeFromName(FName(*GraphPinObj->DefaultValue));
	}

	if(PickerCombo)
	{
		PickerCombo->RequestRefresh();
	}
}

void SGraphPinParamName::HandleGraphModified(ERigVMGraphNotifType InType, URigVMGraph* InGraph, UObject* InSubject)
{
	switch (InType)
	{
	case ERigVMGraphNotifType::PinDefaultValueChanged:
		UpdateCachedParamType();
		break;
	default:
		break;
	}
}

}

#undef LOCTEXT_NAMESPACE