// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnalyticsProviderMulticast.h"
#include "StudioTelemetryLog.h"
#include "Analytics.h"
#include "AnalyticsProviderMulticast.h"
#include "AnalyticsProviderConfigurationDelegate.h"
#include "Misc/ConfigCacheIni.h"
#include "HttpModule.h"

const FString TelemetrySection(TEXT("StudioTelemetry"));
static FString ProviderSection;

FString GetAnalyticsProviderConfiguration(const FString& Name, bool)
{
	FString Result;
	GConfig->GetString(*ProviderSection, *Name, Result, GEngineIni);
	return Result;
}

TSharedPtr<FAnalyticsProviderMulticast> FAnalyticsProviderMulticast::CreateAnalyticsProvider()
{
	return MakeShared<FAnalyticsProviderMulticast>();
}

TWeakPtr<IAnalyticsProviderET> FAnalyticsProviderMulticast::GetAnalyticsProvider(const FString& Name)
{
	TSharedPtr<IAnalyticsProviderET>* ProviderPtr = Providers.Find(Name);
	return ProviderPtr != nullptr ? *ProviderPtr : TSharedPtr<IAnalyticsProviderET>();
}

FAnalyticsProviderMulticast::FAnalyticsProviderMulticast()
{
	TArray<FString> SectionNames;
	
	if (GConfig->GetSectionNames(GEngineIni, SectionNames))
	{
		for (const FString& SectionName : SectionNames)
		{
			if (SectionName.Find(TelemetrySection) != INDEX_NONE)
			{
				ProviderSection = SectionName;

				FString UsageType;

				// Validate the usage type is for this build type
				if (GConfig->GetString(*ProviderSection, TEXT("UsageType"), UsageType, GEngineIni))
				{
#if WITH_EDITOR
					// Must specify a Editor usage type for this type build
					if (UsageType.Find(TEXT("Editor")) == INDEX_NONE)
					{
						continue;
					}
#elif WITH_SERVER_CODE
					// Must specify a Server usage type for this type build
					if (UsageType.Find(TEXT("Server")) == INDEX_NONE)
					{
						continue;
					}
#else
					// Must specify a Client or Program usage type for this type build
					if (UsageType.Find(TEXT("Program")) == INDEX_NONE && UsageType.Find(TEXT("Client")) == INDEX_NONE)
					{
						continue;
					}
#endif
				}
				else
				{
					// Must always specify a usage type
					UE_LOG(LogStudioTelemetry, Error, TEXT("There must be a valid UsageType specified for analytics provider %s"), *ProviderSection);
					continue;
				}

				FString ProviderType;

				if (GConfig->GetString(*ProviderSection, TEXT("ProviderType"), ProviderType, GEngineIni))
				{
					TSharedPtr<IAnalyticsProviderET> Provider;

					FString Name = GetAnalyticsProviderConfiguration("Name", true);

					if ( Name.IsEmpty() )
					{ 
						UE_LOG(LogStudioTelemetry, Error, TEXT("There must be a valid Name specified for analytics provider %s."), *ProviderSection);
						continue;
					}
					else if (Providers.Find(Name) )
					{
						UE_LOG(LogStudioTelemetry, Warning, TEXT("An analytics provider with name %s already exists."), *Name);
						continue;
					}
					
					if (ProviderType.Equals(TEXT("FAnalyticsProviderET"), ESearchCase::IgnoreCase))
					{
						Provider = FAnalyticsET::Get().CreateAnalyticsProviderET(FAnalyticsProviderConfigurationDelegate::CreateStatic(&GetAnalyticsProviderConfiguration));
					}
					
					if (Provider.IsValid())
					{
						UE_LOG(LogStudioTelemetry, Display, TEXT("Created a %s analytics provider %s from configuration %s [%s]"), *ProviderType, *Name, *GEngineIni, *ProviderSection);

						Providers.Add(Name, Provider);
					}
				}
				else
				{
					UE_LOG(LogStudioTelemetry, Error, TEXT("There must be a valid ProviderType specified for analytics provider %s"), *ProviderSection);
				}
			}
		}
	}
}

void FAnalyticsProviderMulticast::SetAppID(FString&& InAppID)
{
	Config.APIKeyET = InAppID;

	for (TProviders::TConstIterator it(Providers); it; ++it)
	{
		(*it).Value->SetAppID(CopyTemp(InAppID));
	}
}

void FAnalyticsProviderMulticast::SetAppVersion(FString&& InAppVersion)
{
	Config.AppVersionET = InAppVersion;

	for (TProviders::TConstIterator it(Providers); it; ++it)
	{
		(*it).Value->SetAppVersion(CopyTemp(InAppVersion));
	}
}

bool FAnalyticsProviderMulticast::SetSessionID(const FString& InSessionID)
{
	SessionID = InSessionID;

	bool bResult = true;

	for (TProviders::TConstIterator it(Providers); it; ++it)
	{
		bResult &= (*it).Value->SetSessionID(InSessionID);
	}

	return bResult;
}

FString FAnalyticsProviderMulticast::GetSessionID() const
{
	return SessionID;
}

void FAnalyticsProviderMulticast::SetUserID(const FString& InUserID)
{
	UserID = InUserID;

	for (TProviders::TConstIterator it(Providers); it; ++it)
	{
		(*it).Value->SetUserID(InUserID);
	}
}

FString FAnalyticsProviderMulticast::GetUserID() const
{
	return UserID;
}

const FAnalyticsET::Config& FAnalyticsProviderMulticast::GetConfig() const
{
	return Config;
}

void FAnalyticsProviderMulticast::SetURLEndpoint(const FString& UrlEndpoint, const TArray<FString>& AltDomains)
{
	for (TProviders::TConstIterator it(Providers); it; ++it)
	{
		(*it).Value->SetURLEndpoint(UrlEndpoint, AltDomains);
	}
}

void FAnalyticsProviderMulticast::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	for (TProviders::TConstIterator it(Providers); it; ++it)
	{
		(*it).Value->SetHeader(HeaderName, HeaderValue);
	}
}

void FAnalyticsProviderMulticast::SetEventCallback(const OnEventRecorded& InCallback)
{
	OnEventRecordedCallback = InCallback;
}

void FAnalyticsProviderMulticast::FlushEvents()
{
	for (TProviders::TConstIterator it(Providers); it; ++it)
	{
		(*it).Value->FlushEvents();
	}
}

void FAnalyticsProviderMulticast::BlockUntilFlushed(float InTimeoutSec)
{
	for (TProviders::TConstIterator it(Providers); it; ++it)
	{
		(*it).Value->BlockUntilFlushed(InTimeoutSec);
	}
}

bool FAnalyticsProviderMulticast::StartSession(FString InSessionID, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	SetSessionID(InSessionID);

	bool bResult = true;

	for (TProviders::TConstIterator it(Providers); it; ++it)
	{
		bResult &= (*it).Value->StartSession(InSessionID, Attributes);
	}

	return bResult;
}

void FAnalyticsProviderMulticast::SetShouldRecordEventFunc(const ShouldRecordEventFunction& InShouldRecordEventFunc)
{
	ShouldRecordEventFunc = InShouldRecordEventFunc;
}

bool FAnalyticsProviderMulticast::ShouldRecordEvent(const FString& EventName) const
{
	return ShouldRecordEventFunc ? ShouldRecordEventFunc(*this, EventName) : true;
}

void FAnalyticsProviderMulticast::SetDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes)
{
	DefaultEventAttributes = Attributes;

	for (TProviders::TConstIterator it(Providers); it; ++it)
	{
		(*it).Value->SetDefaultEventAttributes(CopyTemp(DefaultEventAttributes));
	}
}

TArray<FAnalyticsEventAttribute> FAnalyticsProviderMulticast::GetDefaultEventAttributesSafe() const
{
	return DefaultEventAttributes;
}

int32 FAnalyticsProviderMulticast::GetDefaultEventAttributeCount() const
{
	return DefaultEventAttributes.Num();
}

FAnalyticsEventAttribute FAnalyticsProviderMulticast::GetDefaultEventAttribute(int AttributeIndex) const
{
	return DefaultEventAttributes[AttributeIndex];
}

bool FAnalyticsProviderMulticast::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
	bool bResult = true;

	for (TProviders::TConstIterator it(Providers);it;++it)
	{
		bResult &= (*it).Value->StartSession(Attributes);
	}
	return bResult;
}

void FAnalyticsProviderMulticast::EndSession()
{
	for (TProviders::TConstIterator it(Providers);it;++it)
	{
		TSharedPtr<IAnalyticsProviderET> Provider = (*it).Value;

		Provider->EndSession();
		Provider.Reset();
	}

	Providers.Reset();
}

void FAnalyticsProviderMulticast::RecordEvent(FString&& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (ShouldRecordEvent(EventName))
	{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		// Expose events that have duplicate aatibute names. This is is not handled by the analytics backends in any reliable way.
		for (int32 index0 = 0; index0 < Attributes.Num(); ++index0)
		{
			for (int32 index1 = index0 + 1; index1 < Attributes.Num(); ++index1)
			{
				checkf(Attributes[index0].GetName() != Attributes[index1].GetName(), TEXT("Duplicate Attributes Found For Event %s %s==%s"), *EventName, *Attributes[index0].GetName(), *Attributes[index1].GetName());
			}
		}
#endif

		for (TProviders::TConstIterator it(Providers); it; ++it)
		{
			(*it).Value->RecordEvent(EventName, Attributes);
		}

		// Notify any callbacks
		if (OnEventRecordedCallback)
		{
			OnEventRecordedCallback(EventName, Attributes, true);
		}
	}

}
