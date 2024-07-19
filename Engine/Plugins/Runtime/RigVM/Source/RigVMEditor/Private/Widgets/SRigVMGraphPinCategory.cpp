// Copyright Epic Games, Inc. All Rights Reserved.


#include "Widgets/SRigVMGraphPinCategory.h"
#include "Widgets/SNullWidget.h"

void SRigVMGraphPinCategory::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SRigVMGraphPinCategory::GetDefaultValueWidget()
{
	return SNullWidget::NullWidget;
}

