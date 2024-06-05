// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HttpRequestHandler.h"
#include "Serialization/JsonWriter.h"
#include "ExternalRpcRegistry.h"


#include "ExternalRpcRegistrationComponent.generated.h"
class FJsonObject;
enum class EHttpServerRequestVerbs : uint16;
struct FExternalRouteInfo;
struct FExternalRpcArgumentDesc;
struct FHttpPath;
struct FHttpServerRequest;
struct FHttpServerResponse;
struct FKey;
UCLASS()
class EXTERNALRPCREGISTRY_API UExternalRpcRegistrationComponent : public UObject
{

	GENERATED_BODY()
public:
	TArray<FName> RegisteredRoutes;
	FString SecuritySecret;


	// Outgoing RPC info
	FString SenderID;
	FString ListenerAddress;

	virtual void DeregisterHttpCallbacks();	
	virtual void RegisterAlwaysOnHttpCallbacks();
	void BroadcastRpcListChanged();
	void RegisterHttpCallback(FExternalRouteInfo InRouteInfo, const FHttpRequestHandler& Handler, bool bOverrideIfBound = false);
	void RegisterHttpCallback(FName RouteName, const FHttpPath& HttpPath, const EHttpServerRequestVerbs& RequestVerbs, const FHttpRequestHandler& Handler, bool bOverrideIfBound = false, FString OptionalCategory = TEXT(""), FString OptionalContentType = {}, TArray<FExternalRpcArgumentDesc> OptionalInArguments = TArray<FExternalRpcArgumentDesc>());


	// Send Message to Listener RPC
	bool HttpSendMessageToListener(FString MessageCategory, FString MessagePayload);
	bool HttpUpdateIpOnListener(FString TargetName, FString MessagePayload);
	FHttpRequestHandler CreateRouteHandle(TDelegate<bool(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)>	 InFunc);

	/** Creates a simple Http response with boolean and optional value */
	TUniquePtr<FHttpServerResponse> CreateSimpleResponse(bool bInWasSuccessful, FString InValue = "");

};