// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGReroute.h"

#include "PCGContext.h"
#include "PCGEdge.h"
#include "PCGNode.h"
#include "PCGPin.h"

UPCGRerouteSettings::UPCGRerouteSettings()
{
#if WITH_EDITORONLY_DATA
	bExposeToLibrary = false;

	// Reroutes don't support disabling or debugging.
	bDisplayDebuggingProperties = false;
	DebugSettings.bDisplayProperties = false;
	bEnabled = true;
	bDebug = false;
#endif
}

TArray<FPCGPinProperties> UPCGRerouteSettings::InputPinProperties() const
{
	FPCGPinProperties PinProperties;
	PinProperties.Label = PCGPinConstants::DefaultInputLabel;
	PinProperties.SetAllowMultipleConnections(false);
	PinProperties.AllowedTypes = EPCGDataType::Any;

	return { PinProperties };
}

TArray<FPCGPinProperties> UPCGRerouteSettings::OutputPinProperties() const
{
	FPCGPinProperties PinProperties;
	PinProperties.Label = PCGPinConstants::DefaultOutputLabel;
	PinProperties.AllowedTypes = EPCGDataType::Any;

	return { PinProperties };
}

FPCGElementPtr UPCGRerouteSettings::CreateElement() const
{
	return MakeShared<FPCGRerouteElement>();
}

TArray<FPCGPinProperties> UPCGNamedRerouteDeclarationSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	// Default visible pin
	PinProperties.Emplace(
		PCGPinConstants::DefaultOutputLabel, 
		EPCGDataType::Any, 
		/*bAllowMultipleConnections=*/true, 
		/*bAllowMultipleData=*/true);

	FPCGPinProperties& InvisiblePin = PinProperties.Emplace_GetRef(
		PCGNamedRerouteConstants::InvisiblePinLabel,
		EPCGDataType::Any,
		/*bAllowMultipleConnections=*/true,
		/*bAllowMultipleData=*/true);
	InvisiblePin.bInvisiblePin = true;
	
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGNamedRerouteUsageSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (ensure(PinProperties.Num() == 1))
	{
		PinProperties[0].bInvisiblePin = true;
	}

	return PinProperties;
}

EPCGDataType UPCGNamedRerouteUsageSettings::GetCurrentPinTypes(const UPCGPin* InPin) const
{
	// Defer to declaration if possible
	return Declaration ? Declaration->GetCurrentPinTypes(InPin) : Super::GetCurrentPinTypes(InPin);
}

bool FPCGRerouteElement::ExecuteInternal(FPCGContext* Context) const
{
	ensureMsgf(false, TEXT("Reroute elements are not supposed to execute - reroutes should be culled during graph compilation."));
	
	Context->OutputData = Context->InputData;
	for (FPCGTaggedData& Output : Context->OutputData.TaggedData)
	{
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
	}

	return true;
}
