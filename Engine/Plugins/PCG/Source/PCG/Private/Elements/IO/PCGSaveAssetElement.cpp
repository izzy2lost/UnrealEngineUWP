// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/IO/PCGSaveAssetElement.h"

#include "PCGAssetExporterUtils.h"
#include "PCGModule.h"

#include "AssetRegistry/AssetData.h"

bool UPCGDataCollectionExporter::ExportAsset(const FString& PackageName, UPCGDataAsset* Asset)
{
	// Relies on default behavior to duplicate if needed
	Asset->Data = Data;
	return true;
}

UPackage* UPCGDataCollectionExporter::UpdateAsset(const FAssetData& PCGAsset)
{
	return nullptr;
}

UPCGSaveDataAssetSettings::UPCGSaveDataAssetSettings()
{
	Pins = Super::InputPinProperties();
}

FPCGElementPtr UPCGSaveDataAssetSettings::CreateElement() const
{
	return MakeShared<FPCGSaveDataAssetElement>();
}

bool FPCGSaveDataAssetElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSaveDataAssetElement::Execute);
	check(Context);
	const UPCGSaveDataAssetSettings* Settings = Context->GetInputSettings<UPCGSaveDataAssetSettings>();
	check(Settings);

	UPCGDataCollectionExporter* Exporter = nullptr;

	if (Settings->CustomDataCollectionExporterClass)
	{
		Exporter = NewObject<UPCGDataCollectionExporter>(GetTransientPackage(), Settings->CustomDataCollectionExporterClass);
	}
	
	if (!Exporter)
	{
		Exporter = NewObject<UPCGDataCollectionExporter>();
	}

	check(Exporter);
	Exporter->Data = Context->InputData;

	UPCGAssetExporterUtils::CreateAsset(Exporter, Settings->Params);

	return true;
}