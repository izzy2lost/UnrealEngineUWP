// Copyright Epic Games, Inc. All Rights Reserved.

#include "GDTF/Geometries/DMXGDTFGeometryCollectBase.h"

#include "GDTF/Geometries/DMXGDTFAxisGeometry.h"
#include "GDTF/Geometries/DMXGDTFBeamGeometry.h"
#include "GDTF/Geometries/DMXGDTFDisplayGeometry.h"
#include "GDTF/Geometries/DMXGDTFFilterBeamGeometry.h"
#include "GDTF/Geometries/DMXGDTFFilterColorGeometry.h"
#include "GDTF/Geometries/DMXGDTFFilterGoboGeometry.h"
#include "GDTF/Geometries/DMXGDTFFilterShaperGeometry.h"
#include "GDTF/Geometries/DMXGDTFGeometry.h"
#include "GDTF/Geometries/DMXGDTFGeometryReference.h"
#include "GDTF/Geometries/DMXGDTFInventoryGeometry.h"
#include "GDTF/Geometries/DMXGDTFLaserGeometry.h"
#include "GDTF/Geometries/DMXGDTFMagnetGeometry.h"
#include "GDTF/Geometries/DMXGDTFMagnetGeometry.h"
#include "GDTF/Geometries/DMXGDTFMediaServerCameraGeometry.h"
#include "GDTF/Geometries/DMXGDTFMediaServerLayerGeometry.h"
#include "GDTF/Geometries/DMXGDTFMediaServerMasterGeometry.h"
#include "GDTF/Geometries/DMXGDTFStructureGeometry.h"
#include "GDTF/Geometries/DMXGDTFSupportGeometry.h"
#include "GDTF/Geometries/DMXGDTFWiringObjectGeometry.h"
#include "Serialization/DMXGDTFNodeInitializer.h"

namespace UE::DMX::GDTF
{
	const TCHAR* FDMXGDTFGeometryCollectBase::GetXmlTag() const
	{
		ensureMsgf(0, TEXT("Unexpected call to FDMXGDTFGeometryCollectBase::GetXmlTag in abstract FDMXGDTFGeometryCollectBase."));
		return TEXT("Invalid");
	}

	void FDMXGDTFGeometryCollectBase::Initialize(const FXmlNode& XmlNode)
	{
		FDMXGDTFNodeInitializer(SharedThis(this), XmlNode)
			.CreateChildren(TEXT("Geometry"), GeometryArray)
			.CreateChildren(TEXT("Axis"), AxisArray)
			.CreateChildren(TEXT("FilterBeam"), FilterBeamArray)
			.CreateChildren(TEXT("FilterColor"), FilterColorArray)
			.CreateChildren(TEXT("FilterGobo"), FilterGoboArray)
			.CreateChildren(TEXT("FilterShaper"), FilterShaperArray)
			.CreateChildren(TEXT("Beam"), BeamArray)
			.CreateChildren(TEXT("MediaServerLayer"), MediaServerLayerArray)
			.CreateChildren(TEXT("MediaServerCamera"), MediaServerCameraArray)
			.CreateChildren(TEXT("MediaServerMaster"), MediaServerMasterArray)
			.CreateChildren(TEXT("Display"), DisplayArray)
			.CreateChildren(TEXT("GeometryReference"), GeometryReferenceArray)
			.CreateChildren(TEXT("Laser"), LaserArray)
			.CreateChildren(TEXT("WiringObject"), WiringObjectArray)
			.CreateChildren(TEXT("Inventory"), InventoryArray)
			.CreateChildren(TEXT("Structure"), StructureArray)
			.CreateChildren(TEXT("Support"), SupportArray)
			.CreateChildren(TEXT("Magnet"), MagnetArray);
	}

	void FDMXGDTFGeometryCollectBase::FindGeometryByName(const TCHAR* InName, TSharedPtr<FDMXGDTFGeometry>& OutGeometry, TSharedPtr<FDMXGDTFGeometryReference>& OutGeometryReference) const
	{
		const TSharedPtr<FDMXGDTFGeometryReference>* GeometryReferencePtr = Algo::FindBy(GeometryReferenceArray, InName, &FDMXGDTFGeometryReference::Name);
		if (GeometryReferencePtr)
		{
			OutGeometryReference = *GeometryReferencePtr;
			return;
		}

		// Helper to find geometry nodes in arrays of different types
		auto FindInArrayLambda = [InName](auto InArray, TSharedPtr<FDMXGDTFGeometry>& OutGeometry) -> bool
			{
				auto const* GeometryPtr = Algo::FindBy(InArray, InName, &FDMXGDTFGeometry::Name);
				OutGeometry = GeometryPtr ? *GeometryPtr : nullptr;
				return GeometryPtr != nullptr;
			};

		TSharedPtr<FDMXGDTFGeometry> Geometry;
		if (FindInArrayLambda(GeometryArray, Geometry)) { OutGeometry = Geometry; }
		else if (FindInArrayLambda(AxisArray, Geometry)) { OutGeometry = Geometry; }
		else if (FindInArrayLambda(FilterBeamArray, Geometry)) { OutGeometry = Geometry; }
		else if (FindInArrayLambda(FilterColorArray, Geometry)) { OutGeometry = Geometry; }
		else if (FindInArrayLambda(FilterGoboArray, Geometry)) { OutGeometry = Geometry; }
		else if (FindInArrayLambda(FilterShaperArray, Geometry)) { OutGeometry = Geometry; }
		else if (FindInArrayLambda(BeamArray, Geometry)) { OutGeometry = Geometry; }
		else if (FindInArrayLambda(MediaServerLayerArray, Geometry)) { OutGeometry = Geometry; }
		else if (FindInArrayLambda(MediaServerCameraArray, Geometry)) { OutGeometry = Geometry; }
		else if (FindInArrayLambda(MediaServerMasterArray, Geometry)) { OutGeometry = Geometry; }
		else if (FindInArrayLambda(DisplayArray, Geometry)) { OutGeometry = Geometry; }
		else if (FindInArrayLambda(LaserArray, Geometry)) { OutGeometry = Geometry; }
		else if (FindInArrayLambda(WiringObjectArray, Geometry)) { OutGeometry = Geometry; }
		else if (FindInArrayLambda(InventoryArray, Geometry)) { OutGeometry = Geometry; }
		else if (FindInArrayLambda(StructureArray, Geometry)) { OutGeometry = Geometry; }
		else if (FindInArrayLambda(SupportArray, Geometry)) { OutGeometry = Geometry; }
		else if (FindInArrayLambda(MagnetArray, Geometry)) { OutGeometry = Geometry; }
	}
}
