// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundSettings.h"

#if WITH_EDITORONLY_DATA
FMetaSoundQualitySettings::FMetaSoundQualitySettings()
	: UniqueId(FGuid::NewGuid())
	, Name(UMetaSoundQuality::GenerateNewName())
{
}
#endif //WITH_EDITORONLY_DATA

FName UMetaSoundQuality::GenerateNewName()
{
	const TArray<FName> Names = GetQualityList();
	const TCHAR* BaseName = TEXT("New Quality");
	FString NewName = BaseName;
	
	for (int32 Postfix=0; Names.Contains(*NewName); ++Postfix)
	{
		NewName = FString::Format(TEXT("{0} {1}"), { BaseName, Postfix });
	}
	return FName(*NewName);
}

TArray<FName> UMetaSoundQuality::GetQualityList()
{
	TArray<FName> Names;
	if (const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>())
	{
		Algo::Transform(Settings->QualitySettings, Names, [](const FMetaSoundQualitySettings& Quality) -> FName
		{
			return Quality.Name;
		});
	}
	return Names;
}
