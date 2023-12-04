// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubFileUtilities.h"

#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "LiveLinkHubConfigData.h"
#include "LiveLinkHubLog.h"

void UE::LiveLinkHub::FileUtilities::Private::SaveConfig(const FLiveLinkHubConfigData& InConfigData, const FString& InFilePath)
{
	if (ensure(!InFilePath.IsEmpty()))
	{
		const TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileWriter(*InFilePath));
		if (Ar)
		{
			const TSharedPtr<FJsonObject> JsonObject = ToJson(InConfigData);
			
			const TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(Ar.Get(), 0);
			FJsonSerializer::Serialize(JsonObject.ToSharedRef(), JsonWriter);

			ensure(Ar->Close());
		}
	}
}

TSharedPtr<FLiveLinkHubConfigData> UE::LiveLinkHub::FileUtilities::Private::LoadConfig(const FString& InFilePath)
{
	if (IFileManager::Get().FileExists(*InFilePath))
	{
		const TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileReader(*InFilePath));
		if (Ar)
		{
			const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Ar.Get());

			TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
			if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
			{
				int32 Version = 0;
				if (JsonObject->TryGetNumberField(JsonVersionKey, Version) &&
					Version <= LiveLinkHubVersion)
				{
					return FromJson(JsonObject);
				}

				UE_LOG(LogLiveLinkHub, Error, TEXT("Could not load config %s because the version field %s was missing or invalid."),
					*InFilePath, *JsonVersionKey);
			}
		}
	}

	return nullptr;
}

TSharedPtr<FJsonObject> UE::LiveLinkHub::FileUtilities::Private::ToJson(const FLiveLinkHubConfigData& InConfigData)
{
	TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonValueNumber> NumberValue = MakeShared<FJsonValueNumber>(LiveLinkHubVersion);
	JsonObject->SetField(JsonVersionKey, NumberValue);
	
	FJsonObjectConverter::UStructToJsonObject(FLiveLinkHubConfigData::StaticStruct(), &InConfigData, JsonObject);
	
	return JsonObject;
}

TSharedPtr<FLiveLinkHubConfigData> UE::LiveLinkHub::FileUtilities::Private::FromJson(const TSharedPtr<FJsonObject>& InJsonObject)
{
	if (InJsonObject.IsValid())
	{
		FLiveLinkHubConfigData OutConfigData;
		const bool bResult =
			FJsonObjectConverter::JsonObjectToUStruct(InJsonObject.ToSharedRef(),
				FLiveLinkHubConfigData::StaticStruct(), &OutConfigData);

		if (!bResult)
		{
			UE_LOG(LogLiveLinkHub, Error, TEXT("Could not convert from json to LiveLinkHubConfigData."))
		}
		return MakeShared<FLiveLinkHubConfigData>(OutConfigData);
	}

	return nullptr;
}
