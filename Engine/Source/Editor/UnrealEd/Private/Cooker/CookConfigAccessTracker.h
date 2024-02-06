// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Mutex.h"
#include "Cooker/MPCollector.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/StringView.h"
#include "Interfaces/ITargetPlatform.h"
#include "Misc/ConfigAccessTracking.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageAccessTracking.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/NameTypes.h"

#if UE_WITH_CONFIG_TRACKING

namespace UE::ConfigAccessTracking
{

/** Full path of a FConfigValue that was reported read. */
struct FConfigAccessData
{
	FNameEntryId ConfigPlatform;
	FNameEntryId FileName;
	FNameEntryId SectionName;
	FMinimalName ValueName;
	const ITargetPlatform* RequestingPlatform = nullptr;
	ELoadType LoadType = ELoadType::Uninitialized;

	FName GetConfigPlatform() const { return FName(ConfigPlatform, ConfigPlatform, NAME_NO_NUMBER_INTERNAL); }
	FName GetFileName() const { return FName(FileName, FileName, NAME_NO_NUMBER_INTERNAL); }
	FName GetSectionName() const { return FName(SectionName, SectionName, NAME_NO_NUMBER_INTERNAL); }
	FName GetValueName() const { return FName(ValueName); }

	friend uint32 GetTypeHash(const FConfigAccessData& Data)
	{
		uint32 Hash = static_cast<uint32>(Data.LoadType);
		Hash = HashCombineFast(Hash, Data.ConfigPlatform.ToUnstableInt());
		Hash = HashCombineFast(Hash, Data.FileName.ToUnstableInt());
		Hash = HashCombineFast(Hash, Data.SectionName.ToUnstableInt());
		Hash = HashCombineFast(Hash, GetTypeHash(Data.ValueName));
		Hash = HashCombineFast(Hash, GetTypeHash(Data.RequestingPlatform));
		return Hash;
	}
	bool IsSameConfigFile(const FConfigAccessData& Other) const
	{
		return LoadType == Other.LoadType && ConfigPlatform == Other.ConfigPlatform && FileName == Other.FileName;
	}
	bool operator==(const FConfigAccessData& Other) const
	{
		return LoadType == Other.LoadType && ConfigPlatform == Other.ConfigPlatform &&
			FileName == Other.FileName && SectionName == Other.SectionName && ValueName == Other.ValueName &&
			RequestingPlatform == Other.RequestingPlatform;
	}
	bool operator!=(const FConfigAccessData& Other) const
	{
		return !(*this == Other);
	}
	bool operator<(const FConfigAccessData& Other) const
	{
		if (LoadType != Other.LoadType) return static_cast<uint32>(LoadType) < static_cast<uint32>(Other.LoadType);
		if (ConfigPlatform != Other.ConfigPlatform) return ConfigPlatform.LexicalLess(Other.ConfigPlatform);
		if (FileName != Other.FileName) return FileName.LexicalLess(Other.FileName);
		if (SectionName != Other.SectionName) return SectionName.LexicalLess(Other.SectionName);
		if (ValueName != Other.ValueName) return FName(ValueName).LexicalLess(FName(Other.ValueName));
		if (RequestingPlatform != Other.RequestingPlatform)
		{
			if (RequestingPlatform == nullptr) return true;
			if (Other.RequestingPlatform == nullptr) return false;
			return RequestingPlatform->PlatformName() < Other.RequestingPlatform->PlatformName();
		}
		return false;
	}
};

/** Disables recording of FConfigAccessData reported as accessed on the current thread while in scope. */
struct FIgnoreScope
{
	FIgnoreScope();
	~FIgnoreScope();

	bool bPreviousIgnoreAccess = false;
};

/**
 * Tracker that subscribes to AddConfigValueReadCallback and for each access records the access associated with
 * the package that is currently in scope according to PackageAccessTracking_Private.
 */
class FCookConfigAccessTracker: public FNoncopyable
{
public:
	static FCookConfigAccessTracker& Get() { return Singleton; }

	void Disable();
	bool IsEnabled() const;
	void DumpStats() const;
	/**
	 * Get records requested for the given package and given platform, including RequestingPlatform=nullptr.
	 * Returned records are SORTED by FConfigAccessData::operator<.
	 */
	TArray<UE::ConfigAccessTracking::FConfigAccessData> GetPackageRecords(FName ReferencerPackage,
		const ITargetPlatform* TargetPlatform) const;
	/**
	 * Get records for all requesting packages, including records not associated with a package.
	 * Returned records are SORTED by FConfigAccessData::operator<.
	 */
	TArray<UE::ConfigAccessTracking::FConfigAccessData> GetCookRecords() const;
	/**
	 * Get records requested for all requesting packages, including records not associated with a package,
	 * but filtered by the given TargetPlatform. Includes records requested with no RequestingPlatform.
	 * TargetPlatform==nullptr returns only records requested with no RequestingPlatform.
	 * Returned records are SORTED by FConfigAccessData::operator<.
	 */
	TArray<UE::ConfigAccessTracking::FConfigAccessData> GetCookRecords(const ITargetPlatform* TargetPlatform) const;
	/** Add a record as if requested by the given package, or not associated with a package if PackageName.IsNone(). */
	void AddRecord(FName PackageName, const UE::ConfigAccessTracking::FConfigAccessData& Data);

private:
	FCookConfigAccessTracker();
	virtual ~FCookConfigAccessTracker();

	/** Track object reference reads */
	static void StaticOnConfigValueRead(UE::ConfigAccessTracking::FSection* Section, FMinimalName ValueName,
		const FConfigValue& ConfigValue);

	/** Helper function for GetRecords functions. */
	static void SortRecordsAndFilterByPlatform(TArray<FConfigAccessData>& Records,
		const ITargetPlatform* TargetPlatform);

private:
	// Use a mutex rather than a critical section for synchronization.  Calls into system libraries, such as windows critical section
	// functions, are 50 times more expensive on build farm VMs, radically affecting cook times, which this avoids. 
	mutable UE::FMutex RecordsLock;
	TMap<FName, TSet<UE::ConfigAccessTracking::FConfigAccessData>> PackageRecords;
	UE::ConfigAccessTracking::FConfigValueReadCallbackId OnConfigValueReadCallbackHandle;
	bool bEnabled = false;

private:
	static FCookConfigAccessTracker Singleton;
};

/**
 * Find a ConfigFile by name and ConfigPlatform, either in GConfig or loaded from disk.
 * @param LoadType LoadType describing which ConfigCacheIni method was used to load the ConfigFile
 * @param ConfigPlatform PlatformName used with the ConfigCacheIni method that loaded the ConfigFile, or
 *        nullptr/emptystring for platform-agnostic
 * @param Filename Short ('Engine') or long ('../../../QAGame/Config/DefaultEngine.ini') name of the ConfigFile
 * @param Buffer FConfigFile Buffer that will hold the result if LoadConfigFile was called
 * @param The discovered configfile, or nullptr.
 */
const FConfigFile* FindOrLoadConfigFile(ELoadType LoadType, const TCHAR* ConfigPlatform, const TCHAR* Filename,
	FConfigFile& Buffer);

/** Return whether LoadType is a type that can be loaded by FindOrLoadConfigFile. */
bool IsLoadableLoadType(ELoadType LoadType);

} // namespace UE::ConfigAccessTracking

/** Convert ELoadType -> text */
const TCHAR* LexToString(UE::ConfigAccessTracking::ELoadType LoadType);

/** Convert text -> ELoadType */
void LexFromString(UE::ConfigAccessTracking::ELoadType& OutLoadType, FStringView Text);

namespace UE::ConfigAccessTracking
{

/** CookMultiprocess collector for ConfigAccess data. */
class FConfigAccessTrackingCollector : public UE::Cook::IMPCollector
{
public:
	virtual FGuid GetMessageType() const override { return MessageType; }
	virtual const TCHAR* GetDebugName() const override { return TEXT("FConfigAccessTrackingCollector"); }

	virtual void ClientTick(UE::Cook::FMPCollectorClientTickContext& Context) override;
	virtual void ServerReceiveMessage(UE::Cook::FMPCollectorServerMessageContext& Context,
		FCbObjectView Message) override;

	static FGuid MessageType;
};

} // namespace UE::ConfigAccessTracking

#else // !UE_WITH_CONFIG_TRACKING

namespace UE::ConfigAccessTracking
{

struct FIgnoreScope
{
	FIgnoreScope() {}
	~FIgnoreScope() {}
};

} // namespace UE::ConfigAccessTracking

#endif // else !UE_WITH_CONFIG_TRACKING
