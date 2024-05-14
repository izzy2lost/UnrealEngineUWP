// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "IO/IoChunkId.h"
#include "IO/IoContainerId.h"
#include "IO/IoHash.h"
#include "IO/IoStatus.h"
#include "Misc/EnumClassFlags.h"
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
struct FKeyChain;
struct FAnalyticsEventAttribute;
struct FIoContainerSettings;
struct FIoStoreWriterSettings;
namespace UE::IoStore { struct FOnDemandEndpoint; }
namespace UE::IoStore { class FOnDemandIoStore; }
using FIoBlockHash = uint32;

// Custom initialization allows users to control when
// the system should be initialized.
#if !defined(UE_IAS_CUSTOM_INITIALIZATION)
	#define UE_IAS_CUSTOM_INITIALIZATION 0
#endif

UE_API DECLARE_LOG_CATEGORY_EXTERN(LogIoStoreOnDemand, Log, All);
UE_API DECLARE_LOG_CATEGORY_EXTERN(LogIas, Log, All);

namespace UE::IoStore
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
	ContainerId		= 6,
	AdditionalFiles	= 7,

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
	FIoContainerId ContainerId;
	FString ContainerName;
	FString EncryptionKeyGuid;
	TArray<FOnDemandTocEntry> Entries;
	TArray<uint32> BlockSizes;
	TArray<FIoBlockHash> BlockHashes;

	/** Hash of the .utoc file (on disk) used to generate this data */
	FIoHash UTocHash;

	UE_API friend FArchive& operator<<(FArchive& Ar, FOnDemandTocContainerEntry& ContainerEntry);
	UE_API friend FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandTocContainerEntry& ContainerEntry);
};

UE_API bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandTocContainerEntry& OutContainer);

struct FOnDemandTocSentinel
{
public:
	static constexpr inline char SentinelImg[] = "-[]--[]--[]--[]-";
	static constexpr uint32 SentinelSize = 16;

	bool IsValid();

	UE_API friend FArchive& operator<<(FArchive& Ar, FOnDemandTocSentinel& Sentinel);

private:
	uint8 Data[SentinelSize] = { 0 };
};

struct FOnDemandTocAdditionalFile
{
	FIoHash Hash;
	FString Filename;
	uint64 FileSize = 0;

	UE_API friend FArchive& operator<<(FArchive& Ar, FOnDemandTocAdditionalFile& AdditionalFile);
	UE_API friend FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandTocAdditionalFile& AdditionalFile);
};

UE_API bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandTocAdditionalFile& AdditionalFile);

struct FOnDemandToc
{
	FOnDemandToc() = default;
	~FOnDemandToc() = default;

	FOnDemandToc(FOnDemandToc&&) = default;
	FOnDemandToc& operator= (FOnDemandToc&&) = default;

	// Copying this structure would be quite expensive so we want to make sure that it doesn't happen.

	FOnDemandToc(const FOnDemandToc&) = delete;
	FOnDemandToc&  operator= (const FOnDemandToc&) = delete;

	FOnDemandTocHeader Header;
	FTocMeta Meta;
	TArray<FOnDemandTocContainerEntry> Containers;
	TArray<FOnDemandTocAdditionalFile> AdditionalFiles;

	UE_API friend FArchive& operator<<(FArchive& Ar, FOnDemandToc& Toc);
	UE_API friend FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandToc& Toc);

	static FGuid VersionGuid;

	static TIoStatusOr<FOnDemandToc> LoadFromFile(const FString& FilePath, bool bValidate);
	static TIoStatusOr<FOnDemandToc> LoadFromUrl(FAnsiStringView Url, uint32 RetryCount = 0, bool bFollowRedirects = false);
	static TIoStatusOr<FOnDemandToc> LoadFromUrl(FStringView Url, uint32 RetryCount = 0, bool bFollowRedirects = false);
};

UE_API bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandToc& OutToc);

#if (IS_PROGRAM || WITH_EDITOR)

////////////////////////////////////////////////////////////////////////////////
/**
 * Parameters for listing uploaded TOC file(s) from an S3 compatible endpoint.
 *
 * Example usage:
 *
 * 1) Print available TOC's from a local server to standard out.
 * UnrealPak.exe -ListTocs -ServiceUrl="http://10.24.101.92:9000" -Bucket=<bucketname> -BucketPrefix=<some/path/to/data> -AccessKey=<accesskey> -SecretKey=<secretkey>
 *
 * 2) Print available TOC's from AWS S3.
 * UnrealPak.exe -Region="us-east-1" -BucketPath=<mybucket/some/data/path/> -CredentialsFile=<path/to/credentials.txt> -BuildVersion=<version> -Json=<path/to/file.json>
 * 
 * 3) Serialize all TOC's matching a specific build version to JSON:
 * UnrealPak.exe -Region="us-east-1" -BucketPath=<mybucket/some/data/path/> -CredentialsFile=<path/to/credentials.txt> -BuildVersion=<version> -Json=<path/to/file.json>
 *
 * 4) Serialize all chunk object key(s) to JSON.
 * UnrealPak.exe -Region="us-east-1" -BucketPath=<mybucket/some/data/path/> -CredentialsFile=<path/to/credentials.txt> -BuildVersion=<version> -ChunkKeys=<path/to/file.json>
 *
 * 5) Fetch a TOC from a public CDN.
 * UnrealPak.exe -ListTocs -TocUrl=<http://some.public.endpoint.net/path/to/1a32076ca12bfc6feb982ffb064d18f28156606c.iochunktoc>
 *
 * Parameters: -TocEntries, -BlockSizes and -BlockHashes controls what to include when serializing TOC's to JSON.
 *
 * Credentials file example:
 *
 * [default]
 * aws_access_key_id="<key>"
 * aws_secret_access_key="<key>
 * aws_session_token="<token>"
 *
 * Note: All values must be surounded with "".
 */
struct FIoStoreListTocsParams
{
	FString OutFile;
	FString ServiceUrl;
	FString Bucket;
	FString BucketPrefix;
	FString Region; 
	FString AccessKey;
	FString SecretKey;
	FString SessionToken;
	FString CredentialsFile;
	FString CredentialsFileKeyName;
	FString TocUrl;
	FString TocKey;
	FString BuildVersion;
	FString TargetPlatform;
	FString ChunkKeys;
	bool bTocEntries = false;
	bool bBlockSizes = false;
	bool bBlockHashes = false;

	static TIoStatusOr<FIoStoreListTocsParams> Parse(const TCHAR* CommandLine);
	FIoStatus Validate() const;
};

FIoStatus ListTocs(const FIoStoreListTocsParams& Params);

#endif // (IS_PROGRAM || WITH_EDITOR)

class IOnDemandIoDispatcherBackend;

#if UE_IAS_CUSTOM_INITIALIZATION

/** Result of calling FIoStoreOnDemandModule::Initialize */
enum class EOnDemandInitResult
{
	/** The module initialized correctly and can be used */
	Success = 0,
	/** The module is disabled as OnDemand data is not required for the current process*/
	Disabled,
	/** The module was unable to start up correctly due to an unexpected error */
	Error,

	/**
	 * The use of the module has been suspended, if possible calling systems should activate alternative ways
	 * to access the OnDemand data. This option is temporary and not intended for general use.
	 */
	Suspended
};

#endif // UE_IAS_CUSTOM_INITIALIZATION

/** Options for controlling the behavior of mount requests. */
enum class EOnDemandMountOptions
{
	/** The TOC is loaded but not installed or available for streaming. */
	None			= 0,
	/** Make on-demand container(s) within a TOC available for streaming. */
	StreamOnDemand	= 1 << 0,
	/** Download and install on-demand contianer(s) to local storage. */
	Install			= 1 << 1
};
ENUM_CLASS_FLAGS(EOnDemandMountOptions);

struct FOnDemandMountArgs
{
	/** Mount an already serialized TOC. */
	TOptional<FOnDemandToc> Toc;
	/** Mandatory ID to be used for unmounting all container file(s) included in the TOC. */
	FString MountId;
	/** Download the TOC from the specified URL. */
	FString Url;
	/** Serialize the TOC from the specified file path. */
	FString FilePath;
	/** Mount options. */
	EOnDemandMountOptions Options;
};

struct FOnDemandMountResult
{
	FString MountId;
};

using FOnDemandMountCompleted = TFunction<void(TIoStatusOr<FOnDemandMountResult>)>;

class FIoStoreOnDemandModule
	: public IModuleInterface
{
private:
	void InitializeInternal();
	TSharedPtr<IOnDemandIoDispatcherBackend> HttpIoDispatcherBackend;
	// Deferred state requests if called before backend
	// is initialized
	TOptional<bool> DeferredEnabled;
	TOptional<bool> DeferredAbandonCache;
	TOptional<bool> DeferredBulkOptionalEnabled;
	TUniquePtr<FOnDemandIoStore> IoStore;

public:
	UE_API void SetBulkOptionalEnabled(bool bInEnabled);
	UE_API void SetEnabled(bool bInEnabled);
	UE_API bool IsEnabled() const;
	UE_API void AbandonCache();

	UE_API void ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const;

	UE_API void Mount(FOnDemandMountArgs&& Args, FOnDemandMountCompleted&& OnCompleted);
	UE_API FIoStatus Unmount(FStringView MountId);

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
#if UE_IAS_CUSTOM_INITIALIZATION
	UE_API EOnDemandInitResult Initialize();
#endif //UE_IAS_CUSTOM_INITIALIZATION
};

} // namespace UE::IoStore

#undef UE_API
