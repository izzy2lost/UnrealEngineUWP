// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkMessages.h"
#include "LiveLinkTypes.h"
#include "LiveLinkHubMessages.generated.h"

/** Annotation put on MessageBus messages to indicate the type of provider used. 
 * Absence of provider type means that the message comes from a regular LiveLinkProvider.
 */
struct LIVELINKHUBMESSAGING_API FLiveLinkHubMessageAnnotation
{
	static FName ProviderTypeAnnotation;
};

namespace UE::LiveLinkHub::Private
{
	/** LiveLink Hub provider type used to identify messages coming from a LiveLinkProvider that lives on a LiveLink Hub. */
	extern const LIVELINKHUBMESSAGING_API FName LiveLinkHubProviderType;
}

/** Status of a UE client connected to a live link hub. */
UENUM()
enum class ELiveLinkClientStatus
{
	Connected, /** Default state of a UE client. */
	Disconnected, /** Client is not connected to the hub. */
	Recording  /** UE is currently doing a take record. */
};

/** Information related to an unreal client that is connecting to a livelink hub instance. */
USTRUCT()
struct LIVELINKHUBMESSAGING_API FLiveLinkClientInfoMessage
{
	GENERATED_BODY()

	/** Full name used to identify this client. (ie.UEFN_sessionID_LDN_WSYS_9999) */
	UPROPERTY()
	FString LongName;

	/** Status of the client, ie. is it actively doing a take record at the moment? */
	UPROPERTY()
	ELiveLinkClientStatus Status = ELiveLinkClientStatus::Disconnected;

	/** Name of the host of the UE client */
	UPROPERTY()
	FString Hostname;

	/** Name of the current project. */
	UPROPERTY()
	FString ProjectName;

	/** Name of the current level opened. */
	UPROPERTY()
	FString CurrentLevel;

	/** LiveLink Version in use by this client. */
	UPROPERTY()
	int32 LiveLinkVersion = 1;
};

/** Special connection message used when connecting to a livelink hub that contains information about this client. */
USTRUCT()
struct LIVELINKHUBMESSAGING_API FLiveLinkHubConnectMessage
{
	GENERATED_BODY()

	/** Client information to forward to the hub */
	UPROPERTY()
	FLiveLinkClientInfoMessage ClientInfo;
};
