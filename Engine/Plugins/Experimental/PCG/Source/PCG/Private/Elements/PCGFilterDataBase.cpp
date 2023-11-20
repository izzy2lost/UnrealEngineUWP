// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGFilterDataBase.h"

#include "PCGCustomVersion.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "Helpers/PCGSettingsHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGFilterDataBase)

#if WITH_EDITOR
void UPCGFilterDataBaseSettings::ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins)
{
	Super::ApplyDeprecationBeforeUpdatePins(InOutNode, InputPins, OutputPins);

	if (DataVersion < FPCGCustomVersion::UpdateFilterNodeOutputPins)
	{
		InOutNode->RenameOutputPin(PCGPinConstants::DefaultOutputLabel, PCGPinConstants::DefaultInFilterLabel);
	}
}

#endif // WITH_EDITOR

EPCGDataType UPCGFilterDataBaseSettings::GetCurrentPinTypes(const UPCGPin* InPin) const
{
	check(InPin);
	if (!InPin->IsOutputPin())
	{
		return Super::GetCurrentPinTypes(InPin);
	}

	// Output pin narrows to union of inputs on first pin
	const EPCGDataType InputTypeUnion = GetTypeUnionOfIncidentEdges(PCGPinConstants::DefaultInputLabel);
	return InputTypeUnion != EPCGDataType::None ? InputTypeUnion : EPCGDataType::Any;
}

TArray<FPCGPinProperties> UPCGFilterDataBaseSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Any);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGFilterDataBaseSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInFilterLabel, EPCGDataType::Any);
	PinProperties.Emplace(PCGPinConstants::DefaultOutFilterLabel, EPCGDataType::Any);

	return PinProperties;
}