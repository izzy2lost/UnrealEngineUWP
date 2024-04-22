// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Geometries/DMXGDTFGeometryCollect.h"

#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFGeometryCollect::FDMXGDTFGeometryCollect(const TSharedRef<FDMXGDTFFixtureType>& InFixtureType)
		: OuterFixtureType(InFixtureType)
	{}

	void FDMXGDTFGeometryCollect::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFGeometryCollectBase::Initialize(XmlNode);
	}
}
