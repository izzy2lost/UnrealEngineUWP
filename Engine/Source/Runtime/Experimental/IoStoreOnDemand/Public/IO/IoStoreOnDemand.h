// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "IO/IoChunkId.h"
#include "IO/IoHash.h"
#include "IO/IoStatus.h"
#include "Misc/Guid.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

#if (IS_PROGRAM || WITH_EDITOR)
#include "Containers/Map.h"
#include "Misc/AES.h"
#endif // (IS_PROGRAM || WITH_EDITOR)

#define UE_API IOSTOREONDEMAND_API

class FArchive;
class FCbFieldView;
class FCbWriter;
class IIoStoreWriter;
struct FAnalyticsEventAttribute;
struct FIoContainerSettings;
struct FIoStoreWriterSettings;
namespace UE::IO::IAS { struct FOnDemandEndpoint; }

UE_API DECLARE_LOG_CATEGORY_EXTERN(LogIas, VeryVerbose, All);

namespace UE::IO::IAS
{

////////////////////////////////////////////////////////////////////////////////

bool TryParseConfigFile(const FString& ConfigPath, FOnDemandEndpoint& OutEndpoint);

////////////////////////////////////////////////////////////////////////////////
enum class EOnDemandTocVersion : uint32
{
	Invalid			= 0,
	Initial			= 1,
	UTocHash		= 2,
	BlockHash32		= 3,
	NoRawHash		= 4,
	Meta			= 5,

	LatestPlusOne,
	Latest			= (LatestPlusOne - 1)
};

enum class EOnDemandChunkVersion : uint32
{
	Invalid			= 0,
	Initial			= 1,

	LatestPlusOne,
	Latest			= (LatestPlusOne - 1)
};

struct FTocMeta
{
	int64 EpochTimestamp = 0;
	FString BuildVersion;
	FString TargetPlatform;

	UE_API friend FArchive& operator<<(FArchive& Ar, FTocMeta& Meta);
	UE_API friend FCbWriter& operator<<(FCbWriter& Writer, const FTocMeta& Meta);
};

UE_API bool LoadFromCompactBinary(FCbFieldView Field, FTocMeta& OutMeta);

struct FOnDemandTocHeader
{
	static constexpr uint64 ExpectedMagic = 0x6f6e64656d616e64; // ondemand

	uint64 Magic = ExpectedMagic;
	uint32 Version = uint32(EOnDemandTocVersion::Latest);
	uint32 ChunkVersion = uint32(EOnDemandChunkVersion::Latest);
	uint32 BlockSize = 0;
	FString CompressionFormat;
	FString ChunksDirectory;
	
	UE_API friend FArchive& operator<<(FArchive& Ar, FOnDemandTocHeader& Header);
	UE_API friend FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandTocHeader& Header);
};

UE_API bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandTocHeader& OutTocHeader);

struct FOnDemandTocEntry
{
	FIoHash Hash = FIoHash::Zero;
	FIoChunkId ChunkId = FIoChunkId::InvalidChunkId;
	uint64 RawSize = 0;
	uint64 EncodedSize = 0;
	uint32 BlockOffset = ~uint32(0);
	uint32 BlockCount = 0; 
	
	UE_API friend FArchive& operator<<(FArchive& Ar, FOnDemandTocEntry& Entry);
	UE_API friend FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandTocEntry& Entry);
};

UE_API bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandTocEntry& OutTocEntry);

struct FOnDemandTocContainerEntry
{
	FString ContainerName;
	FString EncryptionKeyGuid;
	TArray<FOnDemandTocEntry> Entries;
	TArray<uint32> BlockSizes;
	TArray<uint32> BlockHashes;
	FIoHash UTocHash;

	UE_API friend FArchive& operator<<(FArchive& Ar, FOnDemandTocContainerEntry& ContainerEntry);
	UE_API friend FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandTocContainerEntry& ContainerEntry);
};

UE_API bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandTocContainerEntry& OutContainer);

struct FOnDemandToc
{
	FOnDemandTocHeader Header;
	FTocMeta Meta;
	TArray<FOnDemandTocContainerEntry> Containers;

	UE_API friend FArchive& operator<<(FArchive& Ar, FOnDemandToc& Toc);
	UE_API friend FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandToc& Toc);

	static FGuid VersionGuid;
};

UE_API bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandToc& OutToc);

TIoStatusOr<FOnDemandToc> LoadTocFromUrl(const FString& ServiceURL, const FString& TocPath, int32 RetryCount);

#if (IS_PROGRAM || WITH_EDITOR)

////////////////////////////////////////////////////////////////////////////////
struct FIoStoreUploadParams
{
	FString ServiceUrl;
	FString Bucket;
	FString BucketPrefix;
	FString Region; 
	FString AccessKey;
	FString SecretKey;
	FString SessionToken;
	FString CredentialsFile;
	FString CredentialsFileKeyName;
	FString BuildVersion;
	FString TargetPlatform;
	int32 MaxConcurrentUploads = 16;
	bool bDeleteContainerFiles = true;
	bool bDeletePakFiles = true;
	
	UE_API static TIoStatusOr<FIoStoreUploadParams> Parse(const TCHAR* CommandLine);
};

struct FIoStoreUploadResult
{
	FIoHash TocHash;
	FString TocPath;
	uint64 TocSize = 0;
};

UE_API TIoStatusOr<FIoStoreUploadResult> UploadContainerFiles(
	const FIoStoreUploadParams& UploadParams,
	TConstArrayView<FString> ContainerFiles,
	const TMap<FGuid, FAES::FAESKey>& EncryptionKeys);

////////////////////////////////////////////////////////////////////////////////
struct FIoStoreDownloadParams
{
	FString Directory;
	FString ServiceUrl;
	FString Bucket;
	FString Region; 
	FString AccessKey;
	FString SecretKey;
	FString SessionToken;
	FString CredentialsFile;
	FString CredentialsFileKeyName;
	int32 MaxConcurrentDownloads = 16;
	
	UE_API static TIoStatusOr<FIoStoreDownloadParams> Parse(const TCHAR* CommandLine);
};

UE_API FIoStatus DownloadContainerFiles(const FIoStoreDownloadParams& DownloadParams, const FString& TocPath);

UE_API FIoStatus PrimeEndPoint(FStringView IoStoreOnDemandIniPath);
#endif // (IS_PROGRAM || WITH_EDITOR)

class IOnDemandIoDispatcherBackend;

class FIoStoreOnDemandModule
	: public IModuleInterface
{
private:
	TSharedPtr<IOnDemandIoDispatcherBackend> Backend;

public:
	UE_API void SetBulkOptionalEnabled(bool bInEnabled);
	UE_API void SetEnabled(bool bInEnabled);

	UE_API void ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const;

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

} // namespace UE::IO::IAS

#undef UE_API
