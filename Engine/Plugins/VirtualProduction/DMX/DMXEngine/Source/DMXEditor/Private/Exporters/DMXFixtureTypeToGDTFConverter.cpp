// Copyright Epic Games, Inc. All Rights Reserved.

#include "Exporters/DMXFixtureTypeToGDTFConverter.h"

#include "Algo/Find.h"
#include "Algo/Sort.h"
#include "DMXGDTF.h"
#include "DMXUnrealToGDTFAttributeConversion.h"
#include "GDTF/AttributeDefinitions/DMXGDTFAttribute.h"
#include "GDTF/AttributeDefinitions/DMXGDTFAttributeDefinitions.h"
#include "GDTF/AttributeDefinitions/DMXGDTFFeature.h"
#include "GDTF/AttributeDefinitions/DMXGDTFFeatureGroup.h"
#include "GDTF/AttributeDefinitions/DMXGDTFPhysicalUnit.h"
#include "GDTF/DMXGDTFFixtureType.h"
#include "GDTF/DMXModes/DMXGDTFChannelFunction.h"
#include "GDTF/DMXModes/DMXGDTFDMXChannel.h"
#include "GDTF/DMXModes/DMXGDTFDMXMode.h"
#include "GDTF/DMXModes/DMXGDTFLogicalChannel.h"
#include "GDTF/Geometries/DMXGDTFBeamGeometry.h"
#include "GDTF/Geometries/DMXGDTFGeometryBreak.h"
#include "GDTF/Geometries/DMXGDTFGeometryCollect.h"
#include "GDTF/Geometries/DMXGDTFGeometryReference.h"
#include "GDTF/Models/DMXGDTFModel.h"
#include "Interfaces/IPluginManager.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXLibrary.h"
#include "XmlFile.h"

namespace UE::DMX::GDTF
{
	const FName FDMXFixtureTypeToGTDFConverter::MatrixCellGeometryReferenceName = TEXT("Instance");

	TSharedPtr<FXmlFile> FDMXFixtureTypeToGTDFConverter::Convert(const UDMXEntityFixtureType* UnrealFixtureType)
	{
		FDMXFixtureTypeToGTDFConverter Converter;
		const TSharedRef<FDMXGDTFFixtureType> GDTFFixtureType = Converter.CreateFixtureType(UnrealFixtureType);

		// Create the XML file
		UDMXGDTF* GDTF = NewObject<UDMXGDTF>();
		GDTF->InitializeFromFixtureType(GDTFFixtureType);
		const TSharedPtr<FXmlFile> DescriptionXml = GDTF->ExportAsXml();

		return DescriptionXml;
	}

	TSharedRef<FDMXGDTFFixtureType> FDMXFixtureTypeToGTDFConverter::CreateFixtureType(const UDMXEntityFixtureType* UnrealFixtureType)
	{
		const TSharedRef<FDMXGDTFFixtureType> GDTFFixtureType = MakeShared<FDMXGDTFFixtureType>();

		GDTFFixtureType->Name = *UnrealFixtureType->Name;
		GDTFFixtureType->ShortName = *UnrealFixtureType->Name;
		GDTFFixtureType->LongName = UnrealFixtureType->GetParentLibrary()->GetName() + TEXT(" ") + UnrealFixtureType->Name;
		GDTFFixtureType->Manufacturer = TEXT("Epic Games");
		GDTFFixtureType->Description = FString::Printf(TEXT("Unreal Engine generated fixture type"));
		GDTFFixtureType->FixtureTypeID = FGuid::NewGuid(); // Avoid any ambiguity with previously exported GDTFs, even if they're identical.
		GDTFFixtureType->bCanHaveChildren = false;

		CreateAttributeDefinitions(UnrealFixtureType, GDTFFixtureType);
		CreateModels(UnrealFixtureType, GDTFFixtureType);
		CreateGeometryCollect(UnrealFixtureType, GDTFFixtureType);
		CreateDMXModes(UnrealFixtureType, GDTFFixtureType);

		return GDTFFixtureType;
	}

	void FDMXFixtureTypeToGTDFConverter::CreateAttributeDefinitions(const UDMXEntityFixtureType* UnrealFixtureType, const TSharedRef<FDMXGDTFFixtureType>& GDTFFixtureType)
	{
		TArray<FName> AttributeNames;
		for (const FDMXFixtureMode& Mode : UnrealFixtureType->Modes)
		{
			for (const FDMXFixtureFunction& Function : Mode.Functions)
			{
				AttributeNames.Add(Function.Attribute.Name);
			}

			if (Mode.bFixtureMatrixEnabled)
			{
				for (const FDMXFixtureCellAttribute& MatrixAttribute : Mode.FixtureMatrixConfig.CellAttributes)
				{
					AttributeNames.Add(MatrixAttribute.Attribute.Name);
				}
			}
		}

		GDTFFixtureType->AttributeDefinitions = MakeShared<FDMXGDTFAttributeDefinitions>(GDTFFixtureType);
		for (const FName& AttributeName : AttributeNames)
		{
			const FName GDTFAttributeName = FDMXUnrealToGDTFAttributeConversion::ConvertUnrealToGDTFAttribute(AttributeName);
			
			const FName PrettyName = FDMXUnrealToGDTFAttributeConversion::GetPrettyFromGDTFAttribute(GDTFAttributeName);
			const FName FeatureGroupName = FDMXUnrealToGDTFAttributeConversion::GetFeatureGroupForGDTFAttribute(GDTFAttributeName);
			const FName FeatureName = FDMXUnrealToGDTFAttributeConversion::GetFeatureForGDTFAttribute(GDTFAttributeName);

			// Get or create the GDTF feature group
			const TSharedRef<FDMXGDTFFeatureGroup> GDTFFeatureGroup = [&GDTFFixtureType, &FeatureGroupName]()
				{		
					const TSharedPtr<FDMXGDTFFeatureGroup>* GDTFFeatureGroupPtr = Algo::FindBy(GDTFFixtureType->AttributeDefinitions->FeatureGroups, FeatureGroupName, &FDMXGDTFFeatureGroup::Name);

					if (GDTFFeatureGroupPtr)
					{
						return (*GDTFFeatureGroupPtr).ToSharedRef();
					}
					else
					{
						const TSharedRef<FDMXGDTFFeatureGroup> NewFeatureGroup = MakeShared<FDMXGDTFFeatureGroup>(GDTFFixtureType->AttributeDefinitions.ToSharedRef());
						GDTFFixtureType->AttributeDefinitions->FeatureGroups.Add(NewFeatureGroup);

						return NewFeatureGroup;
					}
				}();

			GDTFFeatureGroup->Name = FeatureGroupName;

			// Get or create the GDTF feature
			const TSharedRef<FDMXGDTFFeature> GDTFFeature = [GDTFFeatureGroup, &FeatureName]()
				{					
					const TSharedPtr<FDMXGDTFFeature>* GDTFFeaturePtr = Algo::FindBy(GDTFFeatureGroup->FeatureArray, FeatureName, &FDMXGDTFFeature::Name);
					if (GDTFFeaturePtr)
					{
						return (*GDTFFeaturePtr).ToSharedRef();
					}
					else
					{
						const TSharedRef<FDMXGDTFFeature> NewFeature = MakeShared<FDMXGDTFFeature>(GDTFFeatureGroup);
						GDTFFeatureGroup->FeatureArray.Add(NewFeature);

						return NewFeature;
					}
				}();

			GDTFFeature->Name = FeatureName;

			// Create GDTF attribute
			const TSharedRef<FDMXGDTFAttribute> GDTFAttribute = MakeShared<FDMXGDTFAttribute>(GDTFFixtureType->AttributeDefinitions.ToSharedRef());
			GDTFFixtureType->AttributeDefinitions->Attributes.Add(GDTFAttribute);

			GDTFAttribute->Name = GDTFAttributeName;
			GDTFAttribute->Pretty = PrettyName.ToString();
			GDTFAttribute->PhysicalUnit = EDMXGDTFPhysicalUnit::None;
			GDTFAttribute->Feature = FeatureGroupName.ToString() + TEXT(".") + FeatureName.ToString();
		}
	}

	void FDMXFixtureTypeToGTDFConverter::CreateModels(const UDMXEntityFixtureType* UnrealFixtureType, const TSharedRef<FDMXGDTFFixtureType>& GDTFFixtureType)
	{
		// Create a model for the matrix if this is a matrix
		const bool bIsMatrix = Algo::FindBy(UnrealFixtureType->Modes, true, &FDMXFixtureMode::bFixtureMatrixEnabled) != nullptr;
		if (bIsMatrix)
		{
			const TSharedRef<FDMXGDTFModel> InstanceModel = MakeShared<FDMXGDTFModel>(GDTFFixtureType);
			GDTFFixtureType->Models.Add(InstanceModel);

			InstanceModel->Name = "Layers";
			InstanceModel->PrimitiveType = EDMXGDTFModelPrimitiveType::Cube;
			InstanceModel->Height = 0.01f;
			InstanceModel->Length = 1.f;
			InstanceModel->Width = 0.3f;
		}
	}

	void FDMXFixtureTypeToGTDFConverter::CreateGeometryCollect(const UDMXEntityFixtureType* UnrealFixtureType, const TSharedRef<FDMXGDTFFixtureType>& GDTFFixtureType)
	{
		GDTFFixtureType->GeometryCollect = MakeShared<FDMXGDTFGeometryCollect>(GDTFFixtureType);

		// Always add a base geometry
		const TSharedRef<FDMXGDTFGeometry> BaseGeometry = MakeShared<FDMXGDTFGeometry>(GDTFFixtureType->GeometryCollect.ToSharedRef());
		GDTFFixtureType->GeometryCollect->GeometryArray.Add(BaseGeometry);

		BaseGeometry->Name = TEXT("Base");

		const TSharedRef<FDMXGDTFBeamGeometry> BeamGeometry = MakeShared<FDMXGDTFBeamGeometry>(GDTFFixtureType->GeometryCollect.ToSharedRef());
		BaseGeometry->BeamArray.Add(BeamGeometry);

		BeamGeometry->Name = TEXT("Beam");

		for (const FDMXFixtureMode& UnrealMode : UnrealFixtureType->Modes)
		{				
			// Remember the geometry for this mode so it later can be referenced when building DMX Modes
			UnrealModeToRootGeometryMap.Add(&UnrealMode, BaseGeometry);

			CreateChildGeometries(UnrealMode, BeamGeometry);
		}
	}

	void FDMXFixtureTypeToGTDFConverter::CreateChildGeometries(const FDMXFixtureMode& UnrealMode, const TSharedRef<FDMXGDTFGeometry>& RootGeometry)
	{
		if (UnrealMode.bFixtureMatrixEnabled && UnrealMode.FixtureMatrixConfig.GetNumChannels() > 0)
		{
			// Get the byte size of an Unreal Matrix Cell
			const int32 CellSize = [UnrealMode]()
				{
					int32 OutCellSize = 0;
					for (const FDMXFixtureCellAttribute& CellAttribute : UnrealMode.FixtureMatrixConfig.CellAttributes)
					{
						OutCellSize += CellAttribute.GetNumChannels();
					}

					return OutCellSize;
				}();

			// Create Geometry References for each Unreal Matrix Cell
			const int32 NumCells = UnrealMode.FixtureMatrixConfig.XCells * UnrealMode.FixtureMatrixConfig.YCells;
			int32 Offset = 1;
			for (int32 CellID = 0; CellID < NumCells; CellID++)
			{
				const int32 DMXOffset = CellID * CellSize + 1;
				const FName GeometryName = *FString::Printf(TEXT("Layer_%i"), CellID + 1);

				const TSharedRef<FDMXGDTFGeometryReference> GeometryReference = MakeShared<FDMXGDTFGeometryReference>(RootGeometry);
				RootGeometry->GeometryReferenceArray.Add(GeometryReference);

				GeometryReference->Name = GeometryName;
				GeometryReference->Geometry = MatrixCellGeometryReferenceName;
				GeometryReference->Model = TEXT("Layers");

				const TSharedRef<FDMXGDTFGeometryBreak> Break = MakeShared<FDMXGDTFGeometryBreak>(GeometryReference);
				Break->DMXBreak = 1;
				Break->DMXOffset = Offset;
				GeometryReference->BreakArray.Add(Break);

				Break->DMXBreak = 1; // Unreal does not support multi universe patches, DMXBreak is always 1
				Break->DMXOffset = DMXOffset;

				for (const FDMXFixtureCellAttribute& UnrealMatrixAttribute : UnrealMode.FixtureMatrixConfig.CellAttributes)
				{
					UnrealCellAttributeToGeometryReferenceMap.Add(&UnrealMatrixAttribute, GeometryReference);
				}

				Offset += CellSize;
			}
		}

		// Always add common functions
		for (const FDMXFixtureFunction& UnrealFunction : UnrealMode.Functions)
		{
			// Common Unreal Functions are assigned to the root geometry for now so they do not need specific handling
			UnrealFunctionToGeometryMap.Add(&UnrealFunction, RootGeometry);
		}
	}

	void FDMXFixtureTypeToGTDFConverter::CreateDMXModes(const UDMXEntityFixtureType* UnrealFixtureType, const TSharedRef<FDMXGDTFFixtureType>& GDTFFixtureType)
	{
		for (const FDMXFixtureMode& UnrealMode : UnrealFixtureType->Modes)
		{
			const TSharedRef<FDMXGDTFGeometry>* RootGeometryPtr = UnrealModeToRootGeometryMap.Find(&UnrealMode);
			if (!ensureMsgf(RootGeometryPtr, TEXT("%hs: Unexpected cannot find root geometry for DMX Mode '%s'. Failed to convert mode to GDTF."), __FUNCTION__, *UnrealMode.ModeName))
			{
				continue;
			}

			// Create the mode
			const TSharedRef<FDMXGDTFDMXMode> DMXMode = MakeShared<FDMXGDTFDMXMode>(GDTFFixtureType);
			GDTFFixtureType->DMXModes.Add(DMXMode);

			DMXMode->Name = *UnrealMode.ModeName;
			DMXMode->Description = TEXT("Unreal Engine generated DMX Mode");
			DMXMode->Geometry = (*RootGeometryPtr)->Name;

			CreateDMXChannels(UnrealMode, DMXMode);
		}
	}

	void FDMXFixtureTypeToGTDFConverter::CreateDMXChannels(const FDMXFixtureMode& UnrealMode, const TSharedRef<FDMXGDTFDMXMode>& GDTFDMXMode)
	{			
		// Create DMX Channels for non-matrix Unreal Functions
		for (const FDMXFixtureFunction& UnrealFunction : UnrealMode.Functions)
		{
			const TSharedRef<FDMXGDTFGeometry>* DMXChannelGeometryPtr = UnrealFunctionToGeometryMap.Find(&UnrealFunction);
			if (!ensureMsgf(DMXChannelGeometryPtr, TEXT("%hs: Unexpected cannot find geometry for DMX Function '%s'. Failed to convert mode to GDTF."), __FUNCTION__, *UnrealFunction.FunctionName))
			{
				continue;
			}

			const TSharedRef<FDMXGDTFDMXChannel> DMXChannel = MakeShared<FDMXGDTFDMXChannel>(GDTFDMXMode);
			GDTFDMXMode->DMXChannels.Add(DMXChannel);

			const FString GDTFAttribute = FDMXUnrealToGDTFAttributeConversion::ConvertUnrealToGDTFAttribute(UnrealFunction.Attribute.Name).ToString();
			const FString ChannelFunctionName = UnrealFunction.FunctionName;

			// The initial function has to be written in following format "GeometryName_LogicalChannelAttribute.ChannelFunctionAttribute.ChannelFunctionName"
			DMXChannel->InitialFunction = FString::Printf(TEXT("%s_%s.%s.%s"), *(*DMXChannelGeometryPtr)->Name.ToString(), *GDTFAttribute, *GDTFAttribute, *ChannelFunctionName);
			DMXChannel->Geometry = (*DMXChannelGeometryPtr)->Name;
			DMXChannel->Offset = [UnrealFunction]()
				{			
					const int32 Offset = UnrealFunction.Channel;
					const uint8 Size = UnrealFunction.GetNumChannels();

					TArray<uint32> Offsets;
					for (int32 ByteOffset = Offset; ByteOffset < Offset + Size; ByteOffset++)
					{
						Offsets.Add(ByteOffset);
					}
					const bool bUseLSBMode = UnrealFunction.bUseLSBMode;
					Algo::Sort(Offsets, [bUseLSBMode](uint32 OffsetA, uint32 OffsetB)
						{
							return bUseLSBMode ? OffsetA >= OffsetB : OffsetA <= OffsetB;
						});

					return Offsets;
				}();

			CreateLogicalChannel(UnrealFunction, DMXChannel, GDTFAttribute);
		}

		// Create DMX Channels for Unreal Matrix Cells if this is a matrix mode
		if (UnrealMode.bFixtureMatrixEnabled)
		{
			const int32 MatrixStartingChannel = UnrealMode.FixtureMatrixConfig.FirstCellChannel;

			int32 Offset = UnrealMode.FixtureMatrixConfig.FirstCellChannel;
			for (const FDMXFixtureCellAttribute& UnrealCellAttribute : UnrealMode.FixtureMatrixConfig.CellAttributes)
			{
				const TSharedRef<FDMXGDTFDMXChannel> DMXChannel = MakeShared<FDMXGDTFDMXChannel>(GDTFDMXMode);
				GDTFDMXMode->DMXChannels.Add(DMXChannel);

				const FString GDTFAttribute = FDMXUnrealToGDTFAttributeConversion::ConvertUnrealToGDTFAttribute(UnrealCellAttribute.Attribute.Name).ToString();

				// For a matrix with geometry references, the initial function has to be written in following format "GeometryName_LogicalChannelAttribute.ChannelFunctionAttribute.ChannelFunctionName"
				DMXChannel->InitialFunction = FString::Printf(TEXT("%s_%s.%s.%s"), *MatrixCellGeometryReferenceName.ToString(), *GDTFAttribute, *GDTFAttribute, *UnrealCellAttribute.Attribute.Name.ToString());
				DMXChannel->Geometry = MatrixCellGeometryReferenceName;
				DMXChannel->Offset = [&Offset, UnrealCellAttribute]()
					{					
						const uint8 Size = UnrealCellAttribute.GetNumChannels();

						TArray<uint32> Offsets;
						for (int32 ByteOffset = Offset; ByteOffset < Offset + Size; ByteOffset++)
						{
							Offsets.Add(ByteOffset);
						}
						const bool bUseLSBMode = UnrealCellAttribute.bUseLSBMode;
						Algo::Sort(Offsets, [bUseLSBMode](uint32 OffsetA, uint32 OffsetB)
							{
								return bUseLSBMode ? OffsetA >= OffsetB : OffsetA <= OffsetB;
							});

						Offset += Size;

						return Offsets;
					}();

				// Using a negative value to express special value "Overwrite"
				DMXChannel->DMXBreak = -1;

				CreateLogicalChannel(UnrealCellAttribute, DMXChannel, GDTFAttribute);
			}
		}

		// Sort by Offset
		Algo::SortBy(GDTFDMXMode->DMXChannels, [](const TSharedPtr<FDMXGDTFDMXChannel>& DMXChannel)
			{
				const uint32* MinElementPtr = Algo::MinElementBy(DMXChannel->Offset, [](int32 Element)
					{
						return Element;
					});

				return MinElementPtr ? *MinElementPtr : 0;
			});
	}

	void FDMXFixtureTypeToGTDFConverter::CreateLogicalChannel(const FDMXFixtureFunction& UnrealFunction, const TSharedRef<FDMXGDTFDMXChannel>& GDTFDMXChannel, const FString& GDTFAttribute)
	{
		const TSharedRef<FDMXGDTFLogicalChannel> LogicalChannel = MakeShared<FDMXGDTFLogicalChannel>(GDTFDMXChannel);
		GDTFDMXChannel->LogicalChannelArray.Add(LogicalChannel);

		LogicalChannel->Attribute = *GDTFAttribute;
	
		CrateChannelFunction(UnrealFunction, LogicalChannel, GDTFAttribute);
	}

	void FDMXFixtureTypeToGTDFConverter::CreateLogicalChannel(const FDMXFixtureCellAttribute& UnrealCellAttribute, const TSharedRef<FDMXGDTFDMXChannel>& GDTFDMXChannel, const FString& GDTFAttribute)
	{
		const TSharedRef<FDMXGDTFLogicalChannel> LogicalChannel = MakeShared<FDMXGDTFLogicalChannel>(GDTFDMXChannel);
		GDTFDMXChannel->LogicalChannelArray.Add(LogicalChannel);

		LogicalChannel->Attribute = *GDTFAttribute;

		CrateChannelFunction(UnrealCellAttribute, LogicalChannel, GDTFAttribute);
	}

	void FDMXFixtureTypeToGTDFConverter::CrateChannelFunction(const FDMXFixtureFunction& UnrealFunction, const TSharedRef<FDMXGDTFLogicalChannel>& GDTFLogicalChannel, const FString& GDTFAttribute)
	{
		const TSharedRef<FDMXGDTFChannelFunction> ChannelFunction = MakeShared<FDMXGDTFChannelFunction>(GDTFLogicalChannel);
		GDTFLogicalChannel->ChannelFunctionArray.Add(ChannelFunction);

		ChannelFunction->Name = *UnrealFunction.FunctionName;
		ChannelFunction->Attribute = GDTFAttribute;

		const FDMXGDTFDMXValue Default = UnrealFunction.DefaultValue;
		ChannelFunction->Default = Default;
		ChannelFunction->DMXFrom = 0;
	}

	void FDMXFixtureTypeToGTDFConverter::CrateChannelFunction(const FDMXFixtureCellAttribute& UnrealCellAttribute, const TSharedRef<FDMXGDTFLogicalChannel>& GDTFLogicalChannel, const FString& GDTFAttribute)
	{
		const TSharedRef<FDMXGDTFChannelFunction> ChannelFunction = MakeShared<FDMXGDTFChannelFunction>(GDTFLogicalChannel);
		GDTFLogicalChannel->ChannelFunctionArray.Add(ChannelFunction);

		ChannelFunction->Name = *(UnrealCellAttribute.Attribute.Name.ToString());
		ChannelFunction->Attribute = GDTFAttribute;
		ChannelFunction->Default = 0;
		ChannelFunction->DMXFrom = 0;
	}
}
