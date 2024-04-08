// Copyright Epic Games, Inc. All Rights Reserved.

#include "TargetDomain/TargetDomainUtils.h"

#include "Algo/BinarySearch.h"
#include "Algo/IsSorted.h"
#include "Algo/Sort.h"
#include "Algo/Unique.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Cooker/CookConfigAccessTracker.h"
#include "Cooker/CookDependency.h"
#include "Cooker/PackageBuildDependencyTracker.h"
#include "CookOnTheSide/CookLog.h"
#include "DerivedDataBuildDefinition.h"
#include "DerivedDataBuildKey.h"
#include "DerivedDataSharedString.h"
#include "EditorDomain/EditorDomain.h"
#include "EditorDomain/EditorDomainUtils.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Hash/Blake3.h"
#include "IO/IoDispatcher.h"
#include "IO/IoHash.h"
#include "Misc/App.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/StringBuilder.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Serialization/PackageWriter.h"
#include "ZenStoreHttpClient.h"

namespace UE::TargetDomain
{

constexpr uint32 CookDependenciesVersion = 0x00000002;
static const FUtf8StringView CookDependenciesAttachmentKey = UTF8TEXTVIEW("CookDependencies");
static const FUtf8StringView BuildDefinitionsAttachmentKey = UTF8TEXTVIEW("BuildDefinitionsAttachmentKey");
/**
 * Reads / writes an oplog for EditorDomain BuildDefinitionLists.
 * TODO: Reduce duplication between this class and FZenStoreWriter
 */
class FEditorDomainOplog
{
public:
	FEditorDomainOplog();

	bool IsValid() const;
	void CommitPackage(FName PackageName, TArrayView<IPackageWriter::FCommitAttachmentInfo> Attachments);
	FCbObject GetOplogAttachment(FName PackageName, FUtf8StringView AttachmentKey);

private:
	struct FOplogEntry
	{
		struct FAttachment
		{
			const UTF8CHAR* Key;
			FIoHash Hash;
		};

		TArray<FAttachment> Attachments;
	};

	void InitializeRead();
	
	FCbAttachment CreateAttachment(FSharedBuffer AttachmentData);
	FCbAttachment CreateAttachment(FCbObject AttachmentData)
	{
		return CreateAttachment(AttachmentData.GetBuffer().ToShared());
	}

	static void StaticInit();
	static bool IsReservedOplogKey(FUtf8StringView Key);

	UE::FZenStoreHttpClient HttpClient;
	FCriticalSection Lock;
	TMap<FName, FOplogEntry> Entries;
	bool bConnectSuccessful = false;
	bool bInitializedRead = false;

	static TArray<const UTF8CHAR*> ReservedOplogKeys;
};
TUniquePtr<FEditorDomainOplog> GEditorDomainOplog;

// Constructor/Destructor defined here in cpp rather than header so we can 
// avoid needing the definition of FCookDependency in the header; it is needed
// for construct/destruct of TArray<FCookDependency>.
FCookDependencies::FCookDependencies() = default;
FCookDependencies::~FCookDependencies() = default;
FCookDependencies::FCookDependencies(const FCookDependencies&) = default;
FCookDependencies::FCookDependencies(FCookDependencies&&) = default;
FCookDependencies& FCookDependencies::operator=(const FCookDependencies&) = default;
FCookDependencies& FCookDependencies::operator=(FCookDependencies&&) = default;


bool FCookDependencies::IsValid() const
{
	return bValid;
}

bool FCookDependencies::HasKeyMatch()
{
	if (!bValid)
	{
		return false;
	}
	if (StoredKey.IsZero())
	{
		return false;
	}
	if (CurrentKey.IsZero())
	{
		if (!TryCalculateCurrentKey())
		{
			return false;
		}
	}
	return CurrentKey == StoredKey;
}

bool FCookDependencies::TryCalculateCurrentKey(FString* OutErrorMessage)
{
	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (PackageName.IsNone())
	{
		if (OutErrorMessage) *OutErrorMessage = TEXT("PackageName is not set.");
		return false;
	}
	if (!AssetRegistry)
	{
		if (OutErrorMessage) *OutErrorMessage = TEXT("AssetRegistry is unavailable.");
		return false;
	}
	FEditorDomain* EditorDomain = FEditorDomain::Get();
	if (!EditorDomain)
	{
		if (OutErrorMessage) *OutErrorMessage = TEXT("EditorDomain is unavailable.");
		return false;
	}
	FBlake3 KeyBuilder;
	UE::EditorDomain::FPackageDigest PackageDigest = EditorDomain->GetPackageDigest(PackageName);
	if (!PackageDigest.IsSuccessful())
	{
		if (OutErrorMessage) *OutErrorMessage = PackageDigest.GetStatusString();
		return false;
	}

	KeyBuilder.Update(&PackageDigest.Hash, sizeof(PackageDigest.Hash));

	for (FName PackageDependency : BuildPackageDependencies)
	{
		PackageDigest = EditorDomain->GetPackageDigest(PackageDependency);
		if (!PackageDigest.IsSuccessful())
		{
			if (OutErrorMessage)
			{
				*OutErrorMessage = FString::Printf(TEXT("Could not create PackageDigest for %s: %s"),
					*PackageDependency.ToString(), *PackageDigest.GetStatusString());
			}
			return false;
		}
		KeyBuilder.Update(&PackageDigest.Hash, sizeof(PackageDigest.Hash));
	}

	if (!ConfigDependencies.IsEmpty())
	{
#if UE_WITH_CONFIG_TRACKING
		using namespace UE::ConfigAccessTracking;
		FCookConfigAccessTracker& ConfigTracker = FCookConfigAccessTracker::Get();
#endif
		for (const FString& ConfigDependency : ConfigDependencies)
		{
			FString Value;
#if UE_WITH_CONFIG_TRACKING
			Value = ConfigTracker.GetValue(ConfigDependency);
#endif
			uint8 Marker = 0;
			KeyBuilder.Update(&Marker, sizeof(Marker));
			if (!Value.IsEmpty())
			{
				KeyBuilder.Update(*Value, Value.Len() * sizeof(Value[0]));
			}
		}
	}

	if (!CookDependencies.IsEmpty())
	{
		bool bError = false;
		UE::Cook::FCookDependencyContext Context(&KeyBuilder, [&bError, OutErrorMessage](FString&& ErrorMessage)
			{
				if (OutErrorMessage)
				{
					if (bError)
					{
						*OutErrorMessage += TEXT("\n");
						*OutErrorMessage += ErrorMessage;
					}
					else
					{
						*OutErrorMessage = MoveTemp(ErrorMessage);
					}
				}
				bError = true;
			});

		for (UE::Cook::FCookDependency& CookDependency : CookDependencies)
		{
			CookDependency.UpdateHash(Context);
		}
		if (bError)
		{
			return false;
		}
	}

	if (OutErrorMessage) OutErrorMessage->Reset();
	CurrentKey = KeyBuilder.Finalize();
	return true;
}

void FCookDependencies::Reset()
{
	BuildPackageDependencies.Reset();
	ConfigDependencies.Reset();
	RuntimePackageDependencies.Reset();
	PackageName = FName();
	StoredKey = FIoHash::Zero;
	CurrentKey = FIoHash::Zero;
	bValid = false;
}

void FCookDependencies::Empty()
{
	Reset();
	BuildPackageDependencies.Empty();
	ConfigDependencies.Empty();
	RuntimePackageDependencies.Empty();
}

FCookDependencies FCookDependencies::Collect(UPackage* Package, const ITargetPlatform* TargetPlatform,
	FSavePackageResultStruct* SaveResult, TArray<FName>&& RuntimeDependencies, FString* OutErrorMessage)
{
	TStringBuilder<256> StringBuffer;
	FName TransientPackageName = GetTransientPackage()->GetFName();
	auto IsTransientPackageName = [&StringBuffer, TransientPackageName](FName InPackageName)
		{
			if (InPackageName == TransientPackageName)
			{
				return true;
			}
			InPackageName.ToString(StringBuffer);
			return FPackageName::IsMemoryPackage(StringBuffer) ||
				FPackageName::IsScriptPackage(StringBuffer);
		};

	if (!Package)
	{
		if (OutErrorMessage) *OutErrorMessage = TEXT("Invalid null package.");
		return FCookDependencies();
	}
	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		if (OutErrorMessage) *OutErrorMessage = TEXT("AssetRegistry is unavailable.");
		return FCookDependencies();
	}
	FEditorDomain* EditorDomain = FEditorDomain::Get();
	if (!EditorDomain)
	{
		if (OutErrorMessage) *OutErrorMessage = TEXT("EditorDomain is unavailable.");
		return FCookDependencies();
	}

	FCookDependencies Result;
	Result.PackageName = Package->GetFName();
	TSet<FName> BuildDependenciesSet;

	TArray<FName> AssetDependencies;
	AssetRegistry->GetDependencies(Result.PackageName, AssetDependencies,
		UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Game);
	RuntimeDependencies.Append(MoveTemp(AssetDependencies));
	RuntimeDependencies.RemoveAllSwap(IsTransientPackageName, EAllowShrinking::No);
	RuntimeDependencies.Sort(FNameLexicalLess());
	RuntimeDependencies.SetNum(Algo::Unique(RuntimeDependencies), EAllowShrinking::Yes);

	FPackageBuildDependencyTracker& Tracker = FPackageBuildDependencyTracker::Get();

#if UE_WITH_PACKAGE_ACCESS_TRACKING
	if (Tracker.IsEnabled())
	{
		TArray<FBuildDependencyAccessData> AccessDatas = Tracker.GetAccessDatas(Result.PackageName);

		BuildDependenciesSet.Reserve(AccessDatas.Num());
		for (FBuildDependencyAccessData& AccessData : AccessDatas)
		{
			if (AccessData.TargetPlatform == TargetPlatform || AccessData.TargetPlatform == nullptr)
			{
				BuildDependenciesSet.Add(AccessData.ReferencedPackage);
			}
		}
	}
	else
#endif
	{
		// Defensively treat all asset dependencies as both build and runtime dependencies
		BuildDependenciesSet.Append(AssetDependencies);
	}

	Result.BuildPackageDependencies = BuildDependenciesSet.Array();
	Result.BuildPackageDependencies.RemoveAllSwap(IsTransientPackageName, EAllowShrinking::Yes);
	Result.BuildPackageDependencies.Sort(FNameLexicalLess());

	Result.RuntimePackageDependencies = MoveTemp(RuntimeDependencies);

#if UE_WITH_CONFIG_TRACKING
	{
		using namespace UE::ConfigAccessTracking;
		FCookConfigAccessTracker& ConfigTracker = FCookConfigAccessTracker::Get();
		if (ConfigTracker.IsEnabled())
		{
			TArray<FConfigAccessData> ConfigKeys = ConfigTracker.GetPackageRecords(Result.PackageName, TargetPlatform);
			Result.ConfigDependencies.Reserve(ConfigKeys.Num());
			for (const FConfigAccessData& ConfigKey : ConfigKeys)
			{
				Result.ConfigDependencies.Add(ConfigKey.FullPathToString());
			}
		}
	}
#endif
	if (SaveResult)
	{
		Result.CookDependencies = MoveTemp(SaveResult->CookDependencies);
		Algo::Sort(Result.CookDependencies);
	}

	if (!Result.TryCalculateCurrentKey(OutErrorMessage))
	{
		return FCookDependencies();
	}
	Result.StoredKey = Result.CurrentKey;
	Result.bValid = true;

	if (OutErrorMessage) OutErrorMessage->Reset();
	return Result;
}

}

bool LoadFromCompactBinary(FCbObjectView ObjectView, UE::TargetDomain::FCookDependencies& Dependencies)
{
	using namespace UE::TargetDomain;

	Dependencies.Reset();
	int32 Version = -1;

	for (FCbFieldViewIterator FieldView(ObjectView.CreateViewIterator()); FieldView; )
	{
		const FCbFieldViewIterator Last = FieldView;
		if (FieldView.GetName().Equals(UTF8TEXTVIEW("Version")))
		{
			Version = FieldView.AsInt32();
			if ((FieldView++).HasError() || Version != CookDependenciesVersion)
			{
				return false;
			}
		}
		if (FieldView.GetName().Equals(UTF8TEXTVIEW("StoredKey")))
		{
			if (!LoadFromCompactBinary(FieldView++, Dependencies.StoredKey))
			{
				return false;
			}
		}
		if (FieldView.GetName().Equals(UTF8TEXTVIEW("BuildPackageDependencies")))
		{
			if (!LoadFromCompactBinary(FieldView++, Dependencies.BuildPackageDependencies))
			{
				return false;
			}
		}
		if (FieldView.GetName().Equals(UTF8TEXTVIEW("ConfigDependencies")))
		{
			if (!LoadFromCompactBinary(FieldView++, Dependencies.ConfigDependencies))
			{
				return false;
			}
		}
		if (FieldView.GetName().Equals(UTF8TEXTVIEW("RuntimePackageDependencies")))
		{
			if (!LoadFromCompactBinary(FieldView++, Dependencies.RuntimePackageDependencies))
			{
				return false;
			}
		}
		if (FieldView.GetName().Equals(UTF8TEXTVIEW("CookDependencies")))
		{
			if (!LoadFromCompactBinary(FieldView++, Dependencies.CookDependencies))
			{
				return false;
			}
		}
		if (FieldView == Last)
		{
			++FieldView;
		}
	}
	if (Version == -1)
	{
		return false;
	}
	Dependencies.bValid = true;
	return true;
}

FCbWriter& operator<<(FCbWriter& Writer, const UE::TargetDomain::FCookDependencies& CookDependencies)
{
	using namespace UE::TargetDomain;

	Writer.BeginObject();
	Writer << "Version" << CookDependenciesVersion;
	Writer << "StoredKey" << CookDependencies.StoredKey;
	if (!CookDependencies.BuildPackageDependencies.IsEmpty())
	{
		Writer << "BuildPackageDependencies" << CookDependencies.BuildPackageDependencies;
	}
	if (!CookDependencies.ConfigDependencies.IsEmpty())
	{
		Writer << "ConfigDependencies" << CookDependencies.ConfigDependencies;
	}
	if (!CookDependencies.RuntimePackageDependencies.IsEmpty())
	{
		Writer << "RuntimePackageDependencies" << CookDependencies.RuntimePackageDependencies;
	}
	if (!CookDependencies.CookDependencies.IsEmpty())
	{
		Writer << "CookDependencies" << CookDependencies.CookDependencies;
	}

	Writer.EndObject();
	return Writer;
}

namespace UE::TargetDomain
{

FBuildDefinitionList FBuildDefinitionList::Collect(UPackage* Package, const ITargetPlatform* TargetPlatform,
	FString* OutErrorMessage)
{
	using namespace UE::DerivedData;

	FBuildDefinitionList Result;

	// TODO_BuildDefinitionList: Calculate and store BuildDefinitionList on the PackageData, or collect it here from some other source.
	if (Result.Definitions.IsEmpty())
	{
		if (OutErrorMessage) *OutErrorMessage = TEXT("Not yet implemented");
		return FBuildDefinitionList();
	}

	TArray<FBuildDefinition>& Defs = Result.Definitions;
	Algo::Sort(Defs, [](const FBuildDefinition& A, const FBuildDefinition& B)
		{
			return A.GetKey().Hash < B.GetKey().Hash;
		});

	if (OutErrorMessage) OutErrorMessage->Reset();
	return Result;
}

void FBuildDefinitionList::Reset()
{
	Definitions.Reset();
}

void FBuildDefinitionList::Empty()
{
	Definitions.Empty();
}

}

bool LoadFromCompactBinary(FCbObject&& Object, UE::TargetDomain::FBuildDefinitionList& Definitions)
{
	using namespace UE::DerivedData;

	FCbField DefinitionsField = Object["BuildDefinitions"];
	FCbArray DefinitionsArrayField = DefinitionsField.AsArray();
	if (DefinitionsField.HasError())
	{
		return false;
	}
	TArray<FBuildDefinition>& Defs = Definitions.Definitions;
	Defs.Empty(DefinitionsArrayField.Num());
	for (FCbField& BuildDefinitionObj : DefinitionsArrayField)
	{
		FOptionalBuildDefinition BuildDefinition = FBuildDefinition::Load(TEXTVIEW("TargetDomainBuildDefinitionList"),
			BuildDefinitionObj.AsObject());
		if (!BuildDefinition)
		{
			Defs.Empty();
			return false;
		}
		Defs.Add(MoveTemp(BuildDefinition).Get());
	}

	return true;
}

FCbWriter& operator<<(FCbWriter& Writer, const UE::TargetDomain::FBuildDefinitionList& Definitions)
{
	using namespace UE::DerivedData;

	Writer.BeginObject();
	Writer.BeginArray("BuildDefinitions");
	for (const FBuildDefinition& BuildDefinition : Definitions.Definitions)
	{
		BuildDefinition.Save(Writer);
	}
	Writer.EndArray();
	return Writer;
}

namespace UE::TargetDomain
{

void FCookAttachments::Reset()
{
	Dependencies.Reset();
	BuildDefinitions.Reset();
}

void FCookAttachments::Empty()
{
	Dependencies.Empty();
	BuildDefinitions.Empty();
}

bool TryCollectAndStoreCookDependencies(UPackage* Package, const ITargetPlatform* TargetPlatform,
	FSavePackageResultStruct* SaveResult, TArray<FName>&& RuntimeDependencies,
	IPackageWriter::FCommitAttachmentInfo& OutResult)
{
	FString ErrorMessage;
	FCookDependencies CookDependencies = FCookDependencies::Collect(Package, TargetPlatform, SaveResult, 
		MoveTemp(RuntimeDependencies), &ErrorMessage);
	if (!CookDependencies.IsValid())
	{
		// CookPackageSplitterTODO: This error occurs for generated packages. Need to register them with EditorDomain.
#if 0
		UE_LOG(LogCook, Error, TEXT("Could not collect CookDependencies for package '%s': %s"),
			*Package->GetName(), *ErrorMessage);
#endif
		OutResult.Value = FCbObject();
		return false;
	}

	FCbWriter Writer;
	Writer << CookDependencies;
	OutResult.Key = CookDependenciesAttachmentKey;
	OutResult.Value = Writer.Save().AsObject();
	return true;
}

bool TryCollectAndStoreBuildDefinitionList(UPackage* Package, const ITargetPlatform* TargetPlatform,
	IPackageWriter::FCommitAttachmentInfo& OutResult)
{
	FBuildDefinitionList Definitions = FBuildDefinitionList::Collect(Package, TargetPlatform);
	if (Definitions.Definitions.IsEmpty())
	{
		OutResult.Value = FCbObject();
		return false;
	}

	FCbWriter Writer;
	Writer << Definitions;
	OutResult.Key = BuildDefinitionsAttachmentKey;
	OutResult.Value = Writer.Save().AsObject();
	return true;
}

void FCookAttachments::Fetch(TArrayView<FName> PackageNames, const ITargetPlatform* TargetPlatform,
	ICookedPackageWriter* PackageWriter,
	TUniqueFunction<void(FName PackageName, FCookAttachments&& Result)>&& Callback)
{
	for (FName PackageName : PackageNames)
	{
		FCbObject DependenciesObj;
		FCbObject BuildDefinitionsObj;
		if (TargetPlatform)
		{
			check(PackageWriter);
			DependenciesObj = PackageWriter->GetOplogAttachment(PackageName, CookDependenciesAttachmentKey);
			BuildDefinitionsObj = PackageWriter->GetOplogAttachment(PackageName, CookDependenciesAttachmentKey);
		}
		else
		{
			if (!GEditorDomainOplog)
			{
				Callback(PackageName, FCookAttachments());
				continue;
			}
			DependenciesObj = GEditorDomainOplog->GetOplogAttachment(PackageName, CookDependenciesAttachmentKey);
			BuildDefinitionsObj = GEditorDomainOplog->GetOplogAttachment(PackageName, BuildDefinitionsAttachmentKey);
		}

		FCookAttachments Result;
		if (LoadFromCompactBinary(DependenciesObj, Result.Dependencies))
		{
			Result.Dependencies.PackageName = PackageName;
		}
		LoadFromCompactBinary(MoveTemp(BuildDefinitionsObj), Result.BuildDefinitions);

		Callback(PackageName, MoveTemp(Result));
	}
}

bool IsIterativeEnabled(FName PackageName, bool bAllowAllClasses)
{
	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		return false;
	}
	TOptional<FAssetPackageData> PackageDataOpt = AssetRegistry->GetAssetPackageDataCopy(PackageName);
	if (!PackageDataOpt)
	{
		return false;
	}
	FAssetPackageData& PackageData = *PackageDataOpt;

	if (!bAllowAllClasses)
	{
		auto LogInvalidDueTo = [](FName PackageName, FName ClassPath)
			{
				UE_LOG(LogEditorDomain, Verbose, TEXT("NonIterative Package %s due to %s"), *PackageName.ToString(), *ClassPath.ToString());
			};

		UE::EditorDomain::FClassDigestMap& ClassDigests = UE::EditorDomain::GetClassDigests();
		FReadScopeLock ClassDigestsScopeLock(ClassDigests.Lock);
		for (FName ClassName : PackageData.ImportedClasses)
		{
			FTopLevelAssetPath ClassPath(WriteToString<256>(ClassName).ToView());
			UE::EditorDomain::FClassDigestData* ExistingData = nullptr;
			if (ClassPath.IsValid())
			{
				ExistingData = ClassDigests.Map.Find(ClassPath);
			}
			if (!ExistingData)
			{
				// !ExistingData -> !allowed, because caller has already called CalculatePackageDigest, so all
				// existing classes in the package have been added to ClassDigests.
				LogInvalidDueTo(PackageName, ClassName);
				return false;
			}
			if (!ExistingData->bNative)
			{
				// TODO: We need to add a way to mark non-native classes (there can be many of them) as allowed or denied.
				// Currently we are allowing them all, so long as their closest native is allowed. But this is not completely
				// safe to do, because non-native classes can add constructionevents that e.g. use the Random function.
				ExistingData = ClassDigests.Map.Find(ExistingData->ClosestNative);
				if (!ExistingData)
				{
					LogInvalidDueTo(PackageName, ClassName);
					return false;
				}
			}
			if (!ExistingData->bTargetIterativeEnabled)
			{
				LogInvalidDueTo(PackageName, ClassName);
				return false;
			}
		}
	}
	return true;
}

TArray<const UTF8CHAR*> FEditorDomainOplog::ReservedOplogKeys;

FEditorDomainOplog::FEditorDomainOplog()
#if UE_WITH_ZEN
: HttpClient(TEXT("localhost"), UE::Zen::FZenServiceInstance::GetAutoLaunchedPort() > 0 ? UE::Zen::FZenServiceInstance::GetAutoLaunchedPort() : 8558)
#else
: HttpClient(TEXT("localhost"), 8558)
#endif
{
	StaticInit();

	FString ProjectId = FApp::GetZenStoreProjectId();
	FString OplogId = TEXT("EditorDomain");

	FString RootDir = FPaths::RootDir();
	FString EngineDir = FPaths::EngineDir();
	FPaths::NormalizeDirectoryName(EngineDir);
	FString ProjectDir = FPaths::ProjectDir();
	FPaths::NormalizeDirectoryName(ProjectDir);
	FString ProjectPath = FPaths::GetProjectFilePath();
	FPaths::NormalizeFilename(ProjectPath);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString AbsServerRoot = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*RootDir);
	FString AbsEngineDir = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*EngineDir);
	FString AbsProjectDir = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*ProjectDir);
	FString ProjectFilePath = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*ProjectPath);

#if UE_WITH_ZEN
	if (UE::Zen::IsDefaultServicePresent())
	{
		bool IsLocalConnection = HttpClient.GetZenServiceInstance().IsServiceRunningLocally();
		HttpClient.TryCreateProject(ProjectId, OplogId, AbsServerRoot, AbsEngineDir, AbsProjectDir, IsLocalConnection ? ProjectFilePath : FStringView());
		HttpClient.TryCreateOplog(ProjectId, OplogId, TEXT("") /*InOplogMarkerFile*/, false /* bFullBuild */);
	}
#endif
}

void FEditorDomainOplog::InitializeRead()
{
	if (bInitializedRead)
	{
		return;
	}
	UE_LOG(LogEditorDomain, Display, TEXT("Fetching EditorDomain oplog..."));

	TFuture<FIoStatus> FutureOplogStatus = HttpClient.GetOplog().Next([this](TIoStatusOr<FCbObject> OplogStatus)
		{
			if (!OplogStatus.IsOk())
			{
				return OplogStatus.Status();
			}

			FCbObject Oplog = OplogStatus.ConsumeValueOrDie();

			for (FCbField& EntryObject : Oplog["entries"])
			{
				FUtf8StringView PackageName = EntryObject["key"].AsString();
				if (PackageName.IsEmpty())
				{
					continue;
				}
				FName PackageFName(PackageName);
				FOplogEntry& Entry = Entries.FindOrAdd(PackageFName);
				Entry.Attachments.Empty();

				for (FCbFieldView Field : EntryObject)
				{
					FUtf8StringView FieldName = Field.GetName();
					if (IsReservedOplogKey(FieldName))
					{
						continue;
					}
					if (Field.IsHash())
					{
						const UTF8CHAR* AttachmentId = UE::FZenStoreHttpClient::FindOrAddAttachmentId(FieldName);
						Entry.Attachments.Add({ AttachmentId, Field.AsHash() });
					}
				}
				Entry.Attachments.Shrink();
				check(Algo::IsSorted(Entry.Attachments, [](const FOplogEntry::FAttachment& A, const FOplogEntry::FAttachment& B)
					{
						return FUtf8StringView(A.Key).Compare(FUtf8StringView(B.Key), ESearchCase::IgnoreCase) < 0;
					}));
			}

			return FIoStatus::Ok;
		});
	FutureOplogStatus.Get();
	bInitializedRead = true;
}

FCbAttachment FEditorDomainOplog::CreateAttachment(FSharedBuffer AttachmentData)
{
	FCompressedBuffer CompressedBuffer = FCompressedBuffer::Compress(AttachmentData);
	check(!CompressedBuffer.IsNull());
	return FCbAttachment(CompressedBuffer);
}

void FEditorDomainOplog::StaticInit()
{
	if (ReservedOplogKeys.Num() > 0)
	{
		return;
	}

	ReservedOplogKeys.Append({ UTF8TEXT("key") });
	Algo::Sort(ReservedOplogKeys, [](const UTF8CHAR* A, const UTF8CHAR* B)
		{
			return FUtf8StringView(A).Compare(FUtf8StringView(B), ESearchCase::IgnoreCase) < 0;
		});;
}

bool FEditorDomainOplog::IsReservedOplogKey(FUtf8StringView Key)
{
	int32 Index = Algo::LowerBound(ReservedOplogKeys, Key,
		[](const UTF8CHAR* Existing, FUtf8StringView Key)
		{
			return FUtf8StringView(Existing).Compare(Key, ESearchCase::IgnoreCase) < 0;
		});
	return Index != ReservedOplogKeys.Num() &&
		FUtf8StringView(ReservedOplogKeys[Index]).Equals(Key, ESearchCase::IgnoreCase);
}

bool FEditorDomainOplog::IsValid() const
{
	return HttpClient.IsConnected();
}

void FEditorDomainOplog::CommitPackage(FName PackageName, TArrayView<IPackageWriter::FCommitAttachmentInfo> Attachments)
{
	FScopeLock ScopeLock(&Lock);

	FCbPackage Pkg;

	TArray<FCbAttachment, TInlineAllocator<2>> CbAttachments;
	int32 NumAttachments = Attachments.Num();
	FOplogEntry& Entry = Entries.FindOrAdd(PackageName);
	Entry.Attachments.Empty(NumAttachments);
	if (NumAttachments)
	{
		TArray<const IPackageWriter::FCommitAttachmentInfo*, TInlineAllocator<2>> SortedAttachments;
		SortedAttachments.Reserve(NumAttachments);
		for (const IPackageWriter::FCommitAttachmentInfo& AttachmentInfo : Attachments)
		{
			SortedAttachments.Add(&AttachmentInfo);
		}
		SortedAttachments.Sort([](const IPackageWriter::FCommitAttachmentInfo& A, const IPackageWriter::FCommitAttachmentInfo& B)
			{
				return A.Key.Compare(B.Key, ESearchCase::IgnoreCase) < 0;
			});
		CbAttachments.Reserve(NumAttachments);
		for (const IPackageWriter::FCommitAttachmentInfo* AttachmentInfo : SortedAttachments)
		{
			const FCbAttachment& CbAttachment = CbAttachments.Add_GetRef(CreateAttachment(AttachmentInfo->Value));
			check(!IsReservedOplogKey(AttachmentInfo->Key));
			Pkg.AddAttachment(CbAttachment);
			Entry.Attachments.Add(FOplogEntry::FAttachment{
				UE::FZenStoreHttpClient::FindOrAddAttachmentId(AttachmentInfo->Key), CbAttachment.GetHash() });
		}
	}

	FCbWriter PackageObj;
	FString PackageNameKey = PackageName.ToString();
	PackageNameKey.ToLowerInline();
	PackageObj.BeginObject();
	PackageObj << "key" << PackageNameKey;
	for (int32 Index = 0; Index < NumAttachments; ++Index)
	{
		FCbAttachment& CbAttachment = CbAttachments[Index];
		FOplogEntry::FAttachment& EntryAttachment = Entry.Attachments[Index];
		PackageObj << EntryAttachment.Key << CbAttachment;
	}
	PackageObj.EndObject();

	FCbObject Obj = PackageObj.Save().AsObject();
	Pkg.SetObject(Obj);
	HttpClient.AppendOp(Pkg);
}

// Note that this is destructive - we yank out the buffer memory from the 
// IoBuffer into the FSharedBuffer
FSharedBuffer IoBufferToSharedBuffer(FIoBuffer& InBuffer)
{
	InBuffer.EnsureOwned();
	const uint64 DataSize = InBuffer.DataSize();
	uint8* DataPtr = InBuffer.Release().ValueOrDie();
	return FSharedBuffer{ FSharedBuffer::TakeOwnership(DataPtr, DataSize, FMemory::Free) };
};

FCbObject FEditorDomainOplog::GetOplogAttachment(FName PackageName, FUtf8StringView AttachmentKey)
{
	FScopeLock ScopeLock(&Lock);
	InitializeRead();

	FOplogEntry* Entry = Entries.Find(PackageName);
	if (!Entry)
	{
		return FCbObject();
	}

	const UTF8CHAR* AttachmentId = UE::FZenStoreHttpClient::FindAttachmentId(AttachmentKey);
	if (!AttachmentId)
	{
		return FCbObject();
	}
	FUtf8StringView AttachmentIdView(AttachmentId);

	int32 AttachmentIndex = Algo::LowerBound(Entry->Attachments, AttachmentIdView,
		[](const FOplogEntry::FAttachment& Existing, FUtf8StringView AttachmentIdView)
		{
			return FUtf8StringView(Existing.Key).Compare(AttachmentIdView, ESearchCase::IgnoreCase) < 0;
		});
	if (AttachmentIndex == Entry->Attachments.Num())
	{
		return FCbObject();
	}
	const FOplogEntry::FAttachment& Existing = Entry->Attachments[AttachmentIndex];
	if (!FUtf8StringView(Existing.Key).Equals(AttachmentIdView, ESearchCase::IgnoreCase))
	{
		return FCbObject();
	}
	TIoStatusOr<FIoBuffer> BufferResult = HttpClient.ReadOpLogAttachment(WriteToString<48>(Existing.Hash));
	if (!BufferResult.IsOk())
	{
		return FCbObject();
	}
	FIoBuffer Buffer = BufferResult.ValueOrDie();
	if (Buffer.DataSize() == 0)
	{
		return FCbObject();
	}

	FSharedBuffer SharedBuffer = IoBufferToSharedBuffer(Buffer);
	return FCbObject(SharedBuffer);
}

void CommitEditorDomainCookAttachments(FName PackageName, TArrayView<IPackageWriter::FCommitAttachmentInfo> Attachments)
{
	if (!GEditorDomainOplog)
	{
		return;
	}
	GEditorDomainOplog->CommitPackage(PackageName, Attachments);
}

void CookInitialize()
{
	bool bCookAttachmentsEnabled = true;
	GConfig->GetBool(TEXT("EditorDomain"), TEXT("CookAttachmentsEnabled"), bCookAttachmentsEnabled, GEditorIni);
	if (bCookAttachmentsEnabled)
	{
		GEditorDomainOplog = MakeUnique<FEditorDomainOplog>();
		if (!GEditorDomainOplog->IsValid())
		{
			UE_LOG(LogEditorDomain, Display, TEXT("Failed to connect to ZenServer; EditorDomain oplog is unavailable."));
			GEditorDomainOplog.Reset();
		}
	}
}


} // namespace UE::TargetDomain
