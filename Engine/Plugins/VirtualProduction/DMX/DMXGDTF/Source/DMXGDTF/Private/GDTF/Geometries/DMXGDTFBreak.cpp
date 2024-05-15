// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Geometries/DMXGDTFBreak.h"

#include "Serialization/DMXGDTFNodeInitializer.h"
#include "Serialization/DMXGDTFXmlNodeBuilder.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFBreak::FDMXGDTFBreak(const TSharedRef<FDMXGDTFGeometryReference>& InGeometryReference)
		: OuterGeometryReference(InGeometryReference)
	{}

	void FDMXGDTFBreak::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("DMXOffset"), DMXOffset)
			.GetAttribute(TEXT("DMXBreak"), DMXBreak);
	}

	FXmlNode* FDMXGDTFBreak::CreateXmlNode(FXmlNode& Parent)
	{
		const FDMXGDTFXmlNodeBuilder ChildBuilder = FDMXGDTFXmlNodeBuilder(Parent, *this)
			.SetAttribute(TEXT("DMXOffset"), DMXOffset)
			.SetAttribute(TEXT("DMXBreak"), DMXBreak);

		return ChildBuilder.GetIntermediateXmlNode();
	}
}
