// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Compression/CompressedBuffer.h"
#include "Containers/UnrealString.h"
#include "Containers/AnsiString.h"
#include "Misc/StringBuilder.h"
#include "Memory/MemoryFwd.h"
#include "Templates/SharedPointer.h"
#include "SocketTypes.h"
#include "HAL/PlatformTime.h"
#include "StorageServerHttpClient.h"
#include "DebugStorageServerConnection.h"
#include "IO/IoChunkId.h"

#if !UE_BUILD_SHIPPING

DECLARE_LOG_CATEGORY_EXTERN(LogStorageServerConnection, Log, All);

struct FPackageStoreEntryResource;

class FStorageServerConnection
{
public:
	FStorageServerConnection() = default;
	~FStorageServerConnection();

	bool Initialize(TArrayView<const FString> HostAddresses, const int32 Port, const FAnsiStringView& InBaseURI);

	void PackageStoreRequest(TFunctionRef<void(FPackageStoreEntryResource&&)> Callback);
	void FileManifestRequest(TFunctionRef<void(FIoChunkId Id, FStringView Path, int64 RawSize)> Callback);
	int64 ChunkSizeRequest(const FIoChunkId& ChunkId);
	TIoStatusOr<FIoBuffer> ReadChunkRequest(
		const FIoChunkId& ChunkId,
		const uint64 Offset,
		const uint64 Size,
		const TOptional<FIoBuffer> OptDestination,
		const bool bHardwareTargetBuffer
	);
	void ReadChunkRequestAsync(
		const FIoChunkId& ChunkId,
		const uint64 Offset,
		const uint64 Size,
		const TOptional<FIoBuffer> OptDestination,
		const bool bHardwareTargetBuffer,
		TFunctionRef<void(TIoStatusOr<FIoBuffer> Data)> OnResponse
	);

	FString GetHostAddr() const
	{
		return CurrentHostAddr;
	}

private:
	TUniquePtr<IStorageServerHttpClient> HttpClient;
	FAnsiString BaseURI;
	FString CurrentHostAddr;
	UDebugStorageServerConnection* StatsObject = nullptr;

	TArray<FString> SortHostAddressesByLocalSubnet(TArrayView<const FString> HostAddresses, const int32 Port);
	static bool IsPlatformSocketAddress(const FString Address); 
	TUniquePtr<IStorageServerHttpClient> CreateHttpClient(const FString Address, const int32 Port);
	TSharedPtr<FInternetAddr> StringToInternetAddr(const FString Address, const int32 Port);
	bool HandshakeRequest();
	void BuildReadChunkRequestUrl(FAnsiStringBuilderBase& Builder, const FIoChunkId& ChunkId, const uint64 Offset, const uint64 Size);
	static TIoStatusOr<FIoBuffer> ReadChunkRequestProcessHttpResult(
		IStorageServerHttpClient::FResult ResultTuple,
		const uint64 Offset,
		const uint64 Size,
		const TOptional<FIoBuffer> OptDestination,
		const bool bHardwareTargetBuffer
	);
	static uint64 GetCompressedOffset(const FCompressedBuffer& Buffer, uint64 RawOffset);
	void AddTimingInstance(const double Duration, const uint64 Bytes);
};

#endif
