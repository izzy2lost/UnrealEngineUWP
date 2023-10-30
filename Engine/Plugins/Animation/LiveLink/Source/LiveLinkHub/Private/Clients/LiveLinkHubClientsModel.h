// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Containers/ObservableArray.h"

template <typename OptionalType> struct TOptional;
struct FLiveLinkClientInfoMessage;
struct FMessageAddress;
struct FLiveLinkHubUEClientInfo;


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

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnClientEvent, const FMessageAddress&, EClientEventType);

	virtual ~ILiveLinkHubClientsModel() {}

	/** Delegate used to get noticed about changes to the client list. */
	virtual FOnClientEvent& OnClientEvent() = 0;

	/** Get the list of clients connected to the hub. */
	virtual TArray<FMessageAddress> GetClients() const = 0;

	/** Get information about a given client given its address. */
	virtual TOptional<FLiveLinkHubUEClientInfo> GetClientInfo(const FMessageAddress& InAddress) const = 0;
};
