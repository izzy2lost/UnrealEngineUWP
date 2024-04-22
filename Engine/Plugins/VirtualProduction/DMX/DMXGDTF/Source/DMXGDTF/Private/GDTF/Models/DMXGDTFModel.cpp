// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Models/DMXGDTFModel.h"

#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFModel::FDMXGDTFModel(const TSharedRef<FDMXGDTFFixtureType>& InFixtureType)
		: OuterFixtureType(InFixtureType)
	{}

	void FDMXGDTFModel::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Name"), Name)
			.GetAttribute(TEXT("Width"), Width)
			.GetAttribute(TEXT("Height"), Height)
			.GetAttribute(TEXT("PrimitiveType"), PrimitiveType)
			.GetAttribute(TEXT("File"), File)
			.GetAttribute(TEXT("SVGOffsetX"), SVGOffsetX)
			.GetAttribute(TEXT("SVGOffsetY"), SVGOffsetY)
			.GetAttribute(TEXT("SVGSideOffsetX"), SVGSideOffsetX)
			.GetAttribute(TEXT("SVGSideOffsetY"), SVGSideOffsetY)
			.GetAttribute(TEXT("SVGFrontOffsetX"), SVGFrontOffsetX)
			.GetAttribute(TEXT("SVGFrontOffsetY"), SVGFrontOffsetY);
	}
}
