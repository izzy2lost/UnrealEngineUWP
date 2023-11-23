// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMessageContext.h"
#include "LiveLinkHubMessages.h"

#include "LiveLinkHubUEClientInfo.generated.h"

/** Wrapper around FLiveLinkClientInfoMessage that adds additional info. Used mainly to display information about a client in the UI. */
USTRUCT()
struct FLiveLinkHubUEClientInfo
{
	GENERATED_BODY();

	FLiveLinkHubUEClientInfo() = default;

	explicit FLiveLinkHubUEClientInfo(const FLiveLinkClientInfoMessage& InClientInfo, FMessageAddress InMessageAddress)
		: MessageAddress(MoveTemp(InMessageAddress))
		, LongName(InClientInfo.LongName)
		, Status(InClientInfo.Status)
		, IPAddress(TEXT("192.168.0.1 (Placeholder)"))
		, Hostname(InClientInfo.Hostname)
		, ProjectName(InClientInfo.ProjectName)
		, CurrentLevel(InClientInfo.CurrentLevel)
	{
	}

	/** MessageBus address associated with this client.  */
	FMessageAddress MessageAddress;

	/** Full name used to identify this client. (ie.UEFN_sessionID_LDN_WSYS_9999) */
	UPROPERTY(VisibleAnywhere, Category = "LiveLink Client")
   	FString LongName;
	
	/** Status of the client, ie. is it actively doing a take record at the moment? */
	UPROPERTY()
	ELiveLinkClientStatus Status = ELiveLinkClientStatus::Disconnected;
	
	UPROPERTY(VisibleAnywhere, Category = "LiveLink Client", DisplayName = "IP Address")
	FString IPAddress;
	
	/** Name of the host of the UE client */
	UPROPERTY(VisibleAnywhere, Category = "LiveLink Client")
	FString Hostname;

	/** Name of the current project. */
	UPROPERTY(VisibleAnywhere, Category = "LiveLink Client")
	FString ProjectName;
	
	/** Name of the current level opened. */
	UPROPERTY(VisibleAnywhere, Category = "LiveLink Client")
	FString CurrentLevel;
	
	/** Subjects that should not be transmitted to this client. */
	UPROPERTY()
	TSet<FName> DisabledSubjects;
	
	/** Whether this client should receive messages. */
	UPROPERTY()
	bool bEnabled = true;
};
