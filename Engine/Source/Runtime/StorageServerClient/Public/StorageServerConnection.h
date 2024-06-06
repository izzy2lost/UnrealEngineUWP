// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Compression/CompressedBuffer.h"
#include "Containers/UnrealString.h"
#include "Misc/StringBuilder.h"
#include "Memory/MemoryFwd.h"
#include "Templates/SharedPointer.h"
#include "SocketTypes.h"
#include "HAL/PlatformTime.h"

#if !UE_BUILD_SHIPPING

class FIoBuffer;
class FIoChunkId;
class FInternetAddr;
class FSocket;
class FStorageServerChunkBatchRequest;
class FStorageServerConnection;
class FStorageConnectionBackend;
class FStorageSocketConnectionBackend;
class FStorageServerPlatformConnectionBackend;
class IStorageConnectionSocket;
class ISocketSubsystem;
struct FPackageStoreEntryResource;

enum class EStorageServerContentType : uint8
{
	Unknown = 0,
	CbObject,
	Binary,
	CompressedBinary,
};

inline FAnsiStringView GetMimeTypeString(EStorageServerContentType ContentType)
{
	switch (ContentType)
	{
		case EStorageServerContentType::CbObject:
			return ANSITEXTVIEW("application/ue-x-cb");
		case EStorageServerContentType::Binary:
			return ANSITEXTVIEW("application/octet-stream");
		case EStorageServerContentType::CompressedBinary:
			return ANSITEXTVIEW("application/x-ue-comp");
		default:
			return ANSITEXTVIEW("unknown");
	};
};

inline EStorageServerContentType GetMimeType(const FAnsiStringView& ContentType)
{
	if (ContentType == ANSITEXTVIEW("application/octet-stream"))
	{
		return EStorageServerContentType::Binary;
	}
	else if (ContentType == ANSITEXTVIEW("application/x-ue-comp"))
	{
		return EStorageServerContentType::CompressedBinary;
	}
	else if (ContentType == ANSITEXTVIEW("application/x-ue-cb"))
	{
		return EStorageServerContentType::CbObject;
	}
	else
	{
		return EStorageServerContentType::Unknown;
	}
};

struct FStorageServerSerializationContext
{
	TArray64<uint8> CompressedBuffer;
	FCompressedBufferReader Decoder;
};

class FStorageServerRequest
	: public FArchive
{
	EStorageServerContentType AcceptContentType() const;
protected:
	friend FStorageServerConnection;
	friend FStorageSocketConnectionBackend;
	friend FStorageServerPlatformConnectionBackend;

	FStorageServerRequest(
		FAnsiStringView Verb,
		FAnsiStringView Resource,
		FAnsiStringView Hostname,
		EStorageServerContentType Accept = EStorageServerContentType::Binary);

	IStorageConnectionSocket* Send(FStorageServerConnection& Owner, bool bLogOnError = true);
	virtual void Serialize(void* V, int64 Length) override;

	EStorageServerContentType AcceptType;
	TAnsiStringBuilder<512> HeaderBuffer;
	TArray<uint8, TInlineAllocator<1024>> BodyBuffer;
	double StartRequestWallTime;

public:
	void StartTiming()
	{
		StartRequestWallTime = FPlatformTime::Seconds();
	}

	double GetDuration() // seconds
	{
		check(StartRequestWallTime > 0.0);
		double EndRequestWallTime = FPlatformTime::Seconds();
		return EndRequestWallTime - StartRequestWallTime;
	}

};

class FStorageServerResponse
	: public FArchive
{
public:
	~FStorageServerResponse()
	{
		if (Socket)
		{
			ReleaseSocket(false);
		}
	}

	bool IsOk() const
	{
		return bIsOk;
	}

	const int32 GetErrorCode() const
	{
		return ErrorCode;
	}

	const FString& GetErrorMessage() const
	{
		return ErrorMessage;
	}

	int64 TotalSize() override
	{
		return ContentLength;
	}

	int64 Tell() override
	{
		return Position;
	}

	void Serialize(void* V, int64 Length) override;

	int64 SerializeChunk(FStorageServerSerializationContext& Context, FIoBuffer& OutChunk, void* TargetVa = nullptr, uint64 RawOffset = 0, uint64 RawSize = MAX_uint64, bool  bHardwareTargetBuffer = false);
	
	inline int64 SerializeChunk(FIoBuffer& OutChunk, void* TargetVa = nullptr, uint64 RawOffset = 0, uint64 RawSize = MAX_uint64, bool bHardwareTargetBuffer = false)
	{
		FStorageServerSerializationContext SerializationContext;
		return SerializeChunk(SerializationContext, OutChunk, TargetVa, RawOffset, RawSize, bHardwareTargetBuffer);
	}

	int64 SerializeChunkTo(FMutableMemoryView Memory, uint64 RawOffset = 0);

	FCbObject GetResponseObject();

private:
	friend FStorageServerConnection;
	friend FStorageSocketConnectionBackend;
	friend FStorageServerPlatformConnectionBackend;
	friend FStorageServerChunkBatchRequest;

	FStorageServerResponse(FStorageServerConnection& Owner, IStorageConnectionSocket& Socket);
	void ReleaseSocket(bool bKeepAlive);

	FStorageServerConnection& Owner;
	IStorageConnectionSocket* Socket = nullptr;
	int64 ContentLength = 0;
	int64 Position = 0;
	int32 ErrorCode;
	FString ErrorMessage;
	bool bIsOk = false;
	EStorageServerContentType ContentType = EStorageServerContentType::Unknown;
};

class FStorageServerChunkBatchRequest
	: private FStorageServerRequest
{
public:
	STORAGESERVERCLIENT_API FStorageServerChunkBatchRequest& AddChunk(const FIoChunkId& ChunkId, int64 Offset, int64 Size);
	STORAGESERVERCLIENT_API bool Issue(TFunctionRef<void(uint32 ChunkCount, uint32* ChunkIndices, uint64* ChunkSizes, FStorageServerResponse& ChunkDataStream)> OnResponse);

private:
	friend FStorageServerConnection;

	FStorageServerChunkBatchRequest(FStorageServerConnection& Owner, FAnsiStringView Resource, FAnsiStringView Hostname);

	FStorageServerConnection& Owner;
	int32 ChunkCountOffset = 0;
};

class IStorageConnectionSocket
{
public:
	IStorageConnectionSocket() = default;
	IStorageConnectionSocket(const IStorageConnectionSocket&) = delete;
	virtual ~IStorageConnectionSocket() = default;

	virtual bool Send(const uint8* Data, const uint64 DataSize)                                                      = 0;
	virtual bool Recv(uint8* Data, const uint64 DataSize, uint64& BytesRead, ESocketReceiveFlags::Type ReceiveFlags) = 0;
	virtual bool HasPendingData(uint64& PendingDataSize) const                                                       = 0;
	virtual void Close()                                                                                             = 0;
};

class FStorageConnectionBackend
{
public:
	FStorageConnectionBackend(FStorageServerConnection& InOwner);
	virtual ~FStorageConnectionBackend() = default;

	bool Initialize(TArrayView<const FString> InHostAddresses, int32 InPort, const TCHAR* InProjectNameOverride, const TCHAR* InPlatformNameOverride);

	virtual IStorageConnectionSocket* AcquireSocketFromPool() = 0;
	virtual IStorageConnectionSocket* AcquireNewSocket(float TimeoutSeconds = -1.f) = 0;
	virtual void ReleaseSocket(IStorageConnectionSocket* Socket, bool bKeepAlive) = 0;
	virtual FString GetHostName() = 0;
	
protected:
	void InitOplog(const TCHAR* InProjectNameOverride, const TCHAR* InPlatformNameOverride);
	virtual bool InitializeInternal(TArrayView<const FString> InHostAddresses, int32 Port) = 0;

protected:
	FStorageServerConnection& Owner;
	TAnsiStringBuilder<1024> OplogPath;
};

class UDebugStorageServerConnection;

class FStorageServerConnection
{
public:
	STORAGESERVERCLIENT_API FStorageServerConnection();
	STORAGESERVERCLIENT_API ~FStorageServerConnection();

	STORAGESERVERCLIENT_API bool Initialize(TArrayView<const FString> HostAddresses, int32 Port, const TCHAR* ProjectNameOverride = nullptr, const TCHAR* PlatformNameOverride = nullptr);

	STORAGESERVERCLIENT_API void PackageStoreRequest(TFunctionRef<void(FPackageStoreEntryResource&&)> Callback);
	STORAGESERVERCLIENT_API void FileManifestRequest(TFunctionRef<void(FIoChunkId Id, FStringView Path, int64 RawSize)> Callback);
	STORAGESERVERCLIENT_API int64 ChunkSizeRequest(const FIoChunkId& ChunkId);
	STORAGESERVERCLIENT_API bool ReadChunkRequest(const FIoChunkId& ChunkId, uint64 Offset, uint64 Size, TFunctionRef<void(FStorageServerResponse&)> OnResponse);
	STORAGESERVERCLIENT_API FStorageServerChunkBatchRequest NewChunkBatchRequest();

	STORAGESERVERCLIENT_API FString GetHostAddr() const;

private:
	friend FStorageServerRequest;
	friend FStorageServerResponse;
	friend FStorageServerChunkBatchRequest;
	friend UDebugStorageServerConnection;

	bool CreateConnectionBackend(TArrayView<const FString> HostAddresses, int32 Port);
	bool CreatePlatformBackend(const FString& HostAddresses, int32 Port);

	FStorageConnectionBackend* GetConnectionBackend() const;
	void AddTimingInstance(double duration, uint64 bytes);

	ISocketSubsystem& SocketSubsystem;
	TAnsiStringBuilder<1024> OplogPath;
	TSharedPtr<FInternetAddr> ServerAddr;
	TAnsiStringBuilder<1024> Hostname;
	TUniquePtr<FStorageConnectionBackend> ConnectionBackend;
	FCriticalSection BackendCS;

	UDebugStorageServerConnection* StatsObject = nullptr;
};

#endif
