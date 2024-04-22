// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/PhysicalDescriptions/DMXGDTFMeasurementPoint.h"

#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	FDMXGDTFMeasurementPoint::FDMXGDTFMeasurementPoint(const TSharedRef<FDMXGDTFMeasurementBase>& InMeasurement)
		: OuterMeasurement(InMeasurement)
	{}

	void FDMXGDTFMeasurementPoint::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("WaveLength"), WaveLength)
			.GetAttribute(TEXT("Energy"), Energy);
	}
}
