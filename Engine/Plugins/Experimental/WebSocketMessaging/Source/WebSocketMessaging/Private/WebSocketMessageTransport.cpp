// Copyright Epic Games, Inc. All Rights Reserved.


#include "WebSocketMessageTransport.h"
#include "Backends/CborStructSerializerBackend.h"
#include "CborReader.h"
#include "CborWriter.h"
#include "IMessageContext.h"
#include "IMessageTransportHandler.h"
#include "INetworkingWebSocket.h"
#include "IWebSocket.h"
#include "IWebSocketNetworkingModule.h"
#include "IWebSocketServer.h"
#include "JsonObjectConverter.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/ArrayWriter.h"
#include "StructSerializer.h"
#include "WebSocketDeserializedMessage.h"
#include "WebSocketMessagingModule.h"
#include "WebSocketMessagingSettings.h"
#include "WebSocketsModule.h"

bool FWebSocketMessageConnection::IsConnected() const
{
	if (WebSocketConnection)
	{
		return WebSocketConnection->IsConnected();
	}

	return WebSocketServerConnection != nullptr;
}

void FWebSocketMessageConnection::Close()
{
	if (WebSocketConnection)
	{
		return WebSocketConnection->Close();
	}
}

FWebSocketMessageTransport::FWebSocketMessageTransport()
{
}

FWebSocketMessageTransport::~FWebSocketMessageTransport()
{
}

FName FWebSocketMessageTransport::GetDebugName() const
{
	static const FName DebugName("WebSocketMessageTransport");
	return DebugName;
}

bool FWebSocketMessageTransport::StartTransport(IMessageTransportHandler& Handler)
{
	const UWebSocketMessagingSettings* Settings = GetDefault<UWebSocketMessagingSettings>();
	
	TransportHandler = &Handler;
	
	const int32 ServerPort = Settings->GetServerPort();
	
	// Cache the settings to be able to detect changes.
	LastServerPort = ServerPort;
	LastServerBindAddress = Settings->ServerBindAddress;
	LastConnectionEndpoints = Settings->ConnectToEndpoints;
	LastHttpHeaders = Settings->HttpHeaders;

	FString ServerBindAddress = Settings->ServerBindAddress;

	if (ServerBindAddress.Compare(TEXT("0.0.0.0")) == 0
		|| ServerBindAddress.Compare(TEXT("any"), ESearchCase::IgnoreCase) == 0)
	{
		ServerBindAddress = TEXT("");	// Leaving empty will bind to all adapters.
	}

	if (ServerPort > 0)
	{
		IWebSocketNetworkingModule* WebSocketNetworkingModule = FModuleManager::Get().LoadModulePtr<IWebSocketNetworkingModule>(TEXT("WebSocketNetworking"));
		if (WebSocketNetworkingModule)
		{
			Server = WebSocketNetworkingModule->CreateServer();
			if (Server)
			{
				FWebSocketClientConnectedCallBack Callback;
				Callback.BindThreadSafeSP(this, &FWebSocketMessageTransport::ClientConnected);
				
				if (!Server->Init(ServerPort, Callback, ServerBindAddress))
				{
					Server.Reset();
					UE_LOG(LogWebSocketMessaging, Error, TEXT("Unable to start WebSocketMessaging Server on port %d"), ServerPort);
				}
				else
				{
					ServerTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateThreadSafeSP(this, &FWebSocketMessageTransport::ServerTick));
					UE_LOG(LogWebSocketMessaging, Log, TEXT("WebSocketMessaging Server started on port %d"), ServerPort);
				}
			}
		}
		else
		{
			UE_LOG(LogWebSocketMessaging, Log, TEXT("Unable to load WebSocketNetworking module, ensure to enable it"));
		}
	}

	for (const FString& Url : Settings->ConnectToEndpoints)
	{
		FGuid Guid = FGuid::NewGuid();

		TMap<FString, FString> Headers;
		Headers.Add(WebSocketMessaging::Header::TransportId, Guid.ToString());
		for (const TPair<FString, FString>& Pair : Settings->HttpHeaders)
		{
			Headers.Add(Pair.Key, Pair.Value);
		}

		TSharedRef<IWebSocket, ESPMode::ThreadSafe> WebSocketConnection = FWebSocketsModule::Get().CreateWebSocket(Url, FString(), Headers);

		FWebSocketMessageConnectionRef WebSocketMessageConnection = MakeShared<FWebSocketMessageConnection>(Url, Guid, WebSocketConnection);

		WebSocketConnection->OnMessage().AddThreadSafeSP(this, &FWebSocketMessageTransport::OnJsonMessage, WebSocketMessageConnection);
		WebSocketConnection->OnClosed().AddThreadSafeSP(this, &FWebSocketMessageTransport::OnClosed, WebSocketMessageConnection);
		WebSocketConnection->OnConnected().AddThreadSafeSP(this, &FWebSocketMessageTransport::OnConnected, WebSocketMessageConnection);
		WebSocketConnection->OnConnectionError().AddThreadSafeSP(this, &FWebSocketMessageTransport::OnConnectionError, WebSocketMessageConnection);

		WebSocketMessageConnections.Add(Guid, WebSocketMessageConnection);

		WebSocketConnection->Connect();
	}

	return true;
}

void FWebSocketMessageTransport::StopTransport()
{
	FTSTicker::GetCoreTicker().RemoveTicker(ServerTickerHandle);
	
	if (Server.IsValid())
	{
		Server.Reset();
	}

	for (TPair<FGuid, FWebSocketMessageConnectionRef> Pair : WebSocketMessageConnections)
	{
		Pair.Value->bDestroyed = true;
		Pair.Value->Close();
	}
	WebSocketMessageConnections.Empty();
}

bool FWebSocketMessageTransport::NeedsRestart() const
{
	const UWebSocketMessagingSettings* Settings = GetDefault<UWebSocketMessagingSettings>();
	
	if (LastServerPort != Settings->GetServerPort()
		|| LastServerBindAddress != Settings->ServerBindAddress
		|| LastConnectionEndpoints != Settings->ConnectToEndpoints
		|| !LastHttpHeaders.OrderIndependentCompareEqual(Settings->HttpHeaders))
	{
		return true;
	}

	return false;
}

void FWebSocketMessageTransport::OnClosed(int32 Code, const FString& Reason, bool bUserClose, FWebSocketMessageConnectionRef WebSocketMessageConnection)
{
	UE_LOG(LogWebSocketMessaging, Log, TEXT("Connection to %s closed, Code: %d Reason: \"%s\" UserClose: %s, retrying..."), *WebSocketMessageConnection->Url, Code, *Reason, bUserClose ? TEXT("true") : TEXT("false"));
	ForgetTransportNode(WebSocketMessageConnection);
	WebSocketMessageConnection->bIsConnecting = false;
	RetryConnection(WebSocketMessageConnection);
}

void FWebSocketMessageTransport::OnConnectionError(const FString& Message, FWebSocketMessageConnectionRef WebSocketMessageConnection)
{
	if (!WebSocketMessageConnection->bIsConnecting)
	{
		UE_LOG(LogWebSocketMessaging, Log, TEXT("Connection to %s error: %s, retrying..."), *WebSocketMessageConnection->Url, *Message);
	}
	ForgetTransportNode(WebSocketMessageConnection);
	WebSocketMessageConnection->bIsConnecting = false;
	RetryConnection(WebSocketMessageConnection);
}

void FWebSocketMessageTransport::OnJsonMessage(const FString& Message, FWebSocketMessageConnectionRef WebSocketMessageConnection)
{
	FString ParseError;
	const TSharedRef<FWebSocketDeserializedMessage> Context = MakeShared<FWebSocketDeserializedMessage>();
	if (Context->ParseJson(Message, ParseError))
	{
		TransportHandler->ReceiveTransportMessage(Context, WebSocketMessageConnection->Guid);
	}
	else
	{
		UE_LOG(LogWebSocketMessaging, Log, TEXT("Invalid Json Message received on %s: %s"), *WebSocketMessageConnection->Url, *ParseError);
	}
}

void FWebSocketMessageTransport::OnServerJsonMessage(void* Data, int32 DataSize, FWebSocketMessageConnectionRef WebSocketMessageConnection)
{
	FString Message(DataSize, reinterpret_cast<UTF8CHAR*>(Data));
	OnJsonMessage(Message, WebSocketMessageConnection);
}

class FWebSocketMessageTransportSerializeHelper
{
public:
	static const TMap<EMessageScope, FString> MessageScopeStringMapping;

	static bool Serialize(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext, bool bInStandardizeCase, FString& OutJsonMessage)
	{
		TSharedRef<FJsonObject> JsonRoot = MakeShared<FJsonObject>();
		JsonRoot->SetStringField(WebSocketMessaging::Tag::Sender, InContext->GetSender().ToString());
		TArray<TSharedPtr<FJsonValue>> JsonRecipients;
		for (const FMessageAddress& Recipient : InContext->GetRecipients())
		{
			JsonRecipients.Add(MakeShared<FJsonValueString>(Recipient.ToString()));
		}
		JsonRoot->SetArrayField(WebSocketMessaging::Tag::Recipients, JsonRecipients);
		JsonRoot->SetStringField(WebSocketMessaging::Tag::MessageType, InContext->GetMessageTypePathName().ToString());
		JsonRoot->SetNumberField(WebSocketMessaging::Tag::Expiration, InContext->GetExpiration().ToUnixTimestamp());
		JsonRoot->SetNumberField(WebSocketMessaging::Tag::TimeSent, InContext->GetTimeSent().ToUnixTimestamp());
		JsonRoot->SetStringField(WebSocketMessaging::Tag::Scope, MessageScopeStringMapping[InContext->GetScope()]);


		TSharedRef<FJsonObject> JsonAnnotations = MakeShared<FJsonObject>();
		for (const TPair<FName, FString>& Pair : InContext->GetAnnotations())
		{
			JsonAnnotations->SetStringField(Pair.Key.ToString(), Pair.Value);
		}
		JsonRoot->SetObjectField(WebSocketMessaging::Tag::Annotations, JsonAnnotations);

		TSharedRef<FJsonObject> OutJsonObject = MakeShared<FJsonObject>();

		constexpr int64 CheckFlags = 0;
		constexpr int64 SkipFlags = 0;
		const EJsonObjectConversionFlags ConversionFlags = bInStandardizeCase ? EJsonObjectConversionFlags::None : EJsonObjectConversionFlags::SkipStandardizeCase;
		if (!FJsonObjectConverter::UStructToJsonObject(InContext->GetMessageTypeInfo().Get(), InContext->GetMessage(), OutJsonObject,
			CheckFlags, SkipFlags, nullptr, ConversionFlags))
		{
			return false;
		}

		JsonRoot->SetObjectField(WebSocketMessaging::Tag::Message, OutJsonObject);

		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonMessage);
		return FJsonSerializer::Serialize(JsonRoot, Writer);
	}

	static bool Serialize(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext, bool bInStandardizeCase, FArrayWriter& OutCborBinaryWriter)
	{
		FCborHeader Header(ECborCode::Map | ECborCode::Indefinite);
		OutCborBinaryWriter << Header;
			
		{
			FCborWriter CborWriter(&OutCborBinaryWriter);
			
			CborWriter.WriteValue(FString(WebSocketMessaging::Tag::Sender));
			CborWriter.WriteValue(InContext->GetSender().ToString());
			
			CborWriter.WriteValue(FString(WebSocketMessaging::Tag::Recipients));
			CborWriter.WriteContainerStart(ECborCode::Array, -1);
			for (const FMessageAddress& Recipient : InContext->GetRecipients())
			{
				CborWriter.WriteValue(Recipient.ToString());
			}
			CborWriter.WriteContainerEnd();
			
			CborWriter.WriteValue(FString(WebSocketMessaging::Tag::MessageType));
			CborWriter.WriteValue(InContext->GetMessageTypePathName().ToString());
			
			CborWriter.WriteValue(FString(WebSocketMessaging::Tag::Expiration));
			CborWriter.WriteValue(InContext->GetExpiration().ToUnixTimestamp());
			
			CborWriter.WriteValue(FString(WebSocketMessaging::Tag::TimeSent));
			CborWriter.WriteValue(InContext->GetTimeSent().ToUnixTimestamp());
			
			CborWriter.WriteValue(FString(WebSocketMessaging::Tag::Scope));
			CborWriter.WriteValue(MessageScopeStringMapping[InContext->GetScope()]);
			
			
			CborWriter.WriteValue(FString(WebSocketMessaging::Tag::Annotations));
			CborWriter.WriteContainerStart(ECborCode::Map, -1);
			for (const TPair<FName, FString>& Annotation : InContext->GetAnnotations())
			{
				CborWriter.WriteValue(Annotation.Key.ToString());
				CborWriter.WriteValue(Annotation.Value);
			}
			CborWriter.WriteContainerEnd();
			
			CborWriter.WriteValue(FString(WebSocketMessaging::Tag::Message));
		}
		FCborStructSerializerBackend Backend(OutCborBinaryWriter, EStructSerializerBackendFlags::Default);
		FStructSerializer::Serialize(InContext->GetMessage(), *InContext->GetMessageTypeInfo().Get(), Backend);
			
		Header.Set(ECborCode::Break);
		OutCborBinaryWriter << Header;

		return true;
	}

	template<typename OutputType>
	struct TOnDemandSerializer
	{
		OutputType OutputMessage;
		bool bIsSerializeAttempted = false;
		bool bIsSerialized = false;
		bool bStandardizeCase = true;

		bool SerializeOnDemand(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
		{
			if (!bIsSerializeAttempted)
			{
				bIsSerializeAttempted = true;
				bIsSerialized = FWebSocketMessageTransportSerializeHelper::Serialize(InContext, bStandardizeCase, OutputMessage);
			}
			return bIsSerialized;
		}
	};
};

const TMap<EMessageScope, FString> FWebSocketMessageTransportSerializeHelper::MessageScopeStringMapping =
{
	{EMessageScope::Thread, "Thread"},
	{EMessageScope::Process, "Process"},
	{EMessageScope::Network, "Network"},
	{EMessageScope::All, "All"}
};

bool FWebSocketMessageTransport::TransportMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context, const TArray<FGuid>& Recipients)
{
	TMap<FGuid, FWebSocketMessageConnectionRef> RecipientConnections;

	if (Recipients.Num() == 0)
	{
		// broadcast the message to all valid connections
		RecipientConnections = WebSocketMessageConnections.FilterByPredicate([](const TPair<FGuid, FWebSocketMessageConnectionRef>& Pair) -> bool
			{
				return !Pair.Value->bDestroyed && Pair.Value->IsConnected();
			});
	}
	else
	{
		// Find connections for each recipient.  We do not transport unicast messages for unknown nodes.
		for (const FGuid& Recipient : Recipients)
		{
			FWebSocketMessageConnectionRef* RecipientConnection = WebSocketMessageConnections.Find(Recipient);
			if (RecipientConnection && !(*RecipientConnection)->bDestroyed && (*RecipientConnection)->IsConnected())
			{
				RecipientConnections.Add(Recipient, *RecipientConnection);
			}
		}
	}

	if (RecipientConnections.Num() == 0)
	{
		return false;
	}

	const UWebSocketMessagingSettings* Settings = GetDefault<UWebSocketMessagingSettings>();
	
	FWebSocketMessageTransportSerializeHelper::TOnDemandSerializer<FString> JsonSerializer;
	JsonSerializer.bStandardizeCase = Settings->bMessageSerializationStandardizeCase;
	
	FWebSocketMessageTransportSerializeHelper::TOnDemandSerializer<FArrayWriter> CborSerializer;
	
	// Serialize the message on demand in the appropriate format for each peer connections.
	for (const TPair<FGuid, FWebSocketMessageConnectionRef>& Connection : RecipientConnections)
	{
		if (Connection.Value->WebSocketConnection.IsValid())
		{
			// Remark: client connections are always text/json
			if (JsonSerializer.SerializeOnDemand(Context))
			{
				Connection.Value->WebSocketConnection->Send(JsonSerializer.OutputMessage);
			}
		}
		else if (Connection.Value->WebSocketServerConnection)
		{
			// Remark: server connections are always binary.
			if (Settings->ServerTransportFormat == EWebSocketMessagingTransportFormat::Json)
			{
				if (JsonSerializer.SerializeOnDemand(Context))
				{
					auto MessageUtf8 = StringCast<UTF8CHAR>(*JsonSerializer.OutputMessage);
					Connection.Value->WebSocketServerConnection->Send(
						reinterpret_cast<const uint8*>(MessageUtf8.Get()), MessageUtf8.Length(), /*bPrependSize*/ false);
				}
			}
			else
			{
				if (CborSerializer.SerializeOnDemand(Context))
				{
					Connection.Value->WebSocketServerConnection->Send(
						CborSerializer.OutputMessage.GetData(), CborSerializer.OutputMessage.Num(), /*bPrependSize*/ false);
				}
			}
		}
	}

	return true;
}

void FWebSocketMessageTransport::OnConnected(FWebSocketMessageConnectionRef WebSocketMessageConnection)
{
	UE_LOG(LogWebSocketMessaging, Log, TEXT("Connected to %s"), *WebSocketMessageConnection->Url);
	WebSocketMessageConnection->bIsConnecting = false;
}

void FWebSocketMessageTransport::OnServerConnectionClosed(FWebSocketMessageConnectionRef WebSocketMessageConnection)
{
	UE_LOG(LogWebSocketMessaging, Log, TEXT("%s disconnected"), *WebSocketMessageConnection->Url);
	ForgetTransportNode(WebSocketMessageConnection);
	WebSocketMessageConnections.Remove(WebSocketMessageConnection->Guid);
}

void FWebSocketMessageTransport::RetryConnection(FWebSocketMessageConnectionRef WebSocketMessageConnection)
{
	if (WebSocketMessageConnection->bIsConnecting)
	{
		return;
	}

	WebSocketMessageConnection->RetryHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float DeltaTime, FWebSocketMessageConnectionRef WebSocketMessageConnection)
		{
			if (!WebSocketMessageConnection->bDestroyed && !WebSocketMessageConnection->bIsConnecting && !WebSocketMessageConnection->WebSocketConnection->IsConnected())
			{
				WebSocketMessageConnection->bIsConnecting = true;
				WebSocketMessageConnection->WebSocketConnection->Connect();
			}
			return false;
		}, WebSocketMessageConnection), 1.0f);
}

void FWebSocketMessageTransport::ClientConnected(INetworkingWebSocket* NetworkingWebSocket)
{
	FString RemoteEndPoint = NetworkingWebSocket->RemoteEndPoint(true);
	UE_LOG(LogWebSocketMessaging, Log, TEXT("New WebSocket Server connection: %s"), *RemoteEndPoint);

	FGuid Guid = FGuid::NewGuid();

	FWebSocketMessageConnectionRef WebSocketMessageConnection = MakeShared<FWebSocketMessageConnection>(RemoteEndPoint, Guid, NetworkingWebSocket);

	NetworkingWebSocket->SetReceiveCallBack(FWebSocketPacketReceivedCallBack::CreateThreadSafeSP(this, &FWebSocketMessageTransport::OnServerJsonMessage, WebSocketMessageConnection));
	NetworkingWebSocket->SetSocketClosedCallBack(FWebSocketInfoCallBack::CreateThreadSafeSP(this, &FWebSocketMessageTransport::OnServerConnectionClosed, WebSocketMessageConnection));
	NetworkingWebSocket->SetErrorCallBack(FWebSocketInfoCallBack::CreateThreadSafeSP(this, &FWebSocketMessageTransport::OnServerConnectionClosed, WebSocketMessageConnection));

	WebSocketMessageConnections.Add(Guid, WebSocketMessageConnection);
}

bool FWebSocketMessageTransport::ServerTick(float DeltaTime)
{
	if (Server.IsValid())
	{
		Server->Tick();
	}

	return true;
}

void FWebSocketMessageTransport::ForgetTransportNode(FWebSocketMessageConnectionRef WebSocketMessageConnection)
{
	if (TransportHandler)
	{
		TransportHandler->ForgetTransportNode(WebSocketMessageConnection->Guid);
	}
}
