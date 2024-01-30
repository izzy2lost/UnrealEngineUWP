// Copyright Epic Games, Inc. All Rights Reserved.

#include "Http/AvaMediaHttpServer.h"

#include "AvaMediaModule.h"
#include "HttpServerModule.h"
#include "HttpServerResponse.h"
#include "Playback/AvaMediaPlaybackServer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY(LogAvaMediaHttpServer)

static const FString& GetHttpRouteVerbString(EHttpServerRequestVerbs InVerbs)
{
	static TMap<EHttpServerRequestVerbs, FString> VerbStrings =
	{
		{ EHttpServerRequestVerbs::VERB_POST, TEXT("POST") },
		{ EHttpServerRequestVerbs::VERB_PUT, TEXT("PUT") },
		{ EHttpServerRequestVerbs::VERB_GET, TEXT("GET") },
		{ EHttpServerRequestVerbs::VERB_PATCH, TEXT("PATCH") },
		{ EHttpServerRequestVerbs::VERB_DELETE, TEXT("DELETE") },
		{ EHttpServerRequestVerbs::VERB_NONE, TEXT("NONE") },
	};

	if (const FString* FoundEntry = VerbStrings.Find(InVerbs))
	{
		return *FoundEntry;		
	}

	static const FString Unknown = TEXT("UNKNOWN");
	return Unknown;
}

FHttpRouteBuilder::FHttpRouteBuilder(
	const TSharedPtr<FAvaMediaHttpServer>& InHttpServer,
	const TSharedPtr<FAvaMediaPlaybackServer>& InPlaybackServer)
	: HttpServer(InHttpServer)
	, PlaybackServer(InPlaybackServer)
{
}

void FAvaMediaHttpServer::Start(int32 InPortToUse)
{
	PortToUse = InPortToUse;
	HttpRouter = FHttpServerModule::Get().GetHttpRouter(PortToUse);

	RegisterRoutes();

	FHttpServerModule::Get().StartAllListeners();
}

void FAvaMediaHttpServer::RegisterRoutes()
{
	const TSharedPtr<FAvaMediaPlaybackServer> MediaPlaybackServer = FModuleManager::GetModulePtr<FAvaMediaModule>(TEXT("AvalancheMedia"))->GetMediaPlaybackServer();
	check(MediaPlaybackServer.IsValid());
	
	// Map as per FAvaMediaPlaybackServer::Init(const FString& AssignedServerName)
	FAvaMediaHttpServer::Builder(AsShared(), MediaPlaybackServer)
	.Route<FAvaMediaPlaybackPing>(TEXT("/playback/ping"), EHttpServerRequestVerbs::VERB_POST, &FAvaMediaPlaybackServer::HandlePlaybackPing)
	.Route<FAvaDeviceProviderDataRequest>(TEXT("/playback/devices"), EHttpServerRequestVerbs::VERB_POST, &FAvaMediaPlaybackServer::HandleDeviceProviderDataRequest)
	.Route<FAvaMediaPlaybackRequest>(TEXT("/playback"), EHttpServerRequestVerbs::VERB_POST, &FAvaMediaPlaybackServer::HandlePlaybackRequest)
	.Route<FAvaMediaAnimPlaybackRequest>(TEXT("/playback/animation"), EHttpServerRequestVerbs::VERB_POST, &FAvaMediaPlaybackServer::HandleAnimPlaybackRequest)
	.Route<FAvaMediaBroadcastRequest>(TEXT("/broadcast"), EHttpServerRequestVerbs::VERB_POST, &FAvaMediaPlaybackServer::HandleBroadcastRequest)
	.Route<FAvaMediaBroadcastStatusRequest>(TEXT("/broadcast/status"), EHttpServerRequestVerbs::VERB_POST, &FAvaMediaPlaybackServer::HandleBroadcastStatusRequest);

	// We always want the ListRegisteredRoutes route bound, no matter what.
	RegisterNewRoute(TEXT("ListRegisteredRoutes"), FHttpPath("/listroutes"), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateSP(this, &FAvaMediaHttpServer::HttpListOpenRoutes), true);
}

bool FAvaMediaHttpServer::GetRegisteredRoute(FName RouteName, FAvaMediaHttpRouteInfo& OutRouteInfo)
{
	if (RegisteredRoutes.Find(RouteName))
	{
		OutRouteInfo.RouteName = RouteName;
		OutRouteInfo.RoutePath = RegisteredRoutes[RouteName].Handle->Path;
		OutRouteInfo.RequestVerbs = RegisteredRoutes[RouteName].Handle->Verbs;
		OutRouteInfo.InputContentType = RegisteredRoutes[RouteName].InputContentType;
		OutRouteInfo.InputExpectedFormat = RegisteredRoutes[RouteName].InputExpectedFormat;
		return true;
	}
	return false;
}

void FAvaMediaHttpServer::RegisterNewRoute(FName RouteName, const FHttpPath& HttpPath, const EHttpServerRequestVerbs& RequestVerbs, const FHttpRequestHandler& Handler, bool bOverrideIfBound /* = false */, FString OptionalContentType /* = TEXT("")*/, FString OptionalExpectedFormat /*= TEXT("")*/)
{
	FAvaMediaHttpRouteInfo InRouteInfo;
	InRouteInfo.RouteName = RouteName;
	InRouteInfo.RoutePath = HttpPath;
	InRouteInfo.RequestVerbs = RequestVerbs;
	InRouteInfo.InputContentType = OptionalContentType;
	InRouteInfo.InputExpectedFormat = OptionalExpectedFormat;
	RegisterNewRoute(InRouteInfo, Handler, bOverrideIfBound);
}

void FAvaMediaHttpServer::RegisterNewRoute(FAvaMediaHttpRouteInfo InRouteInfo, const FHttpRequestHandler& Handler, bool bOverrideIfBound /* = false */)
{
	if (HttpRouter.IsValid())
	{
		if (RegisteredRoutes.Find(InRouteInfo.RouteName))
		{
			if (!bOverrideIfBound)
			{
				UE_LOG(LogAvaMediaHttpServer, Error, TEXT("Failed to bind route with friendly key %s - a route at location %s already exists."), *InRouteInfo.RouteName.ToString(), *InRouteInfo.RoutePath.GetPath());
				return;
			}
			UE_LOG(LogAvaMediaHttpServer, Log, TEXT("Overwriting route at friendly key %s - from %s to %s "), *InRouteInfo.RouteName.ToString(), *RegisteredRoutes[InRouteInfo.RouteName].Handle->Path, *InRouteInfo.RoutePath.GetPath());
			HttpRouter->UnbindRoute(RegisteredRoutes[InRouteInfo.RouteName].Handle);
		}
		FAvaMediaHttpRouteDesc RouteDesc;
		RouteDesc.Handle = HttpRouter->BindRoute(InRouteInfo.RoutePath, InRouteInfo.RequestVerbs, Handler);
		RouteDesc.InputContentType = InRouteInfo.InputContentType;
		RouteDesc.InputExpectedFormat = InRouteInfo.InputExpectedFormat;
		RegisteredRoutes.Add(InRouteInfo.RouteName, RouteDesc);
	}
}

void FAvaMediaHttpServer::CleanUpRoute(FName RouteName, bool bFailIfUnbound /* = false */)
{
	if (HttpRouter.IsValid())
	{
		if (RegisteredRoutes.Find(RouteName))
		{
			HttpRouter->UnbindRoute(RegisteredRoutes[RouteName].Handle);
			RegisteredRoutes.Remove(RouteName);
			UE_LOG(LogAvaMediaHttpServer, Log, TEXT("Route name %s was unbound!"), *RouteName.ToString());
		}
		else
		{
			UE_LOG(LogAvaMediaHttpServer, Warning, TEXT("Route name %s does not exist, could not unbind."), *RouteName.ToString());
			check(!bFailIfUnbound);
		}
	}
}

bool FAvaMediaHttpServer::HttpListOpenRoutes(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	FString ResponseStr;
	TArray<FName> OutRouteKeys;
	RegisteredRoutes.GetKeys(OutRouteKeys);
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&ResponseStr);
	JsonWriter->WriteArrayStart();
	for (const FName& RouteKey : OutRouteKeys)
	{
		JsonWriter->WriteObjectStart();
		JsonWriter->WriteValue(TEXT("name"), RouteKey.ToString());
		JsonWriter->WriteValue(TEXT("route"), RegisteredRoutes[RouteKey].Handle->Path);
		JsonWriter->WriteValue(TEXT("verb"), GetHttpRouteVerbString(RegisteredRoutes[RouteKey].Handle->Verbs));
		if (!RegisteredRoutes[RouteKey].InputContentType.IsEmpty())
		{
			JsonWriter->WriteValue(TEXT("inputContentType"), RegisteredRoutes[RouteKey].InputContentType);
		}
		if (!RegisteredRoutes[RouteKey].InputExpectedFormat.IsEmpty())
		{
			JsonWriter->WriteValue(TEXT("inputExpectedFormat"), RegisteredRoutes[RouteKey].InputExpectedFormat);
		}
		JsonWriter->WriteObjectEnd();
	}
	JsonWriter->WriteArrayEnd();
	JsonWriter->Close();
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseStr, TEXT("application/json"));
	OnComplete(MoveTemp(Response));
	return true;
}