// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Geometries/DMXGDTFLaserProtocol.h"

#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFLaserProtocol::FDMXGDTFLaserProtocol(const TSharedRef<FDMXGDTFLaserGeometry>& InLaserGeometry)
		: OuterLaserGeometry(InLaserGeometry)
	{}

	void FDMXGDTFLaserProtocol::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Name"), Name);
	}
}
