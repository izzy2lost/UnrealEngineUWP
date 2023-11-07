// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FString;
class IConcertClient;
struct FGuid;

namespace UE::MultiUserClient::ClientUtils
{
	/**
	 * Gets the display name for a client. Appends (me) if the client is local.
	 * 
	 * @param InLocalClientInstance Used to look up client display info
	 * @param InClientToGetName The endpoint ID of the client whose name to get
	 * @return The display name or empty
	 */
	FString GetClientDisplayName(const IConcertClient& InLocalClientInstance, const FGuid& InClientToGetName);
}
