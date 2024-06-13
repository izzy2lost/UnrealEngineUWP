// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

struct FDMXFixtureFunction;
struct FDMXFixtureCellAttribute;
struct FDMXFixtureMode;
class FXmlFile;
class UDMXEntityFixtureType;

namespace UE::DMX::GDTF
{
	class FDMXGDTFAttributeDefinitions;
	class FDMXGDTFDMXChannel;
	class FDMXGDTFDMXMode;
	class FDMXGDTFFixtureType;
	class FDMXGDTFGeometry;
	class FDMXGDTFGeometryCollect;
	class FDMXGDTFGeometryReference;
	class FDMXGDTFLogicalChannel;

	/** Converts a Fixture Type to a GDTF. Internally caches of each collect, and finally assembles the GDTF. */
	class FDMXFixtureTypeToGTDFConverter
	{
	public:
		/** Converts the Fixture Type to a GDTF description */
		static TSharedPtr<FXmlFile> Convert(const UDMXEntityFixtureType* UnrealFixtureType);

	private:
		/** Creates the Fixture Type */
		TSharedRef<FDMXGDTFFixtureType> CreateFixtureType(const UDMXEntityFixtureType* UnrealFixtureType);

		/** Creates attribute definitions */
		void CreateAttributeDefinitions(const UDMXEntityFixtureType* UnrealFixtureType, const TSharedRef<FDMXGDTFFixtureType>& GDTFFixtureType);

		/** Creates model */
		void CreateModels(const UDMXEntityFixtureType* UnrealFixtureType, const TSharedRef<FDMXGDTFFixtureType>& GDTFFixtureType);

		/** Creates the geometry collect */
		void CreateGeometryCollect(const UDMXEntityFixtureType* UnrealFixtureType, const TSharedRef<FDMXGDTFFixtureType>& GDTFFixtureType);

		/** Creates Child geometries inside a root geometry */
		void CreateChildGeometries(const FDMXFixtureMode& UnrealMode, const TSharedRef<FDMXGDTFGeometry>& RootGeometry);

		/** Creates DMX Modes */
		void CreateDMXModes(const UDMXEntityFixtureType* UnrealFixtureType, const TSharedRef<FDMXGDTFFixtureType>& GDTFFixtureType);

		/** Creates DMX Channels for specified mode */
		void CreateDMXChannels(const FDMXFixtureMode& UnrealMode, const TSharedRef<FDMXGDTFDMXMode>& GDTFDMXMode);

		/** Creates a Logical Channel for the DMX Channel, using an Unreal Function for as input */
		void CreateLogicalChannel(const FDMXFixtureFunction& UnrealFunction, const TSharedRef<FDMXGDTFDMXChannel>& GDTFDMXChannel, const FString& GDTFAttribute);

		/** Creates a Logical Channel for the DMX Channel, using an Unreal Cell Attribute as the input */
		void CreateLogicalChannel(const FDMXFixtureCellAttribute& UnrealCellAttribute, const TSharedRef<FDMXGDTFDMXChannel>& GDTFDMXChannel, const FString& GDTFAttribute);

		/** Creates a Channel Function for the Logical Channel, using an Unreal Function for as input */
		void CrateChannelFunction(const FDMXFixtureFunction& UnrealFunction, const TSharedRef<FDMXGDTFLogicalChannel>& GDTFLogicalChannel, const FString& GDTFAttribute);

		/** Creates a Channel Function for the Logical Channel, using an Unreal Cell Attribute as the input */
		void CrateChannelFunction(const FDMXFixtureCellAttribute& UnrealCellAttribute, const TSharedRef<FDMXGDTFLogicalChannel>& GDTFLogicalChannel, const FString& GDTFAttribute);

		/** Map of Unreal Modes to the root geometry they control */
		TMap<const FDMXFixtureMode*, TSharedRef<FDMXGDTFGeometry>> UnrealModeToRootGeometryMap;

		/** Map of Unreal Fixture Functions to the geometry they control */
		TMap<const FDMXFixtureFunction*, TSharedRef<FDMXGDTFGeometry>> UnrealFunctionToGeometryMap;

		/** Map of Unreal Matrix Attribute to the geometry they control */
		TMap<const FDMXFixtureCellAttribute*, TSharedRef<FDMXGDTFGeometryReference>> UnrealCellAttributeToGeometryReferenceMap;

		/** The name of a matrix cell geometry reference */
		static const FName MatrixCellGeometryReferenceName;
	};
}
