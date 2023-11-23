// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Containers/ObservableArray.h"

template <typename OptionalType> struct TOptional;
struct FLiveLinkClientInfoMessage;
struct FMessageAddress;
struct FLiveLinkHubUEClientInfo;
struct FLiveLinkSubjectKey;


/** Decouples the UI from the livelink hub functions. */
class ILiveLinkHubClientsModel
{
public:
	/** Types of client updates. */
	enum class EClientEventType
	{
		Connected,
		Disconnected,
		Modified
	};

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnClientEvent, FMessageAddress, EClientEventType);

	virtual ~ILiveLinkHubClientsModel() = default;

	/** Delegate used to get noticed about changes to the client list. */
	virtual FOnClientEvent& OnClientEvent() = 0;

	/** Get the status text of a client. */
	virtual FText GetClientStatus(FMessageAddress Client) const = 0;

	/** Get the list of clients connected to the hub. */
	virtual TArray<FMessageAddress> GetClients() const = 0;

	/** Get information about a given client given its address. */
	virtual TOptional<FLiveLinkHubUEClientInfo> GetClientInfo(FMessageAddress InAddress) const = 0;

	/** Get whether a client should receive livelink data. */
	virtual bool IsClientEnabled(FMessageAddress Client) const = 0;

	/** Set whether a client should receive livelink data. */
	virtual void SetClientEnabled(FMessageAddress Client, bool bInEnable) = 0;

	/** Get whether a subject is enabled on a given client. */
	virtual bool IsSubjectEnabled(FMessageAddress Client, const FLiveLinkSubjectKey& Subject) const = 0;

	/** Set whether a subject should receive livelink data. */
	virtual void SetSubjectEnabled(FMessageAddress Client, const FLiveLinkSubjectKey& Subject, bool bInEnable) = 0;
};
