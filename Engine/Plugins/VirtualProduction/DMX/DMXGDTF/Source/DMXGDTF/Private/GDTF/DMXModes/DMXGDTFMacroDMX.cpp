// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/DMXModes/DMXGDTFMacroDMX.h"

#include "GDTF/DMXModes/DMXGDTFMacroDMXStep.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFMacroDMX::FDMXGDTFMacroDMX(const TSharedRef<FDMXGDTFFTMacro>& InFTMacro)
		: OuterFTMacro(InFTMacro)
	{}

	void FDMXGDTFMacroDMX::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.CreateChildren(TEXT("MacroDMXStep"), MacroDMXStepArray);
	}
}
