// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Geometries/DMXGDTFSupportGeometry.h"

#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	void FDMXGDTFSupportGeometry::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFGeometry::Initialize(XmlNode);

		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("SupportType"), SupportType)
			.GetAttribute(TEXT("RopeCrossSection"), RopeCrossSection)
			.GetAttribute(TEXT("RopeOffset"), RopeOffset)
			.GetAttribute(TEXT("CapacityX"), CapacityX)
			.GetAttribute(TEXT("CapacityY"), CapacityY)
			.GetAttribute(TEXT("CapacityZ"), CapacityZ)
			.GetAttribute(TEXT("CapacityXX"), CapacityXX)
			.GetAttribute(TEXT("CapacityYY"), CapacityYY)
			.GetAttribute(TEXT("CapacityZZ"), CapacityZZ)
			.GetAttribute(TEXT("ResistanceX"), ResistanceX)
			.GetAttribute(TEXT("ResistanceY"), ResistanceY)
			.GetAttribute(TEXT("ResistanceZ"), ResistanceZ)
			.GetAttribute(TEXT("ResistanceXX"), ResistanceXX)
			.GetAttribute(TEXT("ResistanceYY"), ResistanceYY)
			.GetAttribute(TEXT("ResistanceZZ"), ResistanceZZ);
	}
}
