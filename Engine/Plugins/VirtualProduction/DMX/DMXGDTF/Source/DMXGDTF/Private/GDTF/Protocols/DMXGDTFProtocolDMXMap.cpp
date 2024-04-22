// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Protocols/DMXGDTFProtocolDMXMap.h"

#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	void FDMXGDTFProtocolDMXMapBase::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Key"), Key)
			.GetAttribute(TEXT("Value"), Value);
	}

	namespace ArtNet
	{
		FDMXGDTFProtocolArtNetDMXMap::FDMXGDTFProtocolArtNetDMXMap(const TWeakPtr<ArtNet::FDMXGDTFProtocolArtNet>& InProtocolArtNet)
			: OuterProtocolArtNet(InProtocolArtNet)
		{}
	}

	namespace SACN
	{
		FDMXGDTFProtocolSACNDMXMap::FDMXGDTFProtocolSACNDMXMap(const TSharedRef<FDMXGDTFProtocolSACN>& InProtocolSACN)
			: OuterProtocolSACN(InProtocolSACN)
		{}
	}
}
