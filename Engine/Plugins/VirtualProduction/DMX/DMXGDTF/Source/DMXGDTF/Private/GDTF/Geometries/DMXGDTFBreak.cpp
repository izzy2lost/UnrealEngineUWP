// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Geometries/DMXGDTFBreak.h"

#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFBreak::FDMXGDTFBreak(const TSharedRef<FDMXGDTFGeometryReference>& InGeometryReference)
		: OuterGeometryReference(InGeometryReference)
	{}

	void FDMXGDTFBreak::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("DMXAddress"), DMXAddress)
			.GetAttribute(TEXT("DMXBreak"), DMXBreak);
	}
}
