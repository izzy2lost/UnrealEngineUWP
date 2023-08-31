// Copyright Epic Games, Inc. All Rights Reserved.


#include "MaterialPins/SGraphPinMaterialInput.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Casts.h"
#include "SGraphSubstrateMaterial.h"
#include "MaterialValueType.h"

void SGraphPinMaterialInput::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	bool bUsePinColor = true;
	if (Strata::IsStrataEnabled())
	{
		if (UMaterialGraphSchema::GetMaterialValueType(InGraphPinObj) == MCT_Strata)
		{
			bUsePinColor = false;
		}			
	}
	SGraphPin::Construct(SGraphPin::FArguments().UsePinColorForText(bUsePinColor), InGraphPinObj);
}

FSlateColor SGraphPinMaterialInput::GetPinColor() const
{
	check(GraphPinObj);
	UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(GraphPinObj->GetOwningNode()->GetGraph());
	const UMaterialGraphSchema* Schema = CastChecked<UMaterialGraphSchema>(MaterialGraph->GetSchema());

	if (MaterialGraph->IsInputActive(GraphPinObj))
	{
		if (Strata::IsStrataEnabled())
		{
			if (UMaterialGraphSchema::GetMaterialValueType(GraphPinObj) == MCT_Strata)
			{
				return FSlateColor(FSubstrateWidget::GetConnectionColor());
			}			
		}
		return Schema->ActivePinColor;
	}
	else
	{
		return Schema->InactivePinColor;
	}
}
