// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Protocols/sACN/DMXGDTFProtocolSACN.h"

#include "GDTF/Protocols/DMXGDTFProtocolDMXMap.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF::SACN
{
	FDMXGDTFProtocolSACN::FDMXGDTFProtocolSACN(const TSharedRef<FDMXGDTFProtocols>& InProtocols)
		: OuterProtocols(InProtocols)
	{}

	void FDMXGDTFProtocolSACN::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.CreateChildren(TEXT("Map"), Maps);
	}
}
