// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/DMXModes/DMXGDTFMacroDMXStep.h"

#include "GDTF/DMXModes/DMXGDTFMacroDMXValue.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFMacroDMXStep::FDMXGDTFMacroDMXStep(const TSharedRef<FDMXGDTFMacroDMX>& InMacroDMX)
		: OuterMacroDMX(InMacroDMX)
	{}

	void FDMXGDTFMacroDMXStep::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Duration"), Duration)
			.CreateChildren(TEXT("MacroDMXValue"), MacroDMXValueArray);
	}
}
