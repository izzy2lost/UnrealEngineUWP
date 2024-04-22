// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Geometries/DMXGDTFDisplayGeometry.h"

#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	void FDMXGDTFDisplayGeometry::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFGeometry::Initialize(XmlNode);

		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Texture"), Texture);
	}
}
