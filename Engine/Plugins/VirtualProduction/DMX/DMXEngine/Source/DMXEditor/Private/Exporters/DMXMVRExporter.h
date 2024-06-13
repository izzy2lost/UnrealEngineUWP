// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
#include "Containers/Array.h"
#include "Templates/SharedPointer.h"

class FDMXZipper;
class FXmlFile;
class UDMXEntityFixtureType;
class UDMXGDTFAssetImportData;
class UDMXLibrary;
class UDMXMVRAssetImportData;
class UDMXMVRGeneralSceneDescription;

namespace UE::DMX
{
	/** Helper class to export a DMX Library as MVR file */
	class FDMXMVRExporter
	{
	public:
		/** Exports the DMX Library as MVR File */
		static void Export(UDMXLibrary* DMXLibrary, const FString& FilePathAndName, FText& OutErrorReason);

	private:
		/** Exports the DMX Library as MVR File. If OutErrorReason is not empty there were issues with the export. */
		void ExportInternal(UDMXLibrary* DMXLibrary, const FString& FilePathAndName, FText& OutErrorReason);

		/** Zips the GeneralSceneDescription.xml */
		[[nodiscard]] bool ZipGeneralSceneDescription(const TSharedRef<FDMXZipper>& Zip, const UDMXMVRGeneralSceneDescription* GeneralSceneDescription, FText& OutErrorReason);

		/** Zips the GDTFs from the Library */
		[[nodiscard]] bool ZipGDTFs(const TSharedRef<FDMXZipper>& Zip, UDMXLibrary* DMXLibrary);

		/** Zips 3rd Party Data from the MVR Asset Import Data */
		void ZipThirdPartyData(const TSharedRef<FDMXZipper>& Zip, const UDMXMVRGeneralSceneDescription* GeneralSceneDescription);

		/** Creates an General Scene Description Xml File from the MVR Source, as it was imported */
		const TSharedPtr<FXmlFile> CreateSourceGeneralSceneDescriptionXmlFile(const UDMXMVRGeneralSceneDescription* GeneralSceneDescription) const;

		/** Writes transforms from level to the MVR Fixture Actors where applicable */
		void WriteMVRFixtureTransformsFromLevel(UDMXMVRGeneralSceneDescription* GeneralSceneDescription);

		/**
		 * Gets raw source data or creates (possibly empty) source data where the source data is not present.
		 * Prior 5.1 there was no source data stored. Offers a dialog to load missing data
		 */
		const TArray64<uint8>& RefreshSourceDataAndFixtureType(UDMXEntityFixtureType& FixtureType, UDMXGDTFAssetImportData& InOutGDTFAssetImportData) const;
	};
}
