// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Protocols/RDM/DMXGDTFDMXPersonality.h"

#include "Algo/Find.h"
#include "GDTF/DMXModes/DMXGDTFDMXMode.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF::RDM
{
	FDMXGDTFDMXPersonality::FDMXGDTFDMXPersonality(const TSharedRef<FDMXGDTFSoftwareVersionID>& InSoftwareVersionID)
		: OuterSoftwareVersionID(InSoftwareVersionID)
	{}

	void FDMXGDTFDMXPersonality::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Value"), Value, &FParse::HexNumber)
			.GetAttribute(TEXT("DMXMode"), DMXMode);
	}

	TSharedPtr<FDMXGDTFDMXMode> FDMXGDTFDMXPersonality::ResolveDMXMode() const
	{
		if (TSharedPtr<FDMXGDTFFixtureType> FixtureType = GetFixtureType().Pin())
		{
			if (const TSharedPtr<FDMXGDTFDMXMode>* DMXModePtr = Algo::FindBy(FixtureType->DMXModes, DMXMode, &FDMXGDTFDMXMode::Name))
			{
				return *DMXModePtr;
			}
		}

		return nullptr;
	}
}
