// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphPin.h"

class URigVMPin;
class URigVMEdGraphNode;

namespace UE::AnimNext::Editor
{

// A pin widget that allows picking using an AnimNext parameter picker
class SGraphPinParamName : public SGraphPin
{
	SLATE_BEGIN_ARGS(SGraphPinParamName) {}

	SLATE_ARGUMENT(URigVMPin*, ModelPin)
	SLATE_ARGUMENT(URigVMEdGraphNode*, GraphNode)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

private:
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;

	URigVMPin* ModelPin = nullptr;
	URigVMEdGraphNode* Node = nullptr;
};

}