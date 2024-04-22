// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Protocols/DMXGDTFProtocols.h"

#include "GDTF/Protocols/RDM/DMXGDTFProtocolRDM.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFProtocols::FDMXGDTFProtocols(const TSharedRef<FDMXGDTFFixtureType>& InFixtureType)
		: OuterFixtureType(InFixtureType)
	{}

	void FDMXGDTFProtocols::Initialize(const FXmlNode& XmlNode)
	{
		using namespace RDM;

		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.CreateOptionalChild(TEXT("RDM"), RDM);
	}
}
