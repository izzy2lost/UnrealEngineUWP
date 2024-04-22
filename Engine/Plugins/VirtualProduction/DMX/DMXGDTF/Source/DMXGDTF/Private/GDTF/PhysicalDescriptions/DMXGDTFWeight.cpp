// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/PhysicalDescriptions/DMXGDTFWeight.h"

#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFWeight::FDMXGDTFWeight(const TSharedRef<FDMXGDTFProperties>& InProperties)
		: OuterProperties(InProperties)
	{}

	void FDMXGDTFWeight::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Value"), Value);
	}
}
