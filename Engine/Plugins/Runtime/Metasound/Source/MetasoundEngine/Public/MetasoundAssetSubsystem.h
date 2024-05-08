// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "MetasoundAssetBase.h"
#include "MetasoundAssetManager.h"
#include "MetasoundBuilderBase.h"
#include "Subsystems/EngineSubsystem.h"
#include "UObject/Object.h"

#include "MetasoundAssetSubsystem.generated.h"

// Forward Declarations
class UAssetManager;
class FMetasoundAssetBase;

struct FDirectoryPath;
struct FMetaSoundFrontendDocumentBuilder;


USTRUCT(BlueprintType)
struct METASOUNDENGINE_API FMetaSoundAssetDirectory
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Directories, meta = (RelativePath, LongPackageName))
	FDirectoryPath Directory;
};

/** Contains info of assets which are currently async loading. */
USTRUCT()
struct FMetaSoundAsyncAssetDependencies
{
	GENERATED_BODY()

	// ID of the async load
	int32 LoadID = 0;

	// Parent MetaSound 
	UPROPERTY(Transient)
	TObjectPtr<UObject> MetaSound;

	// Dependencies of parent MetaSound
	TArray<FSoftObjectPath> Dependencies;

	// Handle to in-flight streaming request
	TSharedPtr<FStreamableHandle> StreamableHandle;
};

namespace Metasound::Engine
{
	void DeinitializeAssetManager();
	void InitializeAssetManager();
} // namespace Metasound::Engine

/** DEPRECATED: Subsystem in charge of the MetaSound asset registry */
UCLASS()
class METASOUNDENGINE_API UMetaSoundAssetSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	using FAssetInfo = Metasound::Frontend::IMetaSoundAssetManager::FAssetInfo;

	virtual void Initialize(FSubsystemCollectionBase& InCollection) override;

	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	virtual void RemoveAsset(const UObject& InObject);

	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	virtual void RemoveAsset(const FAssetData& InAssetData);

	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	virtual void RenameAsset(const FAssetData& InAssetData, bool bInReregisterWithFrontend = true);

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	virtual void AddAssetReferences(FMetasoundAssetBase& InAssetBase);
#endif

	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	Metasound::Frontend::FNodeRegistryKey AddOrUpdateAsset(const FAssetData& InAssetData);

	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	virtual Metasound::Frontend::FNodeRegistryKey AddOrUpdateAsset(const UObject& InObject);

	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	virtual bool CanAutoUpdate(const FMetasoundFrontendClassName& InClassName) const;

	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	virtual bool ContainsKey(const Metasound::Frontend::FNodeRegistryKey& InRegistryKey) const;

	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	virtual const FSoftObjectPath* FindObjectPathFromKey(const Metasound::Frontend::FNodeRegistryKey& RegistryKey) const;

	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	virtual FMetasoundAssetBase* GetAsAsset(UObject& InObject) const;

	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	virtual const FMetasoundAssetBase* GetAsAsset(const UObject& InObject) const;

	UE_DEPRECATED(5.5, "Implementation of MetaSound asset management has been moved to raw c++ implementation for more reliable, monolithic "
		"lifetime management. This subsystem continues to exist only for Blueprint-related asset functionality. "
		"Use IMetaSoundAssetManager::GetChecked() instead")
	static UMetaSoundAssetSubsystem& GetChecked();

#if WITH_EDITOR
	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	virtual TSet<FAssetInfo> GetReferencedAssetClasses(const FMetasoundAssetBase& InAssetBase) const;
#endif

	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	virtual FMetasoundAssetBase* TryLoadAsset(const FSoftObjectPath& InObjectPath) const;

	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	virtual FMetasoundAssetBase* TryLoadAssetFromKey(const Metasound::Frontend::FNodeRegistryKey& RegistryKey) const;

	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	virtual bool TryLoadReferencedAssets(const FMetasoundAssetBase& InAssetBase, TArray<FMetasoundAssetBase*>& OutReferencedAssets) const;

	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	virtual void RequestAsyncLoadReferencedAssets(FMetasoundAssetBase& InAssetBase) { }

	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	virtual void WaitUntilAsyncLoadReferencedAssetsComplete(FMetasoundAssetBase& InAssetBase) { }

	UFUNCTION(BlueprintCallable, Category = "MetaSounds|Registration")
	void RegisterAssetClassesInDirectories(const TArray<FMetaSoundAssetDirectory>& Directories);

	UFUNCTION(BlueprintCallable, Category = "MetaSounds|Registration")
	void UnregisterAssetClassesInDirectories(const TArray<FMetaSoundAssetDirectory>& Directories);

protected:
	UE_DEPRECATED(5.5, "Moved to private implementation")
	void PostEngineInit() { }

	UE_DEPRECATED(5.5, "Moved to private implementation")
	void PostInitAssetScan() { }

	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	void RebuildDenyListCache(const UAssetManager& InAssetManager) { }

	UE_DEPRECATED(5.5, "Use FMetaSoundDocumentBuilder::SetDisplayName instead (call now only available with editor compiled)")
	void ResetAssetClassDisplayName(const FAssetData& InAssetData) { }

	UE_DEPRECATED(5.5, "Moved to internal implementation, use IMetaSoundAssetManager::GetChecked() and analogous call")
	void SearchAndIterateDirectoryAssets(const TArray<FDirectoryPath>& InDirectories, TFunctionRef<void(const FAssetData&)> InFunction) { }

private:
	void PostEngineInitInternal();
	void PostInitAssetScanInternal();
};
