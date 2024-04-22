// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Protocols/RDM/DMXGDTFSoftwareVersionID.h"

#include "GDTF/Protocols/RDM/DMXGDTFDMXPersonality.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF::RDM
{
	FDMXGDTFSoftwareVersionID::FDMXGDTFSoftwareVersionID(const TSharedRef<FDMXGDTFProtocolRDM>& InProtocolRDM)
		: OuterProtocolRDM(InProtocolRDM)
	{}

	void FDMXGDTFSoftwareVersionID::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Value"), Value, &FParse::HexNumber)
			.CreateChildren(TEXT("DMXPersonality"), DMXPersonalityArray);
	}
}
