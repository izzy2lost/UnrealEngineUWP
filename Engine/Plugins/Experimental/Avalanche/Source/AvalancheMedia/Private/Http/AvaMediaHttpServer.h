// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMessageContext.h"
#include "IMessageAttachment.h"
#include "HttpResultCallback.h"
#include "HttpRouteHandle.h"
#include "HttpServerRequest.h"
#include "IHttpRouter.h"
#include "StructDeserializer.h"
#include "Backends/JsonStructDeserializerBackend.h"
#include "Serialization/MemoryReader.h"

#include "AvaMediaHttpServer.generated.h"

class FAvaMediaPlaybackServer;

namespace UE::Ava::Web::Private
{
	template <typename MessageType>
	[[nodiscard]] bool DeserializeRequest(const FHttpServerRequest& InRequest, const FHttpResultCallback* InCompleteCallback, MessageType& OutDeserializedRequest);
}

// Required by Messaging API which this wraps, so a dummy context is required
class FEmptyMessageContext
	: public IMessageContext
{
public:
	FEmptyMessageContext() = default;
	virtual ~FEmptyMessageContext() override = default;

public:
	//~ IMessageContext interface
	virtual const TMap<FName, FString>& GetAnnotations() const override { return EmptyMap; }
	virtual TSharedPtr<IMessageAttachment, ESPMode::ThreadSafe> GetAttachment() const override { return nullptr; }
	virtual const FDateTime& GetExpiration() const override { return EmptyDate; }
	virtual const void* GetMessage() const override { return nullptr; }
	virtual const TWeakObjectPtr<UScriptStruct>& GetMessageTypeInfo() const override { return EmptyTypeInfo; }
	virtual TSharedPtr<IMessageContext, ESPMode::ThreadSafe> GetOriginalContext() const override { return nullptr; }
	virtual const TArray<FMessageAddress>& GetRecipients() const override { return EmptyRecipients; }
	virtual EMessageScope GetScope() const override { return {}; }
	virtual EMessageFlags GetFlags() const override { return {}; }
	virtual const FMessageAddress& GetSender() const override { return EmptyAddress; }
	virtual const FMessageAddress& GetForwarder() const override { return EmptyAddress; }
	virtual ENamedThreads::Type GetSenderThread() const override  { return {}; }
	virtual const FDateTime& GetTimeForwarded() const override { return EmptyDate; }
	virtual const FDateTime& GetTimeSent() const override { return EmptyDate; }

private:
	TMap<FName, FString> EmptyMap;
	FDateTime EmptyDate;
	TWeakObjectPtr<UScriptStruct> EmptyTypeInfo;
	TArray<FMessageAddress> EmptyRecipients;
	FMessageAddress EmptyAddress;
};

USTRUCT()
struct FAvaMediaHttpRouteInfo
{
	GENERATED_BODY()
public:
	FName RouteName;
	FHttpPath RoutePath;
	EHttpServerRequestVerbs RequestVerbs;
	FString InputContentType;
	FString InputExpectedFormat;
	FAvaMediaHttpRouteInfo()
	{
		RouteName = FName(TEXT(""));
		RoutePath = FHttpPath();
		RequestVerbs = EHttpServerRequestVerbs::VERB_NONE;
		InputContentType = TEXT("");
		InputExpectedFormat = TEXT("");
	}
	FAvaMediaHttpRouteInfo(FName InRouteName, FHttpPath InRoutePath, EHttpServerRequestVerbs InRequestVerbs, FString InContentType = TEXT(""), FString InExpectedFormat = TEXT(""))
	{
		RouteName = InRouteName;
		RoutePath = InRoutePath;
		RequestVerbs = InRequestVerbs;
		InputContentType = InContentType;
		InputExpectedFormat = InExpectedFormat;
	}
};

USTRUCT()
struct FAvaMediaHttpRouteDesc
{
	GENERATED_BODY()
public:
	FHttpRouteHandle Handle;
	FString InputContentType;
	FString InputExpectedFormat;
	FAvaMediaHttpRouteDesc()
	{
	
	}
	FAvaMediaHttpRouteDesc(FHttpRouteHandle InHandle, FString InContentType, FString InExpectedFormat)
	{
		Handle = InHandle;
		InputContentType = InContentType;
		InputExpectedFormat = InExpectedFormat;
	}
};

DECLARE_LOG_CATEGORY_EXTERN(LogAvaMediaHttpServer, Log, All);

class FAvaMediaHttpServer;

struct FHttpRouteBuilder
{
public:
	template <typename MessageType, typename HandlerType>
	struct TMessageHandler
	{
		typedef void (HandlerType::*FuncType)(const MessageType&, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>&);
	};

public:
	FHttpRouteBuilder(const TSharedPtr<FAvaMediaHttpServer>& InHttpServer, const TSharedPtr<FAvaMediaPlaybackServer>& InPlaybackServer);
	
public:	
	template <typename MessageType>
	FHttpRouteBuilder& Route(const FString& InPath, const EHttpServerRequestVerbs& InVerb, typename TMessageHandler<MessageType, FAvaMediaPlaybackServer>::FuncType InHandlerFunc);
	
private:
	TSharedPtr<FAvaMediaHttpServer> HttpServer;
	TSharedPtr<FAvaMediaPlaybackServer> PlaybackServer;
};

class FAvaMediaHttpServer : public TSharedFromThis<FAvaMediaHttpServer>
{
public:
	typedef struct FHttpRouteBuilder Builder;
	
	FAvaMediaHttpServer() = default;
	virtual ~FAvaMediaHttpServer() = default;

	void Start(int32 InPortToUse = 10123);

	void RegisterRoutes();

	/**
	 * Try to get a route registered under given friendly name. Returns false if could not be found.
	 */
	bool GetRegisteredRoute(FName RouteName, FAvaMediaHttpRouteInfo& OutRouteInfo);

	void RegisterNewRoute(FAvaMediaHttpRouteInfo InRouteInfo, const FHttpRequestHandler& Handler, bool bOverrideIfBound = false);

	/**
	 * Register a new route.
	 * Will override existing routes if option is set, otherwise will error and fail to bind.
	 */
	void RegisterNewRoute(FName RouteName, const FHttpPath& HttpPath, const EHttpServerRequestVerbs& RequestVerbs, const FHttpRequestHandler& Handler, bool bOverrideIfBound = false, FString OptionalContentType = TEXT(""), FString OptionalExpectedFormat = TEXT(""));

	/**
	 * Clean up a route.
	 * Can be set to fail if trying to unbind an unbound route.
	 */
	void CleanUpRoute(FName RouteName, bool bFailIfUnbound = false);

	/**
	 * Default Route Listing http call. Spits out all registered routes and describes them via a REST API call.
	 * Always registered at /listroutes GET by default
	 */
	bool HttpListOpenRoutes(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	
	bool IsRunning() const { return HttpRouter.IsValid(); }

protected:
	int PortToUse = 10123;
	TSharedPtr<IHttpRouter> HttpRouter;
	TMap<FName, FAvaMediaHttpRouteDesc> RegisteredRoutes;
};

template <typename MessageType>
bool UE::Ava::Web::Private::DeserializeRequest(
	const FHttpServerRequest& InRequest,
	const FHttpResultCallback* InCompleteCallback,
	MessageType& OutDeserializedRequest)
{
	TArray<uint8> TCHARPayload;

	const int32 StartIndex = TCHARPayload.Num();
	TCHARPayload.AddUninitialized(FUTF8ToTCHAR_Convert::ConvertedLength((ANSICHAR*)InRequest.Body.GetData(), InRequest.Body.Num() / sizeof(ANSICHAR)) * sizeof(TCHAR));
	FUTF8ToTCHAR_Convert::Convert((TCHAR*)(TCHARPayload.GetData() + StartIndex), (TCHARPayload.Num() - StartIndex) / sizeof(TCHAR), (ANSICHAR*)InRequest.Body.GetData(), InRequest.Body.Num() / sizeof(ANSICHAR));

	FMemoryReaderView Reader(TCHARPayload);
	FJsonStructDeserializerBackend DeserializerBackend(Reader);
	if (!FStructDeserializer::Deserialize(&OutDeserializedRequest, *MessageType::StaticStruct(), DeserializerBackend, FStructDeserializerPolicies()))
	{
		if (InCompleteCallback)
		{
			TUniquePtr<FHttpServerResponse> Response = MakeUnique<FHttpServerResponse>();
			Response->Code = EHttpServerResponseCodes::BadRequest;
			(*InCompleteCallback)(MoveTemp(Response));
		}
		return false;
	}

	return true;
}

template <typename MessageType>
FHttpRouteBuilder& FHttpRouteBuilder::Route(
	const FString& InPath,
	const EHttpServerRequestVerbs& InVerb,
	typename TMessageHandler<MessageType, FAvaMediaPlaybackServer>::FuncType InHandlerFunc)
{
	static_assert(TModels<CStaticStructProvider, MessageType>::Value, "MessageType must be a UStruct");

	TWeakPtr<FAvaMediaPlaybackServer> WeakPlaybackServerPtr = TWeakPtr<FAvaMediaPlaybackServer>(PlaybackServer);
	
	HttpServer->RegisterNewRoute(MessageType::StaticStruct()->GetFName(), FHttpPath(InPath), InVerb,
	FHttpRequestHandler::CreateLambda([WeakPlaybackServerPtr, HandlerFunc = MoveTemp(InHandlerFunc)](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
	{
		if (WeakPlaybackServerPtr.IsValid())
		{
			MessageType DeserializedMessage;
			if (!UE::Ava::Web::Private::DeserializeRequest(Request, &OnComplete, DeserializedMessage))
			{
				return false;
			}

			static TSharedRef<IMessageContext> EmptyContext = MakeShared<FEmptyMessageContext>();
			(WeakPlaybackServerPtr.Pin().Get()->*HandlerFunc)(DeserializedMessage, EmptyContext);

			if (OnComplete)
			{
				TUniquePtr<FHttpServerResponse> Response = MakeUnique<FHttpServerResponse>();
				Response->Headers.Add(TEXT("content-type"), { TEXT("application/json") });
				Response->Code = EHttpServerResponseCodes::Ok;

				OnComplete(MoveTemp(Response));
			}
			
			return true;
		}
		return false;
	}), true);

	return *this;
}
