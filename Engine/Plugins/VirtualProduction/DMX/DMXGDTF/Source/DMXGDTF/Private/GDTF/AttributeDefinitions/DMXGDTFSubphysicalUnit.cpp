// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/AttributeDefinitions/DMXGDTFSubphysicalUnit.h"

#include "GDTF/AttributeDefinitions/DMXGDTFPhysicalUnit.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFSubphysicalUnit::FDMXGDTFSubphysicalUnit(const TSharedRef<FDMXGDTFAttribute>& InAttribute)
		: OuterAttribute(InAttribute)
	{}

	void FDMXGDTFSubphysicalUnit::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Type"), Type)
			.GetAttribute(TEXT("PhysicalUnit"), PhysicalUnit)
			.GetAttribute(TEXT("PhysicalFrom"), PhysicalFrom)
			.GetAttribute(TEXT("PhysicalTo"), PhysicalTo);
	}
}
