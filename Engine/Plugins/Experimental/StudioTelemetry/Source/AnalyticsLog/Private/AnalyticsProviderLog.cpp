// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnalyticsProviderLog.h"
#include "Analytics.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

FAnalyticsProviderLog::FAnalyticsProviderLog(const FAnalyticsProviderConfigurationDelegate& GetConfigValue)
{
	FString FileName = GetConfigValue.Execute(TEXT("FileName"), true);

	if (FileName.IsEmpty())
	{
		// Use default filename
		FileName = TEXT("Telemetry.json");
	}

	FString PathName = GetConfigValue.Execute(TEXT("PathName"), true);

	if (PathName.IsEmpty())
	{
		// Use default output path
		PathName = FPaths::ProjectSavedDir() / TEXT("Telemetry");
	}

	// Create the full output path
	FString FilePath = PathName / FileName;
	FileWriter = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*FilePath, FILEWRITE_EvenIfReadOnly));
}

FAnalyticsProviderLog::~FAnalyticsProviderLog()
{
}

bool FAnalyticsProviderLog::SetSessionID(const FString& InSessionID)
{
	SessionID = InSessionID;
	return true;
}

FString FAnalyticsProviderLog::GetSessionID() const
{
	return SessionID;
}

void FAnalyticsProviderLog::SetUserID(const FString& InUserID)
{
	UserID = InUserID;

}

FString FAnalyticsProviderLog::GetUserID() const
{
	return UserID;
}

void FAnalyticsProviderLog::FlushEvents()
{
}

void FAnalyticsProviderLog::SetDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes)
{
	DefaultEventAttributes = Attributes;
}

TArray<FAnalyticsEventAttribute> FAnalyticsProviderLog::GetDefaultEventAttributesSafe() const
{
	return DefaultEventAttributes;
}

int32 FAnalyticsProviderLog::GetDefaultEventAttributeCount() const
{
	return DefaultEventAttributes.Num();
}

FAnalyticsEventAttribute FAnalyticsProviderLog::GetDefaultEventAttribute(int AttributeIndex) const
{
	return DefaultEventAttributes[AttributeIndex];
}

bool FAnalyticsProviderLog::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
	RecordEvent(TEXT("StartSession"), Attributes);

	return true;
}

void FAnalyticsProviderLog::EndSession()
{
	RecordEvent(TEXT("EndSession"));

	if (FileWriter)
	{
		FileWriter->Flush();
		FileWriter->Close();
	}
}

void FAnalyticsProviderLog::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{	
	if (FileWriter)
	{
		TStringBuilder<1024> Builder;

		// Log event as Newline - delimited JSON
		Builder.Appendf(TEXT("{\"EventName\":\"%s\""), *EventName);

		// Add the event timestamp field
		Builder.Appendf(TEXT(",\"TimestampUTC\":\"%f\""), FDateTime::UtcNow().ToUnixTimestampDecimal());

		// Log the default attributes
		for (const FAnalyticsEventAttribute& Attribute : DefaultEventAttributes)
		{
			Builder.Appendf(TEXT(",\"%s\":\"%s\""), *Attribute.GetName(), *Attribute.GetValue());
		}

		// Log the event attributes
		for ( const FAnalyticsEventAttribute& Attribute : Attributes )
		{
			Builder.Appendf(TEXT(",\"%s\":\"%s\""), *Attribute.GetName(), *Attribute.GetValue());
		}

		FileWriter->Logf(TEXT("%s}"),Builder.ToString());
		FileWriter->Flush();
	}
}