// Copyright Epic Games, Inc. All Rights Reserved.

#include "IO/IoStoreOnDemand.h"

#include "HAL/FileManager.h"
#include "HAL/FileManagerGeneric.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "IO/IoChunkEncoding.h"
#include "IasCache.h"
#include "Misc/Base64.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DateTime.h"
#include "Misc/EncryptionKeyManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/PathViews.h"
#include "Modules/ModuleManager.h"
#include "OnDemandIoStore.h"
#include "OnDemandHttpClient.h"
#include "OnDemandIoDispatcherBackend.h"
#include "Serialization/Archive.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Serialization/CustomVersion.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Statistics.h"
#include "String/LexFromString.h"

#if (PLATFORM_DESKTOP && (IS_PROGRAM || WITH_EDITOR))
#include "Algo/Sort.h"
#include "Async/Async.h"
#include "Async/Mutex.h"
#include "Async/ParallelFor.h"
#include "Async/UniqueLock.h"
#include "HAL/PlatformFileManager.h"
#include "IO/IoStore.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/KeyChainUtilities.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "S3/S3Client.h"
#endif // (PLATFORM_DESKTOP && (IS_PROGRAM || WITH_EDITOR))

DEFINE_LOG_CATEGORY(LogIoStoreOnDemand);
DEFINE_LOG_CATEGORY(LogIas);

namespace UE::IoStore
{

////////////////////////////////////////////////////////////////////////////////
FString GIasOnDemandTocExt = TEXT(".uondemandtoc");

bool GIasSuspendSystem = false;
static FAutoConsoleVariableRef CVar_SuspendSystemEnabled(
	TEXT("ias.SuspendSystem"),
	GIasSuspendSystem,
	TEXT("Suspends the use of the OnDemand system"),
	ECVF_ReadOnly
);

/** Temp cvar to allow the fallback url to be hotfixed in case of problems */
static FString GDistributedEndpointFallbackUrl;
static FAutoConsoleVariableRef CVar_DistributedEndpointFallbackUrl(
	TEXT("ias.DistributedEndpointFallbackUrl"),
	GDistributedEndpointFallbackUrl,
	TEXT("CDN url to be used if a distributed endpoint cannot be reached (overrides IoStoreOnDemand.ini)")
);

////////////////////////////////////////////////////////////////////////////////
static int64 ParseSizeParam(FStringView Value)
{
	Value = Value.TrimStartAndEnd();

	int64 Size = -1;
	LexFromString(Size, Value);
	if (Size >= 0)
	{
		if (Value.EndsWith(TEXT("GB"))) return Size << 30;
		if (Value.EndsWith(TEXT("MB"))) return Size << 20;
		if (Value.EndsWith(TEXT("KB"))) return Size << 10;
	}
	return Size;
}

////////////////////////////////////////////////////////////////////////////////
static int64 ParseSizeParam(const TCHAR* CommandLine, const TCHAR* Param)
{
	FString ParamValue;
	if (!FParse::Value(CommandLine, Param, ParamValue))
	{
		return -1;
	}

	return ParseSizeParam(ParamValue);
}

////////////////////////////////////////////////////////////////////////////////
static bool ParseEncryptionKeyParam(const FString& Param, FGuid& OutKeyGuid, FAES::FAESKey& OutKey)
{
	TArray<FString> Tokens;
	Param.ParseIntoArray(Tokens, TEXT(":"), true);

	if (Tokens.Num() == 2)
	{
		TArray<uint8> KeyBytes;
		if (FGuid::Parse(Tokens[0], OutKeyGuid) && FBase64::Decode(Tokens[1], KeyBytes))
		{
			if (OutKeyGuid != FGuid() && KeyBytes.Num() == FAES::FAESKey::KeySize)
			{
				FMemory::Memcpy(OutKey.Key, KeyBytes.GetData(), FAES::FAESKey::KeySize);
				return true;
			}
		}
	}
	
	return false;
}

////////////////////////////////////////////////////////////////////////////////
static bool ApplyEncryptionKeyFromString(const FString& GuidKeyPair)
{
	FGuid KeyGuid;
	FAES::FAESKey Key;

	if (ParseEncryptionKeyParam(GuidKeyPair, KeyGuid, Key))
	{
		// TODO: PAK and I/O store should share key manager
		FEncryptionKeyManager::Get().AddKey(KeyGuid, Key);
		FCoreDelegates::GetRegisterEncryptionKeyMulticastDelegate().Broadcast(KeyGuid, Key);

		return true;
	}
	else
	{
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////
static bool TryParseConfigContent(const FString& ConfigContent, const FString& ConfigFileName, FOnDemandEndpointConfig& OutEndpoint)
{
	if (ConfigContent.IsEmpty())
	{
		return false;
	}

	FConfigFile Config;
	Config.ProcessInputFileContents(ConfigContent, ConfigFileName);

	Config.GetString(TEXT("Endpoint"), TEXT("DistributionUrl"), OutEndpoint.DistributionUrl);
	if (!OutEndpoint.DistributionUrl.IsEmpty())
	{
		Config.GetString(TEXT("Endpoint"), TEXT("FallbackUrl"), OutEndpoint.FallbackUrl);

		if (!GDistributedEndpointFallbackUrl.IsEmpty())
		{
			OutEndpoint.FallbackUrl = GDistributedEndpointFallbackUrl;
		}
	}
	
	Config.GetArray(TEXT("Endpoint"), TEXT("ServiceUrl"), OutEndpoint.ServiceUrls);
	Config.GetString(TEXT("Endpoint"), TEXT("TocPath"), OutEndpoint.TocPath);

	if (OutEndpoint.DistributionUrl.EndsWith(TEXT("/")))
	{
		OutEndpoint.DistributionUrl = OutEndpoint.DistributionUrl.Left(OutEndpoint.DistributionUrl.Len() - 1);
	}

	for (FString& ServiceUrl : OutEndpoint.ServiceUrls)
	{
		if (ServiceUrl.EndsWith(TEXT("/")))
		{
			ServiceUrl.LeftInline(ServiceUrl.Len() - 1);
		}
	}

	if (OutEndpoint.TocPath.StartsWith(TEXT("/")))
	{
		OutEndpoint.TocPath.RightChopInline(1);
	}

	FString ContentKey;
	if (Config.GetString(TEXT("Endpoint"), TEXT("ContentKey"), ContentKey))
	{
		ApplyEncryptionKeyFromString(ContentKey);
	}

	return OutEndpoint.IsValid();
}

////////////////////////////////////////////////////////////////////////////////
static bool TryParseConfigFileFromPlatformPackage(FOnDemandEndpointConfig& OutConfig)
{
	const FString ConfigFileName = TEXT("IoStoreOnDemand.ini");
	const FString ConfigPath = FPaths::Combine(TEXT("Cloud"), ConfigFileName);
	
	if (FPlatformMisc::FileExistsInPlatformPackage(ConfigPath))
	{
		const FString ConfigContent = FPlatformMisc::LoadTextFileFromPlatformPackage(ConfigPath);
		return TryParseConfigContent(ConfigContent, ConfigFileName, OutConfig);
	}
	else
	{
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////
bool TryParseEndpointConfig(const TCHAR* CommandLine, FOnDemandEndpointConfig& OutConfig)
{
	OutConfig = FOnDemandEndpointConfig();
#if !UE_BUILD_SHIPPING
	FString UrlParam;
	if (FParse::Value(CommandLine, TEXT("Ias.TocUrl="), UrlParam))
	{
		FStringView UrlView(UrlParam);
		if (UrlView.StartsWith(TEXTVIEW("http://")) && UrlView.EndsWith(TEXTVIEW(".iochunktoc")))
		{
			int32 Delim = INDEX_NONE;
			if (UrlView.RightChop(7).FindChar(TEXT('/'), Delim))
			{
				OutConfig.ServiceUrls.Add(FString(UrlView.Left(7 +  Delim)));
				OutConfig.TocPath = UrlView.RightChop(OutConfig.ServiceUrls[0].Len() + 1);
			}
		}
	}
	else
#endif
	{
		if (TryParseConfigFileFromPlatformPackage(OutConfig))
		{
			TStringBuilder<256> TocFilePath;
			FPathViews::Append(TocFilePath, TEXT("Cloud"), FPaths::GetBaseFilename(OutConfig.TocPath));
			TocFilePath.Append(TEXT(".iochunktoc"));

			if (FPlatformMisc::FileExistsInPlatformPackage(*TocFilePath))
			{
				OutConfig.TocFilePath = TocFilePath;
			}
		}
	}

	return OutConfig.IsValid();
}

////////////////////////////////////////////////////////////////////////////////
static FIasCacheConfig GetIasCacheConfig(const TCHAR* CommandLine)
{
	FIasCacheConfig Ret;

	// Fetch values from .ini files
	auto GetConfigIntImpl = [CommandLine] (const TCHAR* ConfigKey, const TCHAR* ParamName, auto& Out)
	{
		int64 Value = -1;
		if (FString Temp; GConfig->GetString(TEXT("Ias"), ConfigKey, Temp, GEngineIni))
		{
			Value = ParseSizeParam(Temp);
		}
#if !UE_BUILD_SHIPPING
		if (int64 Override = ParseSizeParam(CommandLine, ParamName); Override >= 0)
		{
			Value = Override;
		}
#endif

		if (Value >= 0)
		{
			Out = decltype(Out)(Value);
		}

		return true;
	};

#define GetConfigInt(Name, Dest) \
	do { GetConfigIntImpl(TEXT("FileCache.") Name, TEXT("Ias.FileCache.") Name TEXT("="), Dest); } while (false)
	GetConfigInt(TEXT("WritePeriodSeconds"),	Ret.WriteRate.Seconds);
	GetConfigInt(TEXT("WriteOpsPerPeriod"),		Ret.WriteRate.Ops);
	GetConfigInt(TEXT("WriteBytesPerPeriod"),	Ret.WriteRate.Allowance);
	GetConfigInt(TEXT("DiskQuota"),				Ret.DiskQuota);
	GetConfigInt(TEXT("MemoryQuota"),			Ret.MemoryQuota);
	GetConfigInt(TEXT("JournalQuota"),			Ret.JournalQuota);
	GetConfigInt(TEXT("JournalMagic"),			Ret.JournalMagic);
	GetConfigInt(TEXT("DemandThreshold"),		Ret.Demand.Threshold);
	GetConfigInt(TEXT("DemandBoost"),			Ret.Demand.Boost);
	GetConfigInt(TEXT("DemandSuperBoost"),		Ret.Demand.SuperBoost);
#undef GetConfigInt

#if !UE_BUILD_SHIPPING
	if (FParse::Param(CommandLine, TEXT("Ias.DropCache")))
	{
		Ret.DropCache = true;
	}
	if (FParse::Param(CommandLine, TEXT("Ias.NoCache")))
	{
		Ret.DiskQuota = 0;
	}
#endif

	return Ret;
}

////////////////////////////////////////////////////////////////////////////////
static void LoadCaCerts()
{
	using namespace UE::IoStore::HTTP;

	IFileManager& Ifm = IFileManager::Get();
	FString PemPath = FPaths::EngineContentDir() / TEXT("Certificates/ThirdParty/cacert.pem");
	FArchive* Reader = Ifm.CreateFileReader(*PemPath);
	check(Reader != nullptr)

	uint32 Size = uint32(Reader->TotalSize());
	FIoBuffer PemData(Size);
	FMutableMemoryView PemView = PemData.GetMutableView();
	Reader->Serialize(PemView.GetData(), Size);

	FCertRoots CaRoots(PemData.GetView());

	uint32 NumCerts = CaRoots.Num();
	UE_LOG(LogIas, Display, TEXT("CaRoots: %u (%u .pem bytes))"), NumCerts, Size);

	FCertRoots::SetDefault(MoveTemp(CaRoots));
	delete Reader;
}

////////////////////////////////////////////////////////////////////////////////
/**
 * Utility to create a FArchive capable of reading from disk using the exact same pathing
 * rules as FPlatformMisc::LoadTextFileFromPlatformPackage but without forcing the entire
 * file to be loaded at once.
 */
static TUniquePtr<FArchive> CreateReaderFromPlatformPackage(const FString& RelPath)
{
	const FString AbsPath = FPaths::Combine(FGenericPlatformMisc::RootDir(), RelPath);
	if (TUniquePtr<IFileHandle> File(IPlatformFile::GetPlatformPhysical().OpenRead(*AbsPath)); File.IsValid())
	{
#if PLATFORM_ANDROID
		// This is a handle to an asset so we need to call Seek(0) to move the internal
		// offset to the start of the asset file.
		File->Seek(0);
#endif //PLATFORM_ANDROID
		const uint32 ReadBufferSize = 256 * 1024;
		const int64 FileSize = File->Size();
		return MakeUnique<FArchiveFileReaderGeneric>(File.Release(), *AbsPath, FileSize, ReadBufferSize);
	}

	return TUniquePtr<FArchive>();
}

////////////////////////////////////////////////////////////////////////////////
FArchive& operator<<(FArchive& Ar, FTocMeta& Meta)
{
	Ar << Meta.EpochTimestamp;
	Ar << Meta.BuildVersion;
	Ar << Meta.TargetPlatform;
	return Ar;
}

FCbWriter& operator<<(FCbWriter& Writer, const FTocMeta& Meta)
{
	Writer.BeginObject();
	Writer.AddInteger(UTF8TEXTVIEW("EpochTimestamp"), Meta.EpochTimestamp);
	Writer.AddString(UTF8TEXTVIEW("BuildVersion"), Meta.BuildVersion);
	Writer.AddString(UTF8TEXTVIEW("TargetPlatform"), Meta.TargetPlatform);
	Writer.EndObject();

	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FTocMeta& OutMeta)
{
	if (FCbObjectView Obj = Field.AsObjectView())
	{
		OutMeta.EpochTimestamp = Obj["EpochTimestamp"].AsInt64();
		OutMeta.BuildVersion = FString(Obj["BuildVersion"].AsString());
		OutMeta.TargetPlatform = FString(Obj["TargetPlatform"].AsString());
		return true;
	}
	
	return false;
}

FArchive& operator<<(FArchive& Ar, FOnDemandTocHeader& Header)
{
	if (Ar.IsLoading() && Ar.TotalSize() < sizeof(FOnDemandTocHeader))
	{
		Ar.SetError();
		return Ar;
	}

	Ar << Header.Magic;
	if (Header.Magic != FOnDemandTocHeader::ExpectedMagic)
	{
		Ar.SetError();
		return Ar;
	}

	Ar << Header.Version;
	if (static_cast<EOnDemandTocVersion>(Header.Version) == EOnDemandTocVersion::Invalid)
	{
		Ar.SetError();
		return Ar;
	}

	if (uint32(Header.Version) > uint32(EOnDemandTocVersion::Latest))
	{
		Ar.SetError();
		return Ar;
	}

	Ar << Header.ChunkVersion;
	Ar << Header.BlockSize;
	Ar << Header.CompressionFormat;
	Ar << Header.ChunksDirectory;

	return Ar;
}

FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandTocHeader& Header)
{
	Writer.BeginObject();
	Writer.AddInteger(UTF8TEXTVIEW("Magic"), Header.Magic);
	Writer.AddInteger(UTF8TEXTVIEW("Version"), Header.Version);
	Writer.AddInteger(UTF8TEXTVIEW("ChunkVersion"), Header.ChunkVersion);
	Writer.AddInteger(UTF8TEXTVIEW("BlockSize"), Header.BlockSize);
	Writer.AddString(UTF8TEXTVIEW("CompressionFormat"), Header.CompressionFormat);
	Writer.AddString(UTF8TEXTVIEW("ChunksDirectory"), Header.ChunksDirectory);
	Writer.EndObject();

	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandTocHeader& OutTocHeader)
{
	if (FCbObjectView Obj = Field.AsObjectView())
	{
		OutTocHeader.Magic = Obj["Magic"].AsUInt64();
		OutTocHeader.Version = Obj["Version"].AsUInt32();
		OutTocHeader.ChunkVersion = Obj["ChunkVersion"].AsUInt32();
		OutTocHeader.BlockSize = Obj["BlockSize"].AsUInt32();
		OutTocHeader.CompressionFormat = FString(Obj["CompressionFormat"].AsString());
		OutTocHeader.ChunksDirectory = FString(Obj["ChunksDirectory"].AsString());

		return OutTocHeader.Magic == FOnDemandTocHeader::ExpectedMagic &&
			static_cast<EOnDemandTocVersion>(OutTocHeader.Version) != EOnDemandTocVersion::Invalid;
	}

	return false;
}

FArchive& operator<<(FArchive& Ar, FOnDemandTocEntry& Entry)
{
	Ar << Entry.Hash;
	Ar << Entry.ChunkId;
	Ar << Entry.RawSize;
	Ar << Entry.EncodedSize;
	Ar << Entry.BlockOffset;
	Ar << Entry.BlockCount;

	return Ar;
}

FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandTocEntry& Entry)
{
	Writer.BeginObject();
	Writer.AddHash(UTF8TEXTVIEW("Hash"), Entry.Hash);
	Writer << UTF8TEXTVIEW("ChunkId") << Entry.ChunkId;
	Writer.AddInteger(UTF8TEXTVIEW("RawSize"), Entry.RawSize);
	Writer.AddInteger(UTF8TEXTVIEW("EncodedSize"), Entry.EncodedSize);
	Writer.AddInteger(UTF8TEXTVIEW("BlockOffset"), Entry.BlockOffset);
	Writer.AddInteger(UTF8TEXTVIEW("BlockCount"), Entry.BlockCount);
	Writer.EndObject();

	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandTocEntry& OutTocEntry)
{
	if (FCbObjectView Obj = Field.AsObjectView())
	{
		if (!LoadFromCompactBinary(Obj["ChunkId"], OutTocEntry.ChunkId))
		{
			return false;
		}

		OutTocEntry.Hash = Obj["Hash"].AsHash();
		OutTocEntry.RawSize = Obj["RawSize"].AsUInt64(~uint64(0));
		OutTocEntry.EncodedSize = Obj["EncodedSize"].AsUInt64(~uint64(0));
		OutTocEntry.BlockOffset = Obj["BlockOffset"].AsUInt32(~uint32(0));
		OutTocEntry.BlockCount = Obj["BlockCount"].AsUInt32();

		return OutTocEntry.Hash != FIoHash::Zero &&
			OutTocEntry.RawSize != ~uint64(0) &&
			OutTocEntry.EncodedSize != ~uint64(0) &&
			OutTocEntry.BlockOffset != ~uint32(0);
	}

	return false;
}

FArchive& operator<<(FArchive& Ar, FOnDemandTocContainerEntry& ContainerEntry)
{
	if (Ar.IsLoading())
	{
		const FCustomVersion* CustomVersion = Ar.GetCustomVersions().GetVersion(FOnDemandToc::VersionGuid);
		check(CustomVersion);
		const uint32 TocVersion = IntCastChecked<uint32>(CustomVersion->Version);

		if (TocVersion >= uint32(EOnDemandTocVersion::ContainerId))
		{
			Ar << ContainerEntry.ContainerId;
		}
	}
	else
	{
		Ar << ContainerEntry.ContainerId;
	}

	Ar << ContainerEntry.ContainerName;
	Ar << ContainerEntry.EncryptionKeyGuid;
	Ar << ContainerEntry.Entries;
	Ar << ContainerEntry.BlockSizes;
	Ar << ContainerEntry.BlockHashes;
	Ar << ContainerEntry.UTocHash;

	return Ar;
}

FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandTocContainerEntry& ContainerEntry)
{
	Writer.BeginObject();
	Writer << UTF8TEXTVIEW("Id") << ContainerEntry.ContainerId;
	Writer.AddString(UTF8TEXTVIEW("Name"), ContainerEntry.ContainerName);
	Writer.AddString(UTF8TEXTVIEW("EncryptionKeyGuid"), ContainerEntry.EncryptionKeyGuid);

	Writer.BeginArray(UTF8TEXTVIEW("Entries"));
	for (const FOnDemandTocEntry& Entry : ContainerEntry.Entries)
	{
		Writer << Entry;
	}
	Writer.EndArray();
	
	Writer.BeginArray(UTF8TEXTVIEW("BlockSizes"));
	for (uint32 BlockSize : ContainerEntry.BlockSizes)
	{
		Writer << BlockSize;
	}
	Writer.EndArray();

	Writer.BeginArray(UTF8TEXTVIEW("BlockHashes"));
	for (uint32 BlockHash : ContainerEntry.BlockHashes)
	{
		Writer << BlockHash;
	}
	Writer.EndArray();

	Writer.AddHash(UTF8TEXTVIEW("UTocHash"), ContainerEntry.UTocHash);

	Writer.EndObject();

	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandTocContainerEntry& OutContainer)
{
	if (FCbObjectView Obj = Field.AsObjectView())
	{
		OutContainer.ContainerName = FString(Obj["Name"].AsString());
		OutContainer.EncryptionKeyGuid = FString(Obj["EncryptionKeyGuid"].AsString());

		FCbArrayView Entries = Obj["Entries"].AsArrayView();
		OutContainer.Entries.Reserve(int32(Entries.Num()));
		for (FCbFieldView ArrayField : Entries)
		{
			if (!LoadFromCompactBinary(ArrayField, OutContainer.Entries.AddDefaulted_GetRef()))
			{
				return false;
			}
		}

		FCbArrayView BlockSizes = Obj["BlockSizes"].AsArrayView();
		OutContainer.BlockSizes.Reserve(int32(BlockSizes.Num()));
		for (FCbFieldView ArrayField : BlockSizes)
		{
			OutContainer.BlockSizes.Add(ArrayField.AsUInt32());
		}

		FCbArrayView BlockHashes = Obj["BlockHashes"].AsArrayView();
		OutContainer.BlockHashes.Reserve(int32(BlockHashes.Num()));
		for (FCbFieldView ArrayField : BlockHashes)
		{
			if (ArrayField.IsHash())
			{
				const FIoHash BlockHash = ArrayField.AsHash();
				OutContainer.BlockHashes.Add(*reinterpret_cast<const uint32*>(&BlockHash));
			}
			else
			{
				OutContainer.BlockHashes.Add(ArrayField.AsUInt32());
			}
		}

		OutContainer.UTocHash = Obj["UTocHash"].AsHash();

		return true;
	}

	return false;
}

bool FOnDemandTocSentinel::IsValid()
{
	return FMemory::Memcmp(&Data, FOnDemandTocSentinel::SentinelImg, FOnDemandTocSentinel::SentinelSize) == 0;
}

FArchive& operator<<(FArchive& Ar, FOnDemandTocSentinel& Sentinel)
{
	if (Ar.IsSaving())
	{	
		// We could just cast FOnDemandTocSentinel::SentinelImg to a non-const pointer but we can't be 
		// 100% sure that the FArchive won't change the data, even if it is in Saving mode. Since this 
		// isn't performance critical we will play it safe.
		uint8 Output[FOnDemandTocSentinel::SentinelSize];
		FMemory::Memcpy(Output, FOnDemandTocSentinel::SentinelImg, FOnDemandTocSentinel::SentinelSize);

		Ar.Serialize(&Output, FOnDemandTocSentinel::SentinelSize);
	}
	else
	{
		Ar.Serialize(&Sentinel.Data, FOnDemandTocSentinel::SentinelSize);
	}

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FOnDemandTocAdditionalFile& AdditionalFile)
{
	Ar << AdditionalFile.Hash;
	Ar << AdditionalFile.Filename;
	Ar << AdditionalFile.FileSize;
	return Ar;
}

FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandTocAdditionalFile& AdditionalFile)
{
	Writer.BeginObject();
	Writer.AddHash(UTF8TEXTVIEW("Hash"), AdditionalFile.Hash);
	Writer.AddString(UTF8TEXTVIEW("Filename"), AdditionalFile.Filename);
	Writer.AddInteger(UTF8TEXTVIEW("Filename"), AdditionalFile.FileSize);
	Writer.EndObject();

	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandTocAdditionalFile& AdditionalFile)
{
	if (FCbObjectView Obj = Field.AsObjectView())
	{
		AdditionalFile.Hash = Obj["Hash"].AsHash();
		AdditionalFile.Filename = FString(Obj["Filename"].AsString());
		AdditionalFile.FileSize = Obj["FileSize"].AsUInt64();
		return true;
	}

	return false;
}

FArchive& operator<<(FArchive& Ar, FOnDemandToc& Toc)
{
	Ar << Toc.Header;
	if (Ar.IsError())
	{
		return Ar;
	}

	Ar.SetCustomVersion(Toc.VersionGuid, int32(Toc.Header.Version), TEXT("OnDemandToc"));

	if (uint32(Toc.Header.Version) >= uint32(EOnDemandTocVersion::Meta))
	{
		Ar << Toc.Meta;
	}
	Ar << Toc.Containers;

	if (uint32(Toc.Header.Version) >= uint32(EOnDemandTocVersion::AdditionalFiles))
	{
		Ar << Toc.AdditionalFiles;
	}

	return Ar;
}

FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandToc& Toc)
{
	Writer.BeginObject();
	Writer << UTF8TEXTVIEW("Header") << Toc.Header;

	Writer.BeginArray(UTF8TEXTVIEW("Containers"));
	for (const FOnDemandTocContainerEntry& Container : Toc.Containers)
	{
		Writer << Container;
	}
	Writer.EndArray();

	if (Toc.AdditionalFiles.Num() > 0)
	{
		Writer.BeginArray(UTF8TEXTVIEW("Files"));
		for (const FOnDemandTocAdditionalFile& File : Toc.AdditionalFiles)
		{
			Writer << File;
		}
		Writer.EndArray();
	}

	Writer.EndObject();
	
	return Writer;
}

FGuid FOnDemandToc::VersionGuid = FGuid("C43DD98F353F499D9A0767F6EA0155EB");

bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandToc& OutToc)
{
	if (FCbObjectView Obj = Field.AsObjectView())
	{
		if (!LoadFromCompactBinary(Obj["Header"], OutToc.Header))
		{
			return false;
		}

		if (uint32(OutToc.Header.Version) >= uint32(EOnDemandTocVersion::Meta))
		{
			if (!LoadFromCompactBinary(Obj["Meta"], OutToc.Meta))
			{
				return false;
			}
		}

		FCbArrayView Containers = Obj["Containers"].AsArrayView();
		OutToc.Containers.Reserve(int32(Containers.Num()));
		for (FCbFieldView ArrayField : Containers)
		{
			if (!LoadFromCompactBinary(ArrayField, OutToc.Containers.AddDefaulted_GetRef()))
			{
				return false;
			}
		}

		if (uint32(OutToc.Header.Version) >= uint32(EOnDemandTocVersion::AdditionalFiles))
		{
			FCbArrayView Files = Obj["Files"].AsArrayView();
			OutToc.AdditionalFiles.Reserve(int32(Files.Num()));
			for (FCbFieldView ArrayField : Files)
			{
				if (!LoadFromCompactBinary(ArrayField, OutToc.AdditionalFiles.AddDefaulted_GetRef()))
				{
					return false;
				}
			}
		}

		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////
TIoStatusOr<FOnDemandToc> FOnDemandToc::LoadFromFile(const FString& FilePath, bool bValidate)
{
	TUniquePtr<FArchive> Ar;
	if (FPlatformMisc::FileExistsInPlatformPackage(FilePath))
	{
		Ar = CreateReaderFromPlatformPackage(FilePath);
	}
	else
	{
		Ar.Reset(IFileManager::Get().CreateFileReader(*FilePath));
	}

	if (Ar.IsValid() == false)
	{
		FIoStatus Status = FIoStatusBuilder(EIoErrorCode::FileNotOpen) << TEXT("Failed to open '") << FilePath << TEXT("'");
		return Status;
	}

	if (bValidate)
	{
		const int64 SentinelPos = Ar->TotalSize() - FOnDemandTocSentinel::SentinelSize;

		if (SentinelPos < 0)
		{
			FIoStatus Status = FIoStatusBuilder(EIoErrorCode::CorruptToc) << TEXT("Unexpected file size");
			return Status;
		}

		Ar->Seek(SentinelPos);

		FOnDemandTocSentinel Sentinel;
		*Ar << Sentinel;

		if (!Sentinel.IsValid())
		{
			return FIoStatus(EIoErrorCode::CorruptToc);
		}

		Ar->Seek(0);
	}

	FOnDemandToc Toc;
	*Ar << Toc;

	if (Ar->IsError() || Ar->IsCriticalError())
	{
		FIoStatus Status = FIoStatusBuilder(EIoErrorCode::FileNotOpen) << TEXT("Failed to serialize TOC file");
		return Status;
	}

	return Toc; 
}

////////////////////////////////////////////////////////////////////////////////
TIoStatusOr<FOnDemandToc> FOnDemandToc::LoadFromUrl(FAnsiStringView Url, uint32 RetryCount, bool bFollowRedirects)
{
	const EHttpRedirects Redirects = bFollowRedirects ? EHttpRedirects::Follow : EHttpRedirects::Disabled;
	TIoStatusOr<FIoBuffer> Response = FHttpClient::Get(Url, RetryCount, Redirects); 

	if (!Response.IsOk())
	{
		FIoStatus Status = FIoStatusBuilder(EIoErrorCode::ReadError) << TEXT("Failed to fetch TOC from URL");
		return Status;
	}

	FMemoryReaderView Ar(Response.ValueOrDie().GetView());
	FOnDemandToc Toc;
	Ar << Toc;

	if (Ar.IsError() || Ar.IsCriticalError())
	{
		FIoStatus Status = FIoStatusBuilder(EIoErrorCode::ReadError) << TEXT("Failed to serialize TOC from HTTP response");
		return Status;
	}

	return Toc; 
}

////////////////////////////////////////////////////////////////////////////////
TIoStatusOr<FOnDemandToc> FOnDemandToc::LoadFromUrl(FStringView Url, uint32 RetryCount, bool bFollowRedirects)
{
	auto AnsiUrl = StringCast<ANSICHAR>(Url.GetData(), Url.Len());
	return LoadFromUrl(AnsiUrl, RetryCount, bFollowRedirects);
}

////////////////////////////////////////////////////////////////////////////////
#if (PLATFORM_DESKTOP && (IS_PROGRAM || WITH_EDITOR))

using FJsonWriter = TSharedPtr<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>>; 
using FJsonWriterFactory = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>;

////////////////////////////////////////////////////////////////////////////////
static void GetChunkObjectKeys(const FOnDemandToc& Toc, FString Prefix, TArray<FString>& OutKeys)
{
	if (Prefix.EndsWith(TEXT("/")))
	{
		Prefix = Prefix.Left(Prefix.Len() - 1);
	}

	TStringBuilder<256> Sb;
	for (const FOnDemandTocContainerEntry& Container : Toc.Containers)
	{
		for (const FOnDemandTocEntry& Entry : Container.Entries)
		{
			Sb.Reset();
			const FString Hash = LexToString(Entry.Hash);
			Sb << Prefix << TEXT("/") << Toc.Header.ChunksDirectory << TEXT("/") << Hash.Left(2) << TEXT("/") << Hash << TEXT(".iochunk");
			OutKeys.Add(FString(Sb.ToString()).ToLower());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
enum class EOnDemandTocJsonOptions
{
	Header			= (1 << 0),
	TocEntries		= (1 << 1),
	BlockSizes		= (1 << 2),
	BlockHashes		= (1 << 3),
	All				= Header | TocEntries | BlockSizes | BlockHashes
};
ENUM_CLASS_FLAGS(EOnDemandTocJsonOptions);

static void ToJson(FJsonWriter JsonWriter, const FOnDemandToc& Toc, EOnDemandTocJsonOptions Options)
{
	JsonWriter->WriteObjectStart(TEXT("Header"));
	{
		JsonWriter->WriteValue(TEXT("Magic"), Toc.Header.Magic);
		JsonWriter->WriteValue(TEXT("Version"), uint64(Toc.Header.Version));
		JsonWriter->WriteValue(TEXT("BlockSize"), uint64(Toc.Header.BlockSize));
		JsonWriter->WriteValue(TEXT("CompressonFormat"), Toc.Header.CompressionFormat);
		JsonWriter->WriteValue(TEXT("ChunksDirectory"), Toc.Header.ChunksDirectory);
	}
	JsonWriter->WriteObjectEnd();
	JsonWriter->WriteObjectStart(TEXT("Meta"));
	{
		const FDateTime Dt = FDateTime::FromUnixTimestamp(Toc.Meta.EpochTimestamp);
		JsonWriter->WriteValue(TEXT("DateTime"), Dt.ToString()), 
		JsonWriter->WriteValue(TEXT("BuildVersion"), Toc.Meta.BuildVersion);
		JsonWriter->WriteValue(TEXT("TargetPlatform"), Toc.Meta.TargetPlatform);
	}
	JsonWriter->WriteObjectEnd();
	JsonWriter->WriteArrayStart(TEXT("Containers"));
	{
		for (const FOnDemandTocContainerEntry& Container : Toc.Containers)
		{
			JsonWriter->WriteObjectStart();
			JsonWriter->WriteValue(TEXT("Name"), Container.ContainerName);
			JsonWriter->WriteValue(TEXT("EncryptionKeyGuid"), Container.EncryptionKeyGuid);

			if (EnumHasAnyFlags(Options, EOnDemandTocJsonOptions::TocEntries))
			{
				JsonWriter->WriteArrayStart("Entries");
				for (const FOnDemandTocEntry& Entry : Container.Entries)
				{
					JsonWriter->WriteObjectStart();
					JsonWriter->WriteValue(TEXT("Hash"), LexToString(Entry.Hash));
					JsonWriter->WriteValue(TEXT("ChunkId"), LexToString(Entry.ChunkId));
					JsonWriter->WriteValue(TEXT("RawSize"), Entry.RawSize);
					JsonWriter->WriteValue(TEXT("EncodedSize"), Entry.EncodedSize);
					JsonWriter->WriteValue(TEXT("BlockOffset"), int32(Entry.BlockOffset));
					JsonWriter->WriteValue(TEXT("BlockCount"), int32(Entry.BlockCount));
					JsonWriter->WriteObjectEnd();
				}
				JsonWriter->WriteArrayEnd();
			}

			if (EnumHasAnyFlags(Options, EOnDemandTocJsonOptions::BlockSizes))
			{
				JsonWriter->WriteArrayStart("Blocks");
				{
					for (uint32 BlockSize : Container.BlockSizes)
					{
						JsonWriter->WriteValue(int32(BlockSize));
					}
				}
				JsonWriter->WriteArrayEnd();
			}

			if (EnumHasAnyFlags(Options, EOnDemandTocJsonOptions::BlockHashes))
			{
				JsonWriter->WriteArrayStart("BlockHashes");
				{
					for (const FIoBlockHash& BlockHash : Container.BlockHashes)
					{
						JsonWriter->WriteValue(int32(BlockHash));
					}
				}
				JsonWriter->WriteArrayEnd();
			}
			JsonWriter->WriteObjectEnd();
		}
	}
	JsonWriter->WriteArrayEnd();
}

////////////////////////////////////////////////////////////////////////////////
TIoStatusOr<FIoStoreDownloadParams> FIoStoreDownloadParams::Parse(const TCHAR* CommandLine)
{
	FIoStoreDownloadParams Params;

	if (!FParse::Value(CommandLine, TEXT("Directory="), Params.Directory))
	{
		FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid output directory"));
	}

	if (!FParse::Value(CommandLine, TEXT("Bucket="), Params.Bucket))
	{
		FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid bucket name"));
	}

	FParse::Value(CommandLine, TEXT("ServiceUrl="), Params.ServiceUrl);
	FParse::Value(CommandLine, TEXT("Region="), Params.Region);
	FParse::Value(CommandLine, TEXT("AccessKey="), Params.AccessKey);
	FParse::Value(CommandLine, TEXT("SecretKey="), Params.SecretKey);
	FParse::Value(CommandLine, TEXT("SessionToken="), Params.SessionToken);
	FParse::Value(CommandLine, TEXT("CredentialsFile="), Params.CredentialsFile);
	FParse::Value(CommandLine, TEXT("CredentialsFileKeyName="), Params.CredentialsFileKeyName);

	if (FIoStatus Validation = Params.Validate(); !Validation.IsOk())
	{
		return Validation;
	}

	return Params;
}

FIoStatus FIoStoreDownloadParams::Validate() const
{
	if (!AccessKey.IsEmpty() && SecretKey.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid secret key"));
	}
	else if (AccessKey.IsEmpty() && !SecretKey.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid access key"));
	}

	if (!CredentialsFile.IsEmpty() && CredentialsFileKeyName.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid credential file key name"));
	}

	if (ServiceUrl.IsEmpty() && Region.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Service URL or AWS region needs to be specified"));
	}

	return FIoStatus::Ok;
}

FIoStatus DownloadContainerFiles(const FIoStoreDownloadParams& DownloadParams, const FString& TocPath)
{
	struct FContainerStats
	{
		uint64 TocEntryCount = 0;
		uint64 TocRawSize = 0;
		uint64 RawSize = 0;
		uint64 CompressedSize = 0;
		EIoContainerFlags ContainerFlags = EIoContainerFlags::None;
		FName CompressionMethod;
	};
	TMap<FString, FContainerStats> ContainerSummary;

	const double StartTime = FPlatformTime::Seconds();

	FS3ClientConfig Config;
	Config.ServiceUrl = DownloadParams.ServiceUrl;
	Config.Region = DownloadParams.Region;
	
	FS3ClientCredentials Credentials;
	if (DownloadParams.CredentialsFile.IsEmpty() == false)
	{
		UE_LOG(LogIas, Display, TEXT("Loading credentials file '%s'"), *DownloadParams.CredentialsFile);
		FS3CredentialsProfileStore CredentialsStore = FS3CredentialsProfileStore::FromFile(DownloadParams.CredentialsFile);
		if (CredentialsStore.TryGetCredentials(DownloadParams.CredentialsFileKeyName, Credentials) == false)
		{
			return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Failed to find valid credentials in credentials file"));
		}
		else
		{
			UE_LOG(LogIas, Display, TEXT("Found credentials for '%s'"), *DownloadParams.CredentialsFileKeyName);
		}
	}
	else
	{
		Credentials = FS3ClientCredentials(DownloadParams.AccessKey, DownloadParams.SecretKey, DownloadParams.SessionToken);
	}

	FS3Client Client(Config, Credentials);

	UE_LOG(LogIas, Display, TEXT("Fetching TOC '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *DownloadParams.Bucket, *TocPath);
	FS3GetObjectResponse TocResponse = Client.GetObject(FS3GetObjectRequest
	{
		DownloadParams.Bucket,
		TocPath
	});

	if (TocResponse.IsOk() == false)
	{
		return FIoStatus(EIoErrorCode::ReadError, TEXT("Failed to fetch TOC"));
	}

	FOnDemandToc OnDemandToc;
	if (LoadFromCompactBinary(FCbFieldView(TocResponse.GetBody().GetData()), OnDemandToc) == false)
	{
		return FIoStatus(EIoErrorCode::ReadError, TEXT("Failed to load on demand TOC"));
	}

	FStringView BucketPrefix;
	{
		FStringView TocPathView = TocPath;
		int32 Index = INDEX_NONE;
		if (TocPathView.FindLastChar(TEXT('/'), Index))
		{
			BucketPrefix = TocPathView.Left(Index);
		}
	}

	for (const FOnDemandTocContainerEntry& ContainerEntry : OnDemandToc.Containers)
	{
		TStringBuilder<256> FileTocKey;
		if (BucketPrefix.IsEmpty() == false)
		{
			FileTocKey << BucketPrefix << TEXT("/");
		}
		FileTocKey << ContainerEntry.UTocHash << TEXT(".utoc");

		UE_LOG(LogIas, Display, TEXT("Fetching '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *DownloadParams.Bucket, FileTocKey.ToString());
		FS3GetObjectResponse Response = Client.GetObject(FS3GetObjectRequest
		{
			DownloadParams.Bucket,
			FString(FileTocKey).ToLower()
		});

		if (Response.IsOk() == false)
		{
			return FIoStatus(EIoErrorCode::ReadError, TEXT("Failed to load container .utoc file"));
		}

		const FString UTocPath = FString::Printf(TEXT("%s/%s.utoc"), *DownloadParams.Directory, *ContainerEntry.ContainerName);
		const FString UCasPath = FPaths::ChangeExtension(UTocPath, TEXT(".ucas")); 
		FContainerStats& ContainerStats = ContainerSummary.FindOrAdd(ContainerEntry.ContainerName);
		ContainerStats.TocRawSize = Response.GetBody().GetSize();

		if (TUniquePtr<FArchive> TocFile(IFileManager::Get().CreateFileWriter(*UTocPath)); TocFile.IsValid())
		{
			UE_LOG(LogIas, Display, TEXT("Writing '%s'"), *UTocPath);
			TocFile->Serialize((void*)Response.GetBody().GetData(), Response.GetBody().GetSize());
		}
		else
		{
			return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to write container .utoc file"));
		}

		FIoStoreTocResource FileToc;
		if (FIoStatus Status = FIoStoreTocResource::Read(*UTocPath, EIoStoreTocReadOptions::ReadAll, FileToc); Status.IsOk() == false)
		{
			return Status;
		}

		const int32 TocEntryCount = int32(FileToc.Header.TocEntryCount);
		const uint64 CompressionBlockSize = FileToc.Header.CompressionBlockSize;
		ContainerStats.TocEntryCount = TocEntryCount;
		ContainerStats.ContainerFlags = FileToc.Header.ContainerFlags; 

		TMap<FIoChunkId, FIoHash> ChunkHashes;
		for (const FOnDemandTocEntry& Entry : ContainerEntry.Entries)
		{
			ChunkHashes.Add(Entry.ChunkId, Entry.Hash);
			ContainerStats.RawSize += Entry.RawSize;
		}

		TArray<int32> SortedIndices;
		for (int32 Idx = 0; Idx < TocEntryCount; ++Idx)
		{
			SortedIndices.Add(Idx);
		}

		Algo::Sort(SortedIndices, [&FileToc](int32 Lhs, int32 Rhs)
		{
			return FileToc.ChunkOffsetLengths[Lhs].GetOffset() < FileToc.ChunkOffsetLengths[Rhs].GetOffset();
		});

		FString ChunksRelativePath = BucketPrefix.IsEmpty()
			? FString::Printf(TEXT("IoChunksV%u"), OnDemandToc.Header.ChunkVersion)
			: FString::Printf(TEXT("%s/IoChunksV%u"), *FString(BucketPrefix), OnDemandToc.Header.ChunkVersion);

		TUniquePtr<FArchive> CasFile(IFileManager::Get().CreateFileWriter(*UCasPath));
		TArray<uint8> PaddingBuffer;

		for (int32 Idx = 0; Idx < TocEntryCount; ++Idx)
		{
			const int32 SortedIdx = SortedIndices[Idx];
			const FIoChunkId& ChunkId = FileToc.ChunkIds[SortedIdx];
			const FIoOffsetAndLength& OffsetLength = FileToc.ChunkOffsetLengths[SortedIdx];
			const FIoHash ChunkHash = ChunkHashes.FindChecked(ChunkId);
			const FString HashString = LexToString(ChunkHash);
			const int32 FirstBlockIndex = int32(OffsetLength.GetOffset() / CompressionBlockSize);
			const int32 LastBlockIndex = int32((Align(OffsetLength.GetOffset() + OffsetLength.GetLength(), CompressionBlockSize) - 1) / CompressionBlockSize);
			const FIoStoreTocCompressedBlockEntry& FirstBlock = FileToc.CompressionBlocks[FirstBlockIndex];
			const FName CompressionMethod = FileToc.CompressionMethods[FirstBlock.GetCompressionMethodIndex()];

			if (CompressionMethod.IsNone() == false && ContainerStats.CompressionMethod.IsNone())
			{
				ContainerStats.CompressionMethod = CompressionMethod;
			}

			TStringBuilder<256> ChunkKey;
			ChunkKey << ChunksRelativePath
				<< TEXT("/") << HashString.Left(2)
				<< TEXT("/") << HashString
				<< TEXT(".iochunk");

			UE_LOG(LogIas, Display, TEXT("Fetching '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *DownloadParams.Bucket, ChunkKey.ToString());
			FS3GetObjectResponse ChunkResponse = Client.GetObject(FS3GetObjectRequest
			{
				DownloadParams.Bucket,
				FString(ChunkKey).ToLower()
			});

			if (ChunkResponse.IsOk() == false)
			{
				return FIoStatus(EIoErrorCode::ReadError, TEXT("Failed to fetch chunk"));
			}

			const uint64 Padding = FirstBlock.GetOffset() - CasFile->Tell();
			if (Padding > PaddingBuffer.Num())
			{
				PaddingBuffer.SetNumZeroed(int32(Padding));
			}

			if (Padding > 0)
			{
				CasFile->Serialize(PaddingBuffer.GetData(), Padding);
			}

			UE_LOG(LogIas, Display, TEXT("Serializing chunk %d/%d '%s' -> '%s' (%llu B)"),
				Idx + 1, TocEntryCount, *HashString, *LexToString(ChunkId), ChunkResponse.GetBody().GetSize());

			check(CasFile->Tell() == FirstBlock.GetOffset());
			CasFile->Serialize((void*)ChunkResponse.GetBody().GetData(), ChunkResponse.GetBody().GetSize());
		}

		ContainerStats.CompressedSize = CasFile->Tell();
	}

	{
		const double Duration = FPlatformTime::Seconds() - StartTime;

		UE_LOG(LogIas, Display, TEXT(""));
		UE_LOG(LogIas, Display, TEXT("---------------------------------------- Download Summary --------------------------------------"));
		UE_LOG(LogIas, Display, TEXT("%-40s: %s"), TEXT("Service URL"), *DownloadParams.ServiceUrl);
		UE_LOG(LogIas, Display, TEXT("%-40s: %s"), TEXT("Bucket"), *DownloadParams.Bucket);
		UE_LOG(LogIas, Display, TEXT("%-40s: %s"), TEXT("TOC"), *TocPath);
		UE_LOG(LogIas, Display, TEXT("%-40s: %.2lf second(s)"), TEXT("Duration"), Duration);
		UE_LOG(LogIas, Display, TEXT(""));

		UE_LOG(LogIas, Display, TEXT("%-30s %10s %15s %15s %15s %25s"),
			TEXT("Container"), TEXT("Flags"), TEXT("TOC Size (KB)"), TEXT("TOC Entries"), TEXT("Size (MB)"), TEXT("Compressed (MB)"));
		UE_LOG(LogIas, Display, TEXT("-------------------------------------------------------------------------------------------------------------------------"));
		
		FContainerStats TotalStats;
		for (const TTuple<FString, FContainerStats>& Kv : ContainerSummary)
		{
			const FString& ContainerName = Kv.Key;
			const FContainerStats& Stats = Kv.Value;

			FString CompressionInfo = TEXT("-");

			if (Stats.CompressionMethod != NAME_None)
			{
				double Procentage = (double(Stats.RawSize - Stats.CompressedSize) / double(Stats.RawSize)) * 100.0;
				CompressionInfo = FString::Printf(TEXT("%.2lf (%.2lf%% %s)"),
					(double)Stats.CompressedSize / 1024.0 / 1024.0,
					Procentage,
					*Stats.CompressionMethod.ToString());
			}

			FString ContainerSettings = FString::Printf(TEXT("%s/%s/%s/%s/%s"),
				EnumHasAnyFlags(Stats.ContainerFlags, EIoContainerFlags::Compressed) ? TEXT("C") : TEXT("-"),
				EnumHasAnyFlags(Stats.ContainerFlags, EIoContainerFlags::Encrypted) ? TEXT("E") : TEXT("-"),
				EnumHasAnyFlags(Stats.ContainerFlags, EIoContainerFlags::Signed) ? TEXT("S") : TEXT("-"),
				EnumHasAnyFlags(Stats.ContainerFlags, EIoContainerFlags::Indexed) ? TEXT("I") : TEXT("-"),
				EnumHasAnyFlags(Stats.ContainerFlags, EIoContainerFlags::OnDemand) ? TEXT("O") : TEXT("-"));

			UE_LOG(LogIas, Display, TEXT("%-30s %10s %15.2lf %15llu %15.2lf %25s"),
				*ContainerName,
				*ContainerSettings,
				(double)Stats.TocRawSize / 1024.0,
				Stats.TocEntryCount,
				(double)Stats.RawSize / 1024.0 / 1024.0,
				*CompressionInfo);

			TotalStats.TocEntryCount += Stats.TocEntryCount;
			TotalStats.TocRawSize += Stats.TocRawSize;
			TotalStats.RawSize += Stats.RawSize;
			TotalStats.CompressedSize += Stats.CompressedSize;
		}
		UE_LOG(LogIas, Display, TEXT("-------------------------------------------------------------------------------------------------------------------------"));
		UE_LOG(LogIas, Display, TEXT("%-30s %10s %15.2lf %15llu %15.2lf %25.2lf "),
			TEXT("Total"),
			TEXT(""),
			(double)TotalStats.TocRawSize / 1024.0,
			TotalStats.TocEntryCount,
			(double)TotalStats.RawSize / 1024.0 / 1024.0,
			(double)TotalStats.CompressedSize / 1024.0 / 1024.0);

		UE_LOG(LogIas, Display, TEXT(""));
		UE_LOG(LogIas, Display, TEXT("** Flags: (C)ompressed / (E)ncrypted / (S)igned) / (I)ndexed) / (O)nDemand **"));
		UE_LOG(LogIas, Display, TEXT(""));
	}

	return FIoStatus::Ok;
}

////////////////////////////////////////////////////////////////////////////////
TIoStatusOr<FIoStoreListTocsParams> FIoStoreListTocsParams::Parse(const TCHAR* CommandLine)
{
	FIoStoreListTocsParams Params;

	// Convenience argument to specify both bucket and bucket prefix
	// -BucketPath="mybucket/some/data/path" is equal to -Bucket="mybucket" -BucketPrefix="some/data/path
	FString BucketPath;
	if (FParse::Value(CommandLine, TEXT("-BucketPath="), BucketPath))
	{
		FStringView PathView = BucketPath;
		int32 Idx = INDEX_NONE;
		if (PathView.FindChar(TCHAR('/'), Idx))
		{
			Params.Bucket = PathView.Left(Idx);
			FStringView BucketPrefix = PathView.RightChop(Idx + 1);
			if (BucketPrefix.EndsWith(TEXT("/")))
			{
				BucketPrefix.LeftChopInline(1);
			}
			Params.BucketPrefix = BucketPrefix;
		}
		else
		{
			Params.Bucket = MoveTemp(BucketPath);
		}
	}

	if (Params.Bucket.IsEmpty())
	{
		if (!FParse::Value(CommandLine, TEXT("Bucket="), Params.Bucket))
		{
			return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid bucket name"));
		}
	}

	FParse::Value(CommandLine, TEXT("BucketPrefix="), Params.BucketPrefix);
	FParse::Value(CommandLine, TEXT("ServiceUrl="), Params.ServiceUrl);
	FParse::Value(CommandLine, TEXT("Region="), Params.Region);
	FParse::Value(CommandLine, TEXT("AccessKey="), Params.AccessKey);
	FParse::Value(CommandLine, TEXT("SecretKey="), Params.SecretKey);
	FParse::Value(CommandLine, TEXT("SessionToken="), Params.SessionToken);
	FParse::Value(CommandLine, TEXT("CredentialsFile="), Params.CredentialsFile);
	FParse::Value(CommandLine, TEXT("TocKey="), Params.TocKey);
	FParse::Value(CommandLine, TEXT("BuildVersion="), Params.BuildVersion);
	FParse::Value(CommandLine, TEXT("TargetPlatform="), Params.TargetPlatform);
	FParse::Value(CommandLine, TEXT("ChunkKeys="), Params.ChunkKeys);
	// JSON serialization options
	Params.bTocEntries = FParse::Param(CommandLine, TEXT("TocEntries"));
	Params.bBlockSizes = FParse::Param(CommandLine, TEXT("BlockSizes"));
	Params.bBlockHashes = FParse::Param(CommandLine, TEXT("BlockHashes"));

	if (!FParse::Value(CommandLine, TEXT("CredentialsFileKeyName="), Params.CredentialsFileKeyName))
	{
		Params.CredentialsFileKeyName = TEXT("default");
	}

	FParse::Value(CommandLine, TEXT("Json="), Params.OutFile);

	if (FParse::Value(CommandLine, TEXT("TocUrl="), Params.TocUrl))
	{
		FStringView UrlView(Params.TocUrl);
		if (UrlView.StartsWith(TEXTVIEW("http://")) && UrlView.EndsWith(TEXTVIEW(".iochunktoc")))
		{
			int32 Delim = INDEX_NONE;
			if (UrlView.RightChop(7).FindChar(TEXT('/'), Delim))
			{
				Params.ServiceUrl = FString(UrlView.Left(7 +  Delim));
				Params.TocKey = UrlView.RightChop(Params.ServiceUrl.Len() + 1);
			}
		}
	}

	if (FIoStatus Status = Params.Validate(); !Status.IsOk())
	{
		return Status;
	}

	return Params;
}

FIoStatus FIoStoreListTocsParams::Validate() const
{
	if (Bucket.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid bucket name"));
	}
	if (!AccessKey.IsEmpty() && SecretKey.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid secret key"));
	}
	else if (AccessKey.IsEmpty() && !SecretKey.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid access key"));
	}

	if (!CredentialsFile.IsEmpty() && CredentialsFileKeyName.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid credential file key name"));
	}

	if (TocUrl.IsEmpty() && ServiceUrl.IsEmpty() && Region.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Service URL or AWS region needs to be specified"));
	}

	return FIoStatus::Ok;
}

FIoStatus ListTocs(const FIoStoreListTocsParams& Params)
{
	struct FChunkStats
	{
		TSet<FIoHash> Chunks;
		uint64 TotalChunkSize = 0;

		void Add(const FIoHash& Hash, uint64 ChunkSize)
		{
			bool bExist = false;
			Chunks.Add(Hash, &bExist);
			if (!bExist)
			{
				TotalChunkSize += ChunkSize;
			}
		}
	};
	TMap<FString, FChunkStats> Stats;

	struct FTocDescription
	{
		FOnDemandToc Toc;
		FDateTime DateTime;
		FString Key;
		uint64 Size = 0;
		uint64 ChunkCount = 0;
		uint64 TotalChunkSize = 0;
	};

	const bool bFilteredQuery = !Params.TocKey.IsEmpty() || !Params.BuildVersion.IsEmpty() || !Params.TargetPlatform.IsEmpty();

	FS3ClientConfig Config;
	Config.ServiceUrl = Params.ServiceUrl;
	Config.Region = Params.Region;
	
	FS3ClientCredentials Credentials;
	if (Params.CredentialsFile.IsEmpty() == false)
	{
		UE_LOG(LogIas, Display, TEXT("Loading credentials file '%s'"), *Params.CredentialsFile);
		FS3CredentialsProfileStore CredentialsStore = FS3CredentialsProfileStore::FromFile(Params.CredentialsFile);
		if (CredentialsStore.TryGetCredentials(Params.CredentialsFileKeyName, Credentials) == false)
		{
			return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Failed to find valid credentials in credentials file"));
		}
		else
		{
			UE_LOG(LogIas, Display, TEXT("Found credentials for '%s'"), *Params.CredentialsFileKeyName);
		}
	}
	else
	{
		Credentials = FS3ClientCredentials(Params.AccessKey, Params.SecretKey, Params.SessionToken);
	}

	FS3Client Client(Config, Credentials);
	FMutex TocMutex;
	TArray<FTocDescription> Tocs;

	if (Params.TocUrl.IsEmpty())
	{
		FStringView PrefixView = Params.BucketPrefix;
		if (PrefixView.StartsWith(TEXT("/")))
		{
			PrefixView.RightChopInline(1);
		}

		TStringBuilder<256> Path;
		if (!PrefixView.IsEmpty())
		{
			Path << PrefixView << TEXT("/");
		}

		UE_LOG(LogIas, Display, TEXT("Fetching TOC's from '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *Params.Bucket, Path.ToString());
		FS3ListObjectResponse Response = Client.ListObjects(FS3ListObjectsRequest
		{
			Params.Bucket,
			Path.ToString(),
			TEXT('/')
		});

		if (Response.Objects.IsEmpty())
		{
			UE_LOG(LogIas, Display, TEXT("Not TOC's found at '%s/%s/%s' (%s)"), *Client.GetConfig().ServiceUrl, *Params.Bucket, Path.ToString(), *Response.GetErrorStatus());
			return FIoStatus(EIoErrorCode::NotFound);
		}

		ParallelFor(Response.Objects.Num(), [&Params, &Client, &Response, &Tocs, &TocMutex](int32 Index)
		{
			const FS3Object& Obj = Response.Objects[Index];
			if (Obj.Key.EndsWith(TEXT("iochunktoc")) == false)
			{
				return;
			}

			if (!Params.TocKey.IsEmpty() && !Params.TocKey.Equals(FPaths::GetBaseFilename(Obj.Key), ESearchCase::IgnoreCase))
			{
				return;
			}

			UE_LOG(LogIas, Display, TEXT("Fetching TOC '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *Params.Bucket, *Obj.Key);
			FS3GetObjectResponse TocResponse = Client.GetObject(FS3GetObjectRequest
			{
				Params.Bucket,
				Obj.Key
			});

			if (TocResponse.IsOk() == false)
			{
				UE_LOG(LogIas, Warning, TEXT("Failed to fetch TOC '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *Params.Bucket, *Obj.Key);
				return;
			}

			FOnDemandToc Toc;
			FMemoryReaderView Ar(TocResponse.GetBody().GetView());
			Ar << Toc;

			if (Ar.IsError() || Toc.Header.Magic != FOnDemandTocHeader::ExpectedMagic)
			{
				UE_LOG(LogIas, Warning, TEXT("Failed to serialize TOC '%s/%s/%s'. Header version/magic mimsatch"), *Client.GetConfig().ServiceUrl, *Params.Bucket, *Obj.Key);
				return;
			}

			if (!Params.BuildVersion.IsEmpty() && !Params.BuildVersion.Equals(Toc.Meta.BuildVersion, ESearchCase::IgnoreCase))
			{
				return;
			}

			if (!Params.TargetPlatform.IsEmpty() && !Params.TargetPlatform.Equals(Toc.Meta.TargetPlatform, ESearchCase::IgnoreCase))
			{
				return;
			}

			FDateTime DateTime = FDateTime::FromUnixTimestamp(Toc.Meta.EpochTimestamp);
			FTocDescription Desc = FTocDescription 
			{
				.Toc = MoveTemp(Toc),
				.DateTime = DateTime,
				.Key = Obj.Key,
				.Size = Obj.Size
			};

			{
				TUniqueLock Lock(TocMutex);
				Tocs.Add(MoveTemp(Desc));
			}
		});
	}
	else
	{
		UE_LOG(LogIas, Display, TEXT("Fetching TOC '%s/%s'"), *Client.GetConfig().ServiceUrl, *Params.TocKey);
		FS3GetObjectResponse TocResponse = Client.GetObject(FS3GetObjectRequest
		{
			TEXT("/"),
			Params.TocKey
		});

		if (TocResponse.IsOk())
		{
			FOnDemandToc Toc;
			FMemoryReaderView Ar(TocResponse.GetBody().GetView());
			Ar << Toc;

			if (Toc.Header.Magic == FOnDemandTocHeader::ExpectedMagic)
			{
				FDateTime DateTime = FDateTime::FromUnixTimestamp(Toc.Meta.EpochTimestamp);
				Tocs.Add(FTocDescription 
				{
					.Toc = MoveTemp(Toc),
					.DateTime = DateTime,
					.Key = Params.TocKey,
					.Size = TocResponse.GetBody().GetSize()
				});
			}
			else
			{
				UE_LOG(LogIas, Warning, TEXT("Failed to serialize TOC '%s/%s'. Header magic mimsatch"), *Client.GetConfig().ServiceUrl, *Params.TocKey);
			}
		}
	}

	if (Tocs.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::NotFound);
	}

	for (FTocDescription& Desc : Tocs)
	{
		FChunkStats& BuildStats = Stats.FindOrAdd(Desc.Toc.Meta.BuildVersion);
		FChunkStats& BucketStats = Stats.FindOrAdd(Params.Bucket);
		for (const FOnDemandTocContainerEntry& Container : Desc.Toc.Containers)
		{
			for (const FOnDemandTocEntry& Entry : Container.Entries)
			{
				Desc.ChunkCount++;
				Desc.TotalChunkSize += Entry.EncodedSize;
				BucketStats.Add(Entry.Hash, Entry.EncodedSize);
				BuildStats.Add(Entry.Hash, Entry.EncodedSize);
			}
		}
	}

	Tocs.Sort([](const auto& LHS, const auto& RHS) { return LHS.DateTime > RHS.DateTime; });

	if (Params.OutFile.IsEmpty())
	{
		int32 Counter = 1;
		TStringBuilder<256> Url;

		for (const FTocDescription& Desc : Tocs)
		{
			Url.Reset();
			Url << Client.GetConfig().ServiceUrl << TEXT("/") << Params.Bucket << TEXT("/") << Desc.Key;

			UE_LOG(LogIas, Display, TEXT(""));
			UE_LOG(LogIas, Display, TEXT("%d) %s"), Counter++, *Desc.Key);
			UE_LOG(LogIas, Display, TEXT("%-20s: %s"),TEXT("Date"), *Desc.DateTime.ToString());
			UE_LOG(LogIas, Display, TEXT("%-20s: %s"), TEXT("BuildVersion"), *Desc.Toc.Meta.BuildVersion);
			UE_LOG(LogIas, Display, TEXT("%-20s: %s"), TEXT("TargetPlatform"), *Desc.Toc.Meta.TargetPlatform);
			UE_LOG(LogIas, Display, TEXT("%-20s: %.2lf KiB"), TEXT("TocSize"), double(Desc.Size) / 1024.0);
			UE_LOG(LogIas, Display, TEXT("%-20s: %llu"), TEXT("ChunkCount"), Desc.ChunkCount);
			UE_LOG(LogIas, Display, TEXT("%-20s: %.2lf MiB"), TEXT("TotalChunkSize"), double(Desc.TotalChunkSize) / 1024.0 / 1024.0);
			UE_LOG(LogIas, Display, TEXT("%-20s: %s"), TEXT("Url"), Url.ToString());
		}
		UE_LOG(LogIas, Display, TEXT(""));
		
		if (bFilteredQuery == false)
		{
			TArray<FString> Keys;
			Stats.GetKeys(Keys);
			Keys.Sort();

			UE_LOG(LogIas, Display, TEXT("%-80s %15s %15s"), TEXT("BuildVersion"), TEXT("Chunk(s)"), TEXT("MiB"));
			UE_LOG(LogIas, Display, TEXT("-------------------------------------------------------------------------------------------------------------------"));
			for (const FString& Key : Keys)
			{
				if (Key != Params.Bucket)
				{
					const FChunkStats& ChunkStats = Stats.FindChecked(Key);
					UE_LOG(LogIas, Display, TEXT("%-80s %15llu %15.2f"), *Key, ChunkStats.Chunks.Num(), double(ChunkStats.TotalChunkSize) / 1024.0 / 1024.0);
				}
			}
			UE_LOG(LogIas, Display, TEXT("-------------------------------------------------------------------------------------------------------------------"));
			FChunkStats& BucketStats = Stats.FindOrAdd(Params.Bucket);
			UE_LOG(LogIas, Display, TEXT("%-80s %15llu %15.2f"), *Params.Bucket, BucketStats.Chunks.Num(), double(BucketStats.TotalChunkSize) / 1024.0 / 1024.0);
			UE_LOG(LogIas, Display, TEXT(""));
		}
	}
	else
	{
		FString Json;
		FJsonWriter JsonWriter = FJsonWriterFactory::Create(&Json);
		JsonWriter->WriteObjectStart();
		JsonWriter->WriteValue(TEXT("ServiceUrl"), Client.GetConfig().ServiceUrl);
		JsonWriter->WriteValue(TEXT("Bucket"), Params.Bucket);
		JsonWriter->WriteValue(TEXT("BucketPrefix"), Params.BucketPrefix);

		JsonWriter->WriteArrayStart(TEXT("Tocs"));
		for (const FTocDescription& Desc : Tocs)
		{
			JsonWriter->WriteObjectStart();
			JsonWriter->WriteValue(TEXT("Key"), Desc.Key);
			JsonWriter->WriteValue(TEXT("ChunkCount"), Desc.ChunkCount);
			JsonWriter->WriteValue(TEXT("TotalChunkSize"), Desc.TotalChunkSize);

			EOnDemandTocJsonOptions JsonOptions = EOnDemandTocJsonOptions::Header;
			if (Params.bTocEntries)
			{
				JsonOptions |= EOnDemandTocJsonOptions::TocEntries;
			}
			if (Params.bBlockSizes)
			{
				JsonOptions |= EOnDemandTocJsonOptions::BlockSizes;
			}
			if (Params.bBlockHashes)
			{
				JsonOptions |= EOnDemandTocJsonOptions::BlockHashes;
			}
			ToJson(JsonWriter, Desc.Toc, JsonOptions);
			JsonWriter->WriteObjectEnd();
		}
		JsonWriter->WriteArrayEnd();

		JsonWriter->WriteObjectEnd();
		JsonWriter->Close();

		UE_LOG(LogIas, Display, TEXT("Saving file '%s'"), *Params.OutFile);
		if (!FFileHelper::SaveStringToFile(Json, *Params.OutFile))
		{
			return FIoStatus(EIoErrorCode::WriteError, TEXTVIEW("Failed writing JSON file")); 
		}
	}

	if (Params.ChunkKeys.IsEmpty() == false)
	{
		FString Json;
		FJsonWriter JsonWriter = FJsonWriterFactory::Create(&Json);
		
		JsonWriter->WriteObjectStart();
		JsonWriter->WriteValue(TEXT("ServiceUrl"), Client.GetConfig().ServiceUrl);
		JsonWriter->WriteValue(TEXT("Bucket"), Params.Bucket);
		JsonWriter->WriteValue(TEXT("BucketPrefix"), Params.BucketPrefix);

		JsonWriter->WriteArrayStart(TEXT("Tocs"));
		for (const FTocDescription& Desc : Tocs)
		{
			TArray<FString> ObjKeys;
			GetChunkObjectKeys(Desc.Toc, Params.BucketPrefix, ObjKeys);

			JsonWriter->WriteObjectStart();
			JsonWriter->WriteValue(TEXT("Key"), Desc.Key);
			JsonWriter->WriteValue(TEXT("BuildVersion"), Desc.Toc.Meta.BuildVersion);
			JsonWriter->WriteValue(TEXT("TargetPlatform"), Desc.Toc.Meta.TargetPlatform);
			JsonWriter->WriteArrayStart(TEXT("ChunkKeys"));
			for (const FString& Key : ObjKeys)
			{
				JsonWriter->WriteValue(Key);
			}
			JsonWriter->WriteArrayEnd();
			JsonWriter->WriteObjectEnd();
		}
		JsonWriter->WriteArrayEnd();

		JsonWriter->WriteObjectEnd();
		JsonWriter->Close();

		UE_LOG(LogIas, Display, TEXT("Saving chunk key(s) '%s'"), *Params.ChunkKeys);
		if (!FFileHelper::SaveStringToFile(Json, *Params.ChunkKeys))
		{
			return FIoStatus(EIoErrorCode::WriteError, TEXTVIEW("Failed writing JSON file")); 
		}
	}

	return FIoStatus::Ok;
}

#endif // (PLATFORM_DESKTOP && (IS_PROGRAM || WITH_EDITOR))



////////////////////////////////////////////////////////////////////////////////
void FIoStoreOnDemandModule::SetBulkOptionalEnabled(bool bInEnabled)
{
	if (HttpIoDispatcherBackend.IsValid())
	{
		HttpIoDispatcherBackend->SetBulkOptionalEnabled(bInEnabled);
	}
	else
	{
		UE_LOG(LogIas, Log, TEXT("Deferring call to FIoStoreOnDemandModule::SetBulkOptionalEnabled(%s)"), bInEnabled ? TEXT("true") : TEXT("false"));
		DeferredBulkOptionalEnabled = bInEnabled;
	}
}

void FIoStoreOnDemandModule::SetEnabled(bool bInEnabled)
{
	if (HttpIoDispatcherBackend.IsValid())
	{
		HttpIoDispatcherBackend->SetEnabled(bInEnabled);
	}
	else
	{
		UE_LOG(LogIas, Log, TEXT("Deferring call to FIoStoreOnDemandModule::SetEnabled(%s)"), bInEnabled ? TEXT("true") : TEXT("false"));
		DeferredEnabled = bInEnabled;
	}
}

void FIoStoreOnDemandModule::AbandonCache()
{
	if (HttpIoDispatcherBackend.IsValid())
	{
		HttpIoDispatcherBackend->AbandonCache();
	}
	else
	{
		UE_LOG(LogIas, Log, TEXT("Deferring call to FIoStoreOnDemandModule::AbandonCache"));
		DeferredAbandonCache = true;
	}
}

bool FIoStoreOnDemandModule::IsEnabled() const
{
	return HttpIoDispatcherBackend.IsValid()? HttpIoDispatcherBackend->IsEnabled():DeferredAbandonCache.IsSet();
}

void FIoStoreOnDemandModule::ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const
{
	if (HttpIoDispatcherBackend.IsValid())
	{
		HttpIoDispatcherBackend->ReportAnalytics(OutAnalyticsArray);
	}
}

void FIoStoreOnDemandModule::Mount(FOnDemandMountArgs&& Args, FOnDemandMountCompleted&& OnCompleted)
{
	if (IoStore.IsValid() == false)
	{
		IoStore = MakeUnique<FOnDemandIoStore>();
		if (FIoStatus Status = IoStore->Initialize(); !Status.IsOk())
		{
			UE_LOG(LogIas, Error, TEXT("Failed to initialize I/O store on-demand, reason '%s'"), *Status.ToString());
			IoStore.Reset();
			return OnCompleted(TIoStatusOr<FOnDemandMountResult>(Status));
		}
	}

	IoStore->Mount(MoveTemp(Args), MoveTemp(OnCompleted));
}

FIoStatus FIoStoreOnDemandModule::Unmount(FStringView MountId)
{
	if (IoStore.IsValid())
	{
		return IoStore->Unmount(MountId);
	}
	return FIoStatus(EIoErrorCode::InvalidCode, TEXT("I/O store on-demand not initialized"));
}

void FIoStoreOnDemandModule::InitializeInternal()
{
	LLM_SCOPE_BYTAG(Ias);

#if WITH_EDITOR
	bool bEnabledInEditor = false;
	GConfig->GetBool(TEXT("Ias"), TEXT("EnableInEditor"), bEnabledInEditor, GEngineIni);

	if (!bEnabledInEditor)
	{
		return;
	}
#endif //WITH_EDITOR

	const TCHAR* CommandLine = FCommandLine::Get();
	
#if !UE_BUILD_SHIPPING
	if (FParse::Param(CommandLine, TEXT("NoIas")))
	{
		return;
	}
#endif

	LoadCaCerts();

	// Make sure we haven't called initialize before
	check(!HttpIoDispatcherBackend.IsValid());

	FOnDemandEndpointConfig EndpointConfig;
	if (TryParseEndpointConfig(CommandLine, EndpointConfig) == false)
	{
		return;
	}

	if (IoStore.IsValid() == false)
	{
		IoStore = MakeUnique<FOnDemandIoStore>();
		if (FIoStatus Status = IoStore->Initialize(); !Status.IsOk())
		{
			UE_LOG(LogIas, Error, TEXT("Failed to initialize I/O store on demand, reason '%s'"), *Status.ToString());
			return;
		}
	}

	{
		FString EncryptionKey;
		if (FParse::Value(CommandLine, TEXT("Ias.EncryptionKey="), EncryptionKey))
		{
			ApplyEncryptionKeyFromString(EncryptionKey);
		}
	}

	TUniquePtr<IIasCache> Cache;
	FIasCacheConfig CacheConfig = GetIasCacheConfig(CommandLine);
	CacheConfig.DropCache = DeferredAbandonCache.Get(CacheConfig.DropCache);
	if (CacheConfig.DiskQuota > 0)
	{
		if (FPaths::HasProjectPersistentDownloadDir())
		{
			FString CacheDir = FPaths::ProjectPersistentDownloadDir();
			Cache = MakeIasCache(*CacheDir, CacheConfig);
		}
	}
	if (!Cache.IsValid())
	{
		UE_LOG(LogIas, Log, TEXT("File cache disabled - streaming only (%s)"),
			(CacheConfig.DiskQuota > 0) ? TEXT("init-fail") : TEXT("zero-quota"));
	}

	HttpIoDispatcherBackend = MakeOnDemandIoDispatcherBackend(EndpointConfig, *IoStore, MoveTemp(Cache));

	int32 BackendPriority = -10;
#if !UE_BUILD_SHIPPING
	if (FParse::Param(CommandLine, TEXT("Ias")))
	{
		// Bump the priority to be higher then the file system backend
		BackendPriority = 10;
	}
#endif

	// Setup any states changes issued before initialization
	if (DeferredEnabled.IsSet())
	{
		HttpIoDispatcherBackend->SetEnabled(*DeferredEnabled);
	}
	if (DeferredBulkOptionalEnabled.IsSet())
	{
		HttpIoDispatcherBackend->SetBulkOptionalEnabled(*DeferredBulkOptionalEnabled);
	}
	
	FIoDispatcher::Get().Mount(HttpIoDispatcherBackend.ToSharedRef(), BackendPriority);

	bool bUsePerContainerTocsConfigValue = false;
	if (GConfig)
	{
		GConfig->GetBool(TEXT("Ias"), TEXT("UsePerContainerTocs"), bUsePerContainerTocsConfigValue, GEngineIni);
	}
	bool bUsePerContainerTocsParam = false;
#if !UE_BUILD_SHIPPING
	bUsePerContainerTocsParam = FParse::Param(CommandLine, TEXT("Ias.UsePerContainerTocs"));
#endif

	const bool bUsePerContainerTocs = bUsePerContainerTocsConfigValue || bUsePerContainerTocsParam;
	UE_LOG(LogIas, Log, TEXT("Using per container TOCs=%s"), bUsePerContainerTocs ? TEXT("True") : TEXT("False"));

	TOptional<FOnDemandMountArgs> MountArgs;
	if (EndpointConfig.TocFilePath.IsEmpty() == false)
	{
		if (bUsePerContainerTocs == false)
		{
			MountArgs.Emplace(FOnDemandMountArgs
			{
				.MountId = EndpointConfig.TocFilePath,
				.FilePath = EndpointConfig.TocFilePath,
				.Options = EOnDemandMountOptions::StreamOnDemand
			});
		}
	}
	else if (!EndpointConfig.ServiceUrls.IsEmpty() && !EndpointConfig.TocPath.IsEmpty())
	{
		const FString TocUrl = EndpointConfig.ServiceUrls[0] / EndpointConfig.TocPath;
		MountArgs.Emplace(FOnDemandMountArgs
		{
			.MountId = TocUrl,
			.Url = TocUrl,
			.Options = EOnDemandMountOptions::StreamOnDemand
		});
	}

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("Iad")))
	{
		// Temporary switch for testing installation to local storage (IAD)
		MountArgs.Emplace(FOnDemandMountArgs
		{
			.MountId = EndpointConfig.TocFilePath,
			.Url = EndpointConfig.ServiceUrls[0] / EndpointConfig.TocPath,
			.FilePath = EndpointConfig.TocFilePath,
			.Options = EOnDemandMountOptions::Install
		});
	}
#endif

	if (MountArgs)
	{
		IoStore->Mount(
			MoveTemp(MountArgs.GetValue()),
			[](TIoStatusOr<FOnDemandMountResult> MountResult)
			{
				UE_CLOG(!MountResult.IsOk(), LogIas, Error,
					TEXT("Failed to mount TOC, reason '%s'"), *MountResult.Status().ToString());
			});
	}
}
	
void FIoStoreOnDemandModule::StartupModule()
{
#if !UE_IAS_CUSTOM_INITIALIZATION

	if (!GIasSuspendSystem)
	{
		InitializeInternal();
	}
	else
	{
		UE_LOG(LogIas, Display, TEXT("The IoStoreOnDemand module has been remotely disabled by the 'ias.SuspendSystemEnabled' cvar"));
	}

#endif // !UE_IAS_CUSTOM_INITIALIZATION
}

void FIoStoreOnDemandModule::ShutdownModule()
{
}

#if UE_IAS_CUSTOM_INITIALIZATION

EOnDemandInitResult FIoStoreOnDemandModule::Initialize()
{
	if (GIasSuspendSystem)
	{
		UE_LOG(LogIas, Display, TEXT("The IoStoreOnDemand module has been remotely disabled by the 'ias.SuspendSystemEnabled' cvar"));
		return EOnDemandInitResult::Suspended;
	}

	InitializeInternal();

	return HttpIoDispatcherBackend.IsValid() ? EOnDemandInitResult::Success : EOnDemandInitResult::Disabled;
};

#endif // UE_IAS_CUSTOM_INITIALIZATION

} // namespace UE::IoStore

////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(UE::IoStore::FIoStoreOnDemandModule, IoStoreOnDemand);
