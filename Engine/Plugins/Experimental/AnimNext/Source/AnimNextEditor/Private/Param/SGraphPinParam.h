// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphPin.h"
#include "Param/AnimNextEditorParam.h"
#include "Param/AnimNextParam.h"
#include "Param/ParamType.h"

class URigVMGraph;
enum class ERigVMGraphNotifType : uint8;
class URigVMPin;
class URigVMEdGraphNode;

namespace UE::AnimNext::Editor
{
	class SParameterPickerCombo;
}

namespace UE::AnimNext::Editor
{

// A pin widget that allows picking using an AnimNext parameter picker
class SGraphPinParam : public SGraphPin
{
	SLATE_BEGIN_ARGS(SGraphPinParam)
		: _ModelPin(nullptr)
		, _GraphNode(nullptr)
	{}

	SLATE_ARGUMENT(URigVMPin*, ModelPin)

	SLATE_ARGUMENT(URigVMEdGraphNode*, GraphNode)

	SLATE_ARGUMENT(FAnimNextParamType, FilterType)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

private:
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;

	void UpdateCachedParam();

	void HandleGraphModified(ERigVMGraphNotifType InType, URigVMGraph* InGraph, UObject* InSubject);

	URigVMPin* ModelPin = nullptr;

	URigVMEdGraphNode* Node = nullptr;

	FAnimNextParamType FilterType;

	FAnimNextEditorParam CachedParam;

	TSharedPtr<SParameterPickerCombo> PickerCombo;

	UScriptStruct* ParamStruct = nullptr; 
};

}