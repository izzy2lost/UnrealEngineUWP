// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParametersEditorSchemaActions.h"
#include "Param/AnimNextParameterBlock_EdGraph.h"
#include "Param/AnimNextParameterBlock_EdGraphNode.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "Settings/ControlRigSettings.h"
#include "EditorUtils.h"

UEdGraphNode* FAnimNextParameterSchemaAction_RigUnit::PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode)
{
	UAnimNextParameterBlock_EditorData* EditorData = ParentGraph->GetTypedOuter<UAnimNextParameterBlock_EditorData>();
	UAnimNextParameterBlock_EdGraphNode* NewNode = nullptr;
	UAnimNextParameterBlock_EdGraph* EdGraph = Cast<UAnimNextParameterBlock_EdGraph>(ParentGraph);

	if (EditorData != nullptr && EdGraph != nullptr)
	{
		FName Name = UE::AnimNext::Editor::FUtils::ValidateName(EditorData, StructTemplate->GetFName().ToString());
		URigVMController* Controller = EditorData->GetRigVMClient()->GetController(ParentGraph);

		Controller->OpenUndoBracket(FString::Printf(TEXT("Add '%s' Node"), *Name.ToString()));

		FRigVMUnitNodeCreatedContext& UnitNodeCreatedContext = Controller->GetUnitNodeCreatedContext();
		FRigVMUnitNodeCreatedContext::FScope ReasonScope(UnitNodeCreatedContext, ERigVMNodeCreatedReason::NodeSpawner);

		if (URigVMUnitNode* ModelNode = Controller->AddUnitNode(StructTemplate, FRigUnit::GetMethodName(), Location, Name.ToString(), true, false))
		{
			NewNode = Cast<UAnimNextParameterBlock_EdGraphNode>(EdGraph->FindNodeForModelNodeName(ModelNode->GetFName()));
			check(NewNode);

			if (NewNode)
			{
				Controller->ClearNodeSelection(true);
				Controller->SelectNode(ModelNode, true, true);
			}

#if WITH_EDITORONLY_DATA

			// @TODO: switch over to custom settings here
			const FControlRigSettingsPerPinBool* ExpansionMapPtr = UControlRigEditorSettings::Get()->RigUnitPinExpansion.Find(ModelNode->GetScriptStruct()->GetName());
			if (ExpansionMapPtr)
			{
				const FControlRigSettingsPerPinBool& ExpansionMap = *ExpansionMapPtr;

				for (const TPair<FString, bool>& Pair : ExpansionMap.Values)
				{
					FString PinPath = FString::Printf(TEXT("%s.%s"), *ModelNode->GetName(), *Pair.Key);
					Controller->SetPinExpansion(PinPath, Pair.Value, true);
				}
			}
		}
#endif
		
		Controller->CloseUndoBracket();
	}

	return NewNode;
}


UEdGraphNode* FAnimNextParameterSchemaAction_DispatchFactory::PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode)
{
	UAnimNextParameterBlock_EditorData* EditorData = ParentGraph->GetTypedOuter<UAnimNextParameterBlock_EditorData>();
	UAnimNextParameterBlock_EdGraphNode* NewNode = nullptr;
	UAnimNextParameterBlock_EdGraph* EdGraph = Cast<UAnimNextParameterBlock_EdGraph>(ParentGraph);

	if (EditorData != nullptr && EdGraph != nullptr)
	{
		const FRigVMTemplate* Template = FRigVMRegistry::Get().FindTemplate(Notation);
		if (Template == nullptr)
		{
			return nullptr;
		}

		const int32 NotationHash = (int32)GetTypeHash(Notation);
		const FString TemplateName = TEXT("RigVMTemplate_") + FString::FromInt(NotationHash);

		FName Name = UE::AnimNext::Editor::FUtils::ValidateName(EditorData, Template->GetName().ToString());
		URigVMController* Controller = EditorData->GetRigVMClient()->GetController(ParentGraph);

		Controller->OpenUndoBracket(FString::Printf(TEXT("Add '%s' Node"), *Name.ToString()));

		if (URigVMTemplateNode* ModelNode = Controller->AddTemplateNode(Notation, Location, Name.ToString(), true, false))
		{
			NewNode = Cast<UAnimNextParameterBlock_EdGraphNode>(EdGraph->FindNodeForModelNodeName(ModelNode->GetFName()));

			if (NewNode)
			{
				Controller->ClearNodeSelection(true);
				Controller->SelectNode(ModelNode, true, true);
			}

			Controller->CloseUndoBracket();
		}
		else
		{
			Controller->CancelUndoBracket();
		}
	}
	return NewNode;
}