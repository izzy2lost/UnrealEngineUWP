// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanProjectUtilities.h"

#include "MetaHumanVersionService.h"
#include "MetaHumanImport.h"
#include "MetaHumanTypes.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, MetaHumanSDKEditor)

FMetaHumanVersion::FMetaHumanVersion(const FString& VersionString)
{
	TArray<FString> ParsedVersionString;
	const int32 NumSections = VersionString.ParseIntoArray(ParsedVersionString, TEXT("."));
	verify(NumSections == 3);
	if (NumSections == 3)
	{
		Major = FCString::Atoi(*ParsedVersionString[0]);
		Minor = FCString::Atoi(*ParsedVersionString[1]);
		Revision = FCString::Atoi(*ParsedVersionString[2]);
	}
}

FMetaHumanVersion FInstalledMetaHuman::GetVersion() const
{
	const FString VersionFilePath = FPaths::Combine(MetaHumansFilePath, Name, TEXT("VersionInfo.txt"));
	return FMetaHumanVersion::ReadFromFile(VersionFilePath);
}

// External APIs
void METAHUMANSDKEDITOR_API FMetaHumanProjectUtilities::EnableAutomation(IMetaHumanProjectUtilitiesAutomationHandler* Handler)
{
	FMetaHumanImport::Get()->SetAutomationHandler(Handler);
}

void METAHUMANSDKEDITOR_API FMetaHumanProjectUtilities::SetBulkImportHandler(IMetaHumanBulkImportHandler* Handler)
{
	FMetaHumanImport::Get()->SetBulkImportHandler(Handler);
}

void METAHUMANSDKEDITOR_API FMetaHumanProjectUtilities::ImportAsset(const FMetaHumanAssetImportDescription& AssetImportDescription)
{
	FMetaHumanImport::Get()->ImportAsset(AssetImportDescription);
}

void METAHUMANSDKEDITOR_API FMetaHumanProjectUtilities::OverrideVersionServiceUrl(const FString& BaseUrl)
{
	UE::MetaHumanVersionService::SetServiceUrl(BaseUrl);
}

TArray<FInstalledMetaHuman> METAHUMANSDKEDITOR_API FMetaHumanProjectUtilities::GetInstalledMetaHumans()
{
	return FInstalledMetaHuman::GetInstalledMetaHumans(FImportPaths{ FMetaHumanAssetImportDescription{} });
}
