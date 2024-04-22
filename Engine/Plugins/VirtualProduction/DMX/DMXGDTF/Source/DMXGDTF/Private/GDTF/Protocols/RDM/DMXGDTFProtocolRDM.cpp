// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Protocols/RDM/DMXGDTFProtocolRDM.h"

#include "GDTF/Protocols/RDM/DMXGDTFSoftwareVersionID.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF::RDM
{
	FDMXGDTFProtocolRDM::FDMXGDTFProtocolRDM(const TSharedRef<FDMXGDTFProtocols>& InProtocols)
		: OuterProtocols(InProtocols)
	{}

	void FDMXGDTFProtocolRDM::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("ManufacturerID"), ManufacturerID, &FParse::HexNumber)
			.GetAttribute(TEXT("DeviceModelID"), DeviceModelID, &FParse::HexNumber)
			.CreateChildren(TEXT("SoftwareVersionID"), SoftwareVersionIDArray);
	}
}
