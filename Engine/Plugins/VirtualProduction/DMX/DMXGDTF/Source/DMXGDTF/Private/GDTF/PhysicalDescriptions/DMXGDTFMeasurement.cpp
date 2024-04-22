// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/PhysicalDescriptions/DMXGDTFMeasurement.h"

#include "GDTF/PhysicalDescriptions/DMXGDTFMeasurementPoint.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	void FDMXGDTFMeasurementBase::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.GetAttribute(TEXT("Physical"), Physical)
			.GetAttribute(TEXT("LuminousIntensity"), LuminousIntensity)
			.GetAttribute(TEXT("Transmission"), Transmission)
			.GetAttribute(TEXT("InterpolationTo"), InterpolationTo)
			.CreateChildren(TEXT("MeasurementPoints"), MeasurementPointArray);
	}

	FDMXGDTFEmitterMeasurement::FDMXGDTFEmitterMeasurement(const TWeakPtr<FDMXGDTFEmitter>& InEmitter)
		: OuterEmitter(InEmitter)
	{}

	FDMXGDTFFilterMeasurement::FDMXGDTFFilterMeasurement(const TSharedRef<FDMXGDTFFilter>& InFilter)
		: OuterFilter(InFilter)
	{}
}
