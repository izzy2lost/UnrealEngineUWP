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
