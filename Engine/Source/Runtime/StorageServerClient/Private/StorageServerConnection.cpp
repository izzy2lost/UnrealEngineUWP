// Copyright Epic Games, Inc. All Rights Reserved.

#include "StorageServerConnection.h"
#include "DebugStorageServerConnection.h"

#include "IO/IoDispatcher.h"
#include "IPAddress.h"
#include "Misc/App.h"
#include "Misc/ScopeLock.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/StringBuilder.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Memory/MemoryView.h"
#include "Memory/SharedBuffer.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "IO/PackageStore.h"
#include "StorageSocketConnectionBackend.h"
#include "StoragePlatformConnectionBackend.h"
#include "GenericPlatform/GenericPlatformHostCommunication.h"
#include "Engine/Engine.h"

#if !UE_BUILD_SHIPPING

DEFINE_LOG_CATEGORY_STATIC(LogStorageServerConnection, Log, All);

TRACE_DECLARE_MEMORY_COUNTER(ZenHttpClientSerializedBytes, TEXT("ZenClient/SerializedBytes"));

static uint64 GetCompressedOffset(const FCompressedBuffer& Buffer, uint64 RawOffset)
{
	if (RawOffset > 0)
	{
		uint64 BlockSize = 0;
		ECompressedBufferCompressor Compressor;
		ECompressedBufferCompressionLevel CompressionLevel;
		const bool bOk = Buffer.TryGetCompressParameters(Compressor, CompressionLevel, BlockSize);
		check(bOk);

		return BlockSize > 0 ? RawOffset % BlockSize : 0;
	}

	return 0;
}

FStorageServerRequest::FStorageServerRequest(FAnsiStringView Verb, FAnsiStringView Resource, FAnsiStringView Hostname, EStorageServerContentType Accept)
: AcceptType(Accept)
{
	SetIsSaving(true);
	HeaderBuffer << Verb << " " << Resource << " HTTP/1.1\r\n" 
		<< "Host: " << Hostname << "\r\n"
		<< "Connection: Keep-Alive\r\n"
		<< "Accept: " << GetMimeTypeString(Accept) << "\r\n";
}

IStorageConnectionSocket* FStorageServerRequest::Send(FStorageServerConnection& Owner, bool bLogOnError)
{
	StartTiming();
	if (BodyBuffer.Num())
	{
		HeaderBuffer.Append("Content-Length: ").Appendf("%d\r\n", BodyBuffer.Num());
	}
	HeaderBuffer << "\r\n";
	int32 BytesLeft = HeaderBuffer.Len();
	
	auto Send = [](IStorageConnectionSocket* Socket, const uint8* Data, int32 Length)
	{
		int32 BytesLeft = Length;
		while (BytesLeft > 0)
		{
			if (!Socket->Send(Data, BytesLeft))
			{
				return false;
			}
			BytesLeft -= BytesLeft;
		}
		return true;
	};

	int32 Attempts = 0;
	FStorageConnectionBackend* Backend = Owner.GetConnectionBackend();
	while (Attempts < 10)
	{
		IStorageConnectionSocket* Socket;
		IStorageConnectionSocket* SocketFromPool;
		Socket = SocketFromPool = Backend->AcquireSocketFromPool();
		if (!Socket)
		{
			Socket = Backend->AcquireNewSocket();
			if (!Socket)
			{
				++Attempts;
				continue;
			}
		}

		if (!Send(Socket, reinterpret_cast<const uint8*>(HeaderBuffer.GetData()), HeaderBuffer.Len()) ||
			!Send(Socket, BodyBuffer.GetData(), BodyBuffer.Num()))
		{
			Backend->ReleaseSocket(Socket, false);
			if (!SocketFromPool)
			{
				++Attempts;
			}
			continue;
		}
		return Socket;
	}

	if (bLogOnError)
	{
		UE_LOG(LogStorageServerConnection, Fatal, TEXT("Failed sending request to storage server."));
	}
	return nullptr;
}

void FStorageServerRequest::Serialize(void* V, int64 Length)
{
	int32 Index = BodyBuffer.AddUninitialized(Length);
	uint8* Dest = BodyBuffer.GetData() + Index;
	FMemory::Memcpy(Dest, V, Length);
}

FStorageServerResponse::FStorageServerResponse(FStorageServerConnection& InOwner, IStorageConnectionSocket& InSocket)
	: Owner(InOwner)
	, Socket(&InSocket)
{
	SetIsLoading(true);
	uint8 Buffer[1024];
	int32 TotalReadFromSocket = 0;
	auto ReadResponseLine = [&Buffer, &InSocket, &TotalReadFromSocket]() -> FAnsiStringView
	{
		for (;;)
		{
			uint64 BytesRead;
			InSocket.Recv(Buffer, 1024, BytesRead, ESocketReceiveFlags::Peek);
			FAnsiStringView ResponseView(reinterpret_cast<const ANSICHAR*>(Buffer), BytesRead);
			int32 LineEndIndex;
			if (ResponseView.FindChar('\r', LineEndIndex) && BytesRead >= LineEndIndex + 2)
			{
				check(ResponseView[LineEndIndex + 1] == '\n');
				InSocket.Recv(Buffer, LineEndIndex + 2, BytesRead, ESocketReceiveFlags::None);
				check(BytesRead == LineEndIndex + 2);
				TotalReadFromSocket += BytesRead;
				return ResponseView.Left(LineEndIndex);
			}
		}
	};

	FAnsiStringView ResponseLine = ReadResponseLine();
	if (ResponseLine == "HTTP/1.1 200 OK")
	{
		bIsOk = true;
	}
	else if (ResponseLine.StartsWith("HTTP/1.1 "))
	{
		ErrorCode = TCString<ANSICHAR>::Atoi64(ResponseLine.GetData() + 9);
	}
	while (!ResponseLine.IsEmpty())
	{
		ResponseLine = ReadResponseLine();
		if (ResponseLine.StartsWith("Content-Length: "))
		{
			ContentLength = FMath::Max(0, TCString<ANSICHAR>::Atoi64(ResponseLine.GetData() + 16));
		}
		else if (ResponseLine.StartsWith("Content-Type: "))
		{
			ContentType = GetMimeType(ResponseLine.RightChop(14));
		}
	}
	if (!bIsOk && ContentLength)
	{
		TArray<uint8> ErrorBuffer;
		ErrorBuffer.SetNumUninitialized(ContentLength + 1);
		uint64 BytesRead = 0;
		InSocket.Recv(ErrorBuffer.GetData(), ContentLength, BytesRead, ESocketReceiveFlags::WaitAll);

		if (BytesRead > 0)
		{
			ErrorBuffer[BytesRead] = '\0';
			ErrorMessage = FString(BytesRead, ANSI_TO_TCHAR(reinterpret_cast<ANSICHAR*>(ErrorBuffer.GetData())));
		}
		else
		{
			ErrorMessage = TEXT("Unknown error");
		}

		ContentLength = 0;
	}
	if (ContentLength == 0)
	{
		ReleaseSocket(true);
	}
}

FStorageServerChunkBatchRequest::FStorageServerChunkBatchRequest(FStorageServerConnection& InOwner, FAnsiStringView Resource, FAnsiStringView Hostname)
	: FStorageServerRequest("POST", Resource, Hostname)
	, Owner(InOwner)
{
	uint32 Magic = 0xAAAA'77AC;
	uint32 ChunkCountPlaceHolder = 0;
	uint32 Reserved1 = 0;
	uint32 Reserved2 = 0;
	*this << Magic;
	ChunkCountOffset = BodyBuffer.Num();
	*this << ChunkCountPlaceHolder << Reserved1 << Reserved2;
}

FStorageServerChunkBatchRequest& FStorageServerChunkBatchRequest::AddChunk(const FIoChunkId& ChunkId, int64 Offset, int64 Size)
{
	uint32* ChunkCount = reinterpret_cast<uint32*>(BodyBuffer.GetData() + ChunkCountOffset);
	*this << const_cast<FIoChunkId&>(ChunkId) << *ChunkCount << Offset << Size;
	++(*ChunkCount);
	return *this;
}

bool FStorageServerChunkBatchRequest::Issue(TFunctionRef<void(uint32 ChunkCount, uint32* ChunkIndices, uint64* ChunkSizes, FStorageServerResponse& ChunkDataStream)> OnResponse)
{
	IStorageConnectionSocket* Socket = Send(Owner);
	if (!Socket)
	{
		UE_LOG(LogStorageServerConnection, Fatal, TEXT("Failed to send chunk batch request to storage server."));
		return false;
	}
	FStorageServerResponse Response(Owner, *Socket);
	if (!Response.IsOk())
	{
		UE_LOG(LogStorageServerConnection, Fatal, TEXT("Failed to read chunk batch from storage server. '%s'"), *Response.GetErrorMessage());
		return false;
	}

	uint32 Magic;
	uint32 ChunkCount;
	uint32 Reserved1;
	uint32 Reserved2;
	Response << Magic;
	if (Magic != 0xbada'b00f)
	{
		UE_LOG(LogStorageServerConnection, Fatal, TEXT("Invalid magic in chunk batch response from storage server."));
		return false;
	}
	Response << ChunkCount;
	if (ChunkCount > INT32_MAX)
	{
		UE_LOG(LogStorageServerConnection, Fatal, TEXT("Invalid chunk count in chunk batch response from storage server."));
		return false;
	}
	Response << Reserved1;
	Response << Reserved2;

	TArray<uint32, TInlineAllocator<64>> ChunkIndices;
	ChunkIndices.Reserve(ChunkCount);
	TArray<uint64, TInlineAllocator<64>> ChunkSizes;
	ChunkSizes.Reserve(ChunkCount);
	for (uint32 Index = 0; Index < ChunkCount; ++Index)
	{
		uint32 ChunkIndex;
		uint32 Flags;
		int64 ChunkSize;
		Response << ChunkIndex;
		Response << Flags;
		Response << ChunkSize;
		ChunkIndices.Add(ChunkIndex);
		ChunkSizes.Emplace(ChunkSize);
	}
	OnResponse(ChunkCount, ChunkIndices.GetData(), ChunkSizes.GetData(), Response);
	Owner.AddTimingInstance(GetDuration(), (double)Response.Tell());
	return true;
}

void FStorageServerResponse::ReleaseSocket(bool bKeepAlive)
{
	Owner.GetConnectionBackend()->ReleaseSocket(Socket, bKeepAlive);
	Socket = nullptr;
}

void FStorageServerResponse::Serialize(void* V, int64 Length)
{
	if (Length == 0)
	{
		return;
	}
	if (!Socket)
	{
		UE_LOG(LogStorageServerConnection, Fatal, TEXT("Trying to read %lld bytes from released socket"), Length);
		return;
	}
	if (Position + Length > ContentLength)
	{
		UE_LOG(LogStorageServerConnection, Fatal, TEXT("Trying to read %lld bytes from socket with only %lld available"), Length, ContentLength - Position);
		return;
	}
	uint64 RemainingBytesToRead = Length;
	uint8* Destination = reinterpret_cast<uint8*>(V);
	while (RemainingBytesToRead)
	{
		uint64 BytesToRead32 = FMath::Min(RemainingBytesToRead, static_cast<uint64>(INT32_MAX));
		uint64 BytesRead;
		if (!Socket->Recv(Destination, static_cast<int32>(BytesToRead32), BytesRead, ESocketReceiveFlags::WaitAll))
		{
			UE_LOG(LogStorageServerConnection, Fatal, TEXT("Failed reading %d bytes from socket"), BytesToRead32);
			return;
		}
		RemainingBytesToRead -= BytesRead;
		Destination += BytesRead;
		Position += BytesRead;
	}

	check(ContentType == EStorageServerContentType::Binary);

	TRACE_COUNTER_ADD(ZenHttpClientSerializedBytes, Length);
	
	if (Position == ContentLength)
	{
		ReleaseSocket(true);
	}
}

int64 FStorageServerResponse::SerializeChunk(FStorageServerSerializationContext& Context, FIoBuffer& OutChunk, void* TargetVa, uint64 RawOffset, uint64 RawSize, bool bHardwareTargetBuffer)
{
	if (ContentLength == 0)
	{
		return 0;
	}

	if (ContentType == EStorageServerContentType::Binary)
	{
		const uint64 ChunkSize = FMath::Min<uint64>(ContentLength, RawSize);
		OutChunk = TargetVa ? FIoBuffer(FIoBuffer::Wrap, TargetVa, ChunkSize) : FIoBuffer(ChunkSize);
		Serialize(OutChunk.Data(), OutChunk.DataSize());
		return ChunkSize;
	}
	else if (ContentType == EStorageServerContentType::CompressedBinary)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ZenClient::SerializeCompressedChunk);

		Context.CompressedBuffer.Reset(ContentLength);
		Serialize(Context.CompressedBuffer.GetData(), ContentLength);

		if (FCompressedBuffer Compressed = FCompressedBuffer::FromCompressed(FSharedBuffer::MakeView(Context.CompressedBuffer.GetData(), ContentLength)))
		{
			FCompressedBufferReaderSourceScope Source(Context.Decoder, Compressed);
			const uint64 ChunkSize = FMath::Min(Compressed.GetRawSize(), RawSize);
			OutChunk = TargetVa ? FIoBuffer(FIoBuffer::Wrap, TargetVa, ChunkSize) : FIoBuffer(ChunkSize);
			const uint64 CompressedOffset = GetCompressedOffset(Compressed, RawOffset);
			if (Context.Decoder.TryDecompressTo(OutChunk.GetMutableView(), CompressedOffset, bHardwareTargetBuffer ? ECompressedBufferDecompressFlags::IntermediateBuffer :ECompressedBufferDecompressFlags::None ))
			{
				return ChunkSize;
			}

			return 0;
		}
	}

	UE_LOG(LogStorageServerConnection, Fatal, TEXT("Received unknown chunk type from storage server"));
	return 0;
}

int64 FStorageServerResponse::SerializeChunkTo(FMutableMemoryView Memory, uint64 RawOffset)
{
	if (ContentLength == 0)
	{
		return 0;
	}

	if (ContentType == EStorageServerContentType::Binary)
	{
		FMutableMemoryView Dst = Memory.Left(FMath::Min<uint64>(Memory.GetSize(), ContentLength));
		Serialize(Dst.GetData(), Dst.GetSize());
		return Dst.GetSize();
	}
	else if (ContentType == EStorageServerContentType::CompressedBinary)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ZenClient::SerializeCompressedChunk);
		
		TArray64<uint8> CompressedBuffer;
		CompressedBuffer.Reset(ContentLength);
		Serialize(CompressedBuffer.GetData(), ContentLength);
		
		if (FCompressedBuffer Compressed = FCompressedBuffer::FromCompressed(FSharedBuffer::MakeView(CompressedBuffer.GetData(), ContentLength)))
		{
			const uint64 CompressedOffset = GetCompressedOffset(Compressed, RawOffset);
			FMutableMemoryView Dst = Memory.Left(FMath::Min(Memory.GetSize(), Compressed.GetRawSize() - CompressedOffset));
			if (FCompressedBufferReader(Compressed).TryDecompressTo(Dst, CompressedOffset))
			{
				return Dst.GetSize();
			}

			return 0;
		}
	}
	
	UE_LOG(LogStorageServerConnection, Fatal, TEXT("Received unknown chunk type from storage server"));
	return 0;
}

FCbObject FStorageServerResponse::GetResponseObject()
{
	FCbField Payload;

	if (ContentType == EStorageServerContentType::CompressedBinary)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GetCompressedResponseObject);

		TArray64<uint8> CompressedBuffer;
		CompressedBuffer.Reset(ContentLength);
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SerializeCompressedData);
			Serialize(CompressedBuffer.GetData(), ContentLength);
		}

		if (FCompressedBuffer Compressed = FCompressedBuffer::FromCompressed(FSharedBuffer::MakeView(CompressedBuffer.GetData(), ContentLength)))
		{
			FIoBuffer Decompressed(Compressed.GetRawSize());
			if (FCompressedBufferReader(Compressed).TryDecompressTo(Decompressed.GetMutableView(), 0))
			{
				FBufferReader DecompressedAr(Decompressed.GetData(), Decompressed.GetSize(), false);
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(LoadingUncompressedCompactBinary);
					Payload = LoadCompactBinary(DecompressedAr);
				}
			}
		}
	}
	else if (ContentType == EStorageServerContentType::CbObject)
	{
		Payload = LoadCompactBinary(*this);
	}
	
	return Payload.AsObject();
}

FStorageConnectionBackend::FStorageConnectionBackend(FStorageServerConnection& InOwner)
	: Owner(InOwner)
{
}

bool FStorageConnectionBackend::Initialize(TArrayView<const FString> InHostAddresses, int32 InPort, const TCHAR* InProjectNameOverride, const TCHAR* InPlatformNameOverride)
{
	InitOplog(InProjectNameOverride, InPlatformNameOverride);

	return InitializeInternal(InHostAddresses, InPort);
}

void FStorageConnectionBackend::InitOplog(const TCHAR* InProjectNameOverride, const TCHAR* InPlatformNameOverride)
{
	OplogPath.Append("/prj/");
	if (InProjectNameOverride)
	{
		OplogPath.Append(TCHAR_TO_ANSI(InProjectNameOverride));
	}
	else
	{
		OplogPath.Append(TCHAR_TO_ANSI(*FApp::GetZenStoreProjectId()));
	}
	OplogPath.Append("/oplog/");
	if (InPlatformNameOverride)
	{
		OplogPath.Append(TCHAR_TO_ANSI(InPlatformNameOverride));
	}
	else
	{
		TArray<FString> TargetPlatformNames;
		FPlatformMisc::GetValidTargetPlatforms(TargetPlatformNames);
		check(TargetPlatformNames.Num() > 0);
		OplogPath.Append(TCHAR_TO_ANSI(*TargetPlatformNames[0]));
	}
}

FStorageServerConnection::FStorageServerConnection()
	: SocketSubsystem(*ISocketSubsystem::Get())
{
}

FStorageServerConnection::~FStorageServerConnection()
{
	if (StatsObject)
	{
		StatsObject->StopDrawing();
		StatsObject->RemoveFromRoot();
	}
}

bool FStorageServerConnection::Initialize(TArrayView<const FString> InHostAddresses, int32 InPort, const TCHAR* InProjectNameOverride, const TCHAR* InPlatformNameOverride)
{
	OplogPath.Append("/prj/");
	if (InProjectNameOverride)
	{
		OplogPath.Append(TCHAR_TO_ANSI(InProjectNameOverride));
	}
	else
	{
		OplogPath.Append(TCHAR_TO_ANSI(*FApp::GetZenStoreProjectId()));
	}
	OplogPath.Append("/oplog/");
	if (InPlatformNameOverride)
	{
		OplogPath.Append(TCHAR_TO_ANSI(InPlatformNameOverride));
	}
	else
	{
		TArray<FString> TargetPlatformNames;
		FPlatformMisc::GetValidTargetPlatforms(TargetPlatformNames);
		check(TargetPlatformNames.Num() > 0);
		OplogPath.Append(TCHAR_TO_ANSI(*TargetPlatformNames[0]));
	}

	if (!CreateConnectionBackend(InHostAddresses, InPort))
	{
		return false;
	}
	
	return ConnectionBackend->Initialize(InHostAddresses, InPort, InProjectNameOverride, InPlatformNameOverride);
}

void FStorageServerConnection::PackageStoreRequest(TFunctionRef<void(FPackageStoreEntryResource&&)> Callback)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(StorageServerPackageStoreRequest);

	TAnsiStringBuilder<256> ResourceBuilder;
	ResourceBuilder.Append(OplogPath).Append("/entries?fieldfilter=packagestoreentry");
	FStorageServerRequest Request("GET", *ResourceBuilder, Hostname, EStorageServerContentType::CompressedBinary);
	IStorageConnectionSocket* Socket = Request.Send(*this);
	if (!Socket)
	{
		UE_LOG(LogStorageServerConnection, Fatal, TEXT("Failed to send oplog request to storage server at %s."), *ServerAddr->ToString(true));
		return;
	}
	FStorageServerResponse Response(*this, *Socket);
	if (Response.IsOk())
	{
		FCbObject ResponseObj = Response.GetResponseObject();

		TRACE_CPUPROFILER_EVENT_SCOPE(StorageServerPackageStoreRequestParseEntries);
		for (FCbField& OplogEntry : ResponseObj["entries"].AsArray())
		{
			FCbObject OplogObj = OplogEntry.AsObject();
			FPackageStoreEntryResource Entry = FPackageStoreEntryResource::FromCbObject(OplogObj["packagestoreentry"].AsObject());
			Callback(MoveTemp(Entry));
		}
	}
	else
	{
		UE_LOG(LogStorageServerConnection, Fatal, TEXT("Failed to read oplog from storage server at %s. '%s'"), *ServerAddr->ToString(true), *Response.GetErrorMessage());
	}
}

void FStorageServerConnection::FileManifestRequest(TFunctionRef<void(FIoChunkId Id, FStringView Path, int64 RawSize)> Callback)
{
	TAnsiStringBuilder<256> ResourceBuilder;
	ResourceBuilder.Append(OplogPath).Append("/files?fieldnames=id,clientpath,rawsize");
	FStorageServerRequest Request("GET", *ResourceBuilder, Hostname, EStorageServerContentType::CbObject);
	IStorageConnectionSocket* Socket = Request.Send(*this);
	if (!Socket)
	{
		UE_LOG(LogStorageServerConnection, Fatal, TEXT("Failed to send file manifest request to storage server at %s."), *ServerAddr->ToString(true));
		return;
	}

	FStorageServerResponse Response(*this, *Socket);
	if (Response.IsOk())
	{
		FCbObject ResponseObj = Response.GetResponseObject();
		
		for (FCbField& FileArrayEntry : ResponseObj["files"].AsArray())
		{
			FCbObject Entry = FileArrayEntry.AsObject();
			FCbObjectId Id = Entry["id"].AsObjectId();
			int64 ResponseRawSize = Entry["rawsize"].AsInt64();

			TStringBuilder<128> WidePath;
			WidePath.Append(FUTF8ToTCHAR(Entry["clientpath"].AsString()));

			FIoChunkId ChunkId;
			ChunkId.Set(Id.GetView());


			Callback(ChunkId, WidePath, ResponseRawSize);
		}
	}
	else
	{
		UE_LOG(LogStorageServerConnection, Fatal, TEXT("Failed to read file manifest from storage server at %s. '%s'"), *ServerAddr->ToString(true), *Response.GetErrorMessage());
	}
}

int64 FStorageServerConnection::ChunkSizeRequest(const FIoChunkId& ChunkId)
{
	TAnsiStringBuilder<256> ResourceBuilder;
	ResourceBuilder.Append(OplogPath);
	ResourceBuilder << "/" << ChunkId << "/info";

	FStorageServerRequest Request("GET", *ResourceBuilder, Hostname, EStorageServerContentType::CbObject);
	IStorageConnectionSocket* Socket = Request.Send(*this);
	if (!Socket)
	{
		UE_LOG(LogStorageServerConnection, Fatal, TEXT("Failed to send chunk size request to storage server at %s."), *ServerAddr->ToString(true));
		return -1;
	}
	FStorageServerResponse Response(*this, *Socket);
	if (Response.IsOk())
	{
		AddTimingInstance(Request.GetDuration(), (double)Response.TotalSize());

		FCbObject ResponseObj = Response.GetResponseObject();

		const int64 ChunkSize = ResponseObj["size"].AsInt64(0);

		return ChunkSize;
	}
	else if (Response.GetErrorCode() == 404)
	{
		return -1;
	}
	else
	{
		UE_LOG(LogStorageServerConnection, Fatal, TEXT("Failed to get chunk size from storage server at %s. '%s'"), *ServerAddr->ToString(true), *Response.GetErrorMessage());
	}
	return -1;
}

bool FStorageServerConnection::ReadChunkRequest(const FIoChunkId& ChunkId, uint64 Offset, uint64 Size, TFunctionRef<void(FStorageServerResponse&)> OnResponse)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ZenHttpClient::ReadChunkRequest);
	TAnsiStringBuilder<256> ResourceBuilder;
	ResourceBuilder.Append(OplogPath) << "/" << ChunkId;

	bool HaveQuery = false;

	auto AppendQueryDelimiter = [&] 
	{
		if (HaveQuery)
		{
			ResourceBuilder.Append(ANSITEXTVIEW("&"));
		}
		else
		{
			ResourceBuilder.Append(ANSITEXTVIEW("?"));
			HaveQuery = true;
		}
	};

	if (Offset)
	{
		AppendQueryDelimiter();
		ResourceBuilder.Appendf("offset=%" UINT64_FMT, Offset);
	}

	if (Size != ~uint64(0))
	{
		AppendQueryDelimiter();
		ResourceBuilder.Appendf("size=%" UINT64_FMT, Size);
	}

	FStorageServerRequest Request("GET", *ResourceBuilder, Hostname, EStorageServerContentType::CompressedBinary);
	IStorageConnectionSocket* Socket = Request.Send(*this);
	if (!Socket)
	{
		UE_LOG(LogStorageServerConnection, Fatal, TEXT("Failed to send chunk read request to storage server at %s."), *ServerAddr->ToString(true));
		return false;
	}
	FStorageServerResponse Response(*this, *Socket);
	if (Response.IsOk())
	{
		OnResponse(Response);
		AddTimingInstance(Request.GetDuration(), (double)Response.Tell());
		return true;
	}
	else if (Response.GetErrorCode() == 404)
	{
		return false;
	}
	else
	{
		UE_LOG(LogStorageServerConnection, Fatal, TEXT("Failed to read chunk from storage server at %s. '%s'"), *ServerAddr->ToString(true), *Response.GetErrorMessage());
		return false;
	}
}

FStorageServerChunkBatchRequest FStorageServerConnection::NewChunkBatchRequest()
{
	TAnsiStringBuilder<256> ResourceBuilder;
	ResourceBuilder.Append(OplogPath).Append("/batch");
	return FStorageServerChunkBatchRequest(*this, *ResourceBuilder, Hostname);
}

FString FStorageServerConnection::GetHostAddr() const
{
	return ServerAddr.IsValid() ? ServerAddr->ToString(false) : FString();
}

bool FStorageServerConnection::CreateConnectionBackend(TArrayView<const FString> HostAddresses, int32 Port)
{
	FString PlatformAddress;
	for (const FString& Addr : HostAddresses)
	{
		if (Addr.Contains(TEXT("platform://")))
		{
			PlatformAddress = Addr;
		}
	}

	if (!PlatformAddress.IsEmpty())
	{
		ConnectionBackend = MakeUnique<FStorageServerPlatformConnectionBackend>(*this);
	}
	else
	{
		ConnectionBackend = MakeUnique<FStorageSocketConnectionBackend>(*this, *ISocketSubsystem::Get());
	}

	return ConnectionBackend != nullptr;
}

bool FStorageServerConnection::CreatePlatformBackend(const FString& HostAddresses, int32 Port)
{
	ConnectionBackend = MakeUnique<FStorageServerPlatformConnectionBackend>(*this);
	if (!ConnectionBackend)
	{
		return false;
	}

	return true;
}

FStorageConnectionBackend* FStorageServerConnection::GetConnectionBackend() const
{
	return ConnectionBackend.Get();
}

void FStorageServerConnection::AddTimingInstance(double duration, uint64 bytes)
{
	if ((StatsObject == nullptr) && (UObjectInitialized()))
	{
		StatsObject = NewObject<UDebugStorageServerConnection>();
		StatsObject->SetOwner(this);
		StatsObject->AddToRoot();
		StatsObject->StartDrawing();
	}

	if (StatsObject)
		StatsObject->AddTimingInstance(duration, bytes);
}


#endif
