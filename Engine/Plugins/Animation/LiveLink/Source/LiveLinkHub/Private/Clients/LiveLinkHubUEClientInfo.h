// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMessageContext.h"
#include "LiveLinkHubMessages.h"

#include "LiveLinkHubUEClientInfo.generated.h"

/**
 * Identifier for a UE client receiving data from the hub.
 */
USTRUCT()
struct FLiveLinkHubClientId
{
	GENERATED_BODY()

	/** Default constructor, should only be used by the reflection code. */
	FLiveLinkHubClientId() = default;

	explicit FLiveLinkHubClientId(FMessageAddress InAddress)
		: Address(MoveTemp(InAddress))
		, Guid(FGuid::NewGuid())
	{
	}

	/** Update the address contained in this ID. */
	void UpdateAddress(FMessageAddress InAddress)
	{
		Address = InAddress;
	}

	/** Invalidate the address contained in this ID. */
	void InvalidateAddress()
	{
		Address.Invalidate();
	}

	/** Get the message bus address of this client. */
	FMessageAddress GetAddress() const
	{
		return Address;
	}

	/** Get the hash of this ID. */
	friend uint32 GetTypeHash(FLiveLinkHubClientId Id)
	{
		if (Id.Address.IsValid())
		{
			return GetTypeHash(Id.Address);
		}

		return GetTypeHash(Id.Guid);
	}

	bool operator==(const FLiveLinkHubClientId& Other) const
	{
		if (Guid == Other.Guid)
		{
			return true;
		}

		if (Address.IsValid() && Other.Address.IsValid())
		{
			return Address == Other.Address;
		}

		// Make sure we return false so that 2 clients with invalid addresses are considered different.
		return false;
	}

	bool operator==(const FMessageAddress& Other) const
	{
		return Address == Other;
	}

private:
	/** MessageBus address for this client. Invalid when the client is disconnected. */
	FMessageAddress Address;

	/** Unique identifier for this client. */
	UPROPERTY()
	FGuid Guid;
};

/** Wrapper around FLiveLinkClientInfoMessage that adds additional info. Used mainly to display information about a client in the UI. */
USTRUCT()
struct FLiveLinkHubUEClientInfo
{
	GENERATED_BODY();

	FLiveLinkHubUEClientInfo() = default;

	explicit FLiveLinkHubUEClientInfo(const FLiveLinkClientInfoMessage& InClientInfo, FMessageAddress Address)
		: Id(FLiveLinkHubClientId(MoveTemp(Address)))
		, LongName(InClientInfo.LongName)
		, Status(InClientInfo.Status)
		, IPAddress(TEXT("192.168.0.1 (Placeholder)"))
		, Hostname(InClientInfo.Hostname)
		, ProjectName(InClientInfo.ProjectName)
		, CurrentLevel(InClientInfo.CurrentLevel)
	{
	}

	void UpdateFromClient(const FLiveLinkClientInfoMessage& InClientInfo, FMessageAddress Address)
    {
		// Preserve old id but update its connection address.
		FLiveLinkHubClientId PreviousId = Id;
		PreviousId.UpdateAddress(Address);

		FLiveLinkHubUEClientInfo NewInfo{InClientInfo, Address};
		NewInfo.Id = PreviousId;

		*this = MoveTemp(NewInfo);
    }

	/** Identifier for this client. */
	UPROPERTY()
	FLiveLinkHubClientId Id;

	/** Full name used to identify this client. (ie.UEFN_sessionID_LDN_WSYS_9999) */
	UPROPERTY(VisibleAnywhere, Category = "LiveLink Client")
   	FString LongName;
	
	/** Status of the client, ie. is it actively doing a take record at the moment? */
	UPROPERTY(transient)
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
