// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/PhysicalDescriptions/DMXGDTFColorSpace.h"

#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFColorSpace::FDMXGDTFColorSpace(const TSharedRef<FDMXGDTFPhysicalDescriptions>& InPhysicalDescriptions)
		: OuterPhysicalDescriptions(InPhysicalDescriptions)
	{}

	void FDMXGDTFColorSpace::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Name"), Name)
			.GetAttribute(TEXT("Mode"), Mode)
			.GetAttribute(TEXT("Red"), Red)
			.GetAttribute(TEXT("Green"), Green)
			.GetAttribute(TEXT("Blue"), Blue)
			.GetAttribute(TEXT("WhitePoint"), WhitePoint);
	}
}
