// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Delegates/DelegateCombinations.h"
#include "Engine/DeveloperSettings.h"
#include "MetasoundFrontendDocument.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/PerPlatformProperties.h"

#include "MetasoundSettings.generated.h"


// Forward Declarations
struct FMetasoundFrontendClassName;
struct FPropertyChangedChainEvent;

#if WITH_EDITORONLY_DATA
namespace Metasound::Engine
{
	DECLARE_MULTICAST_DELEGATE(FOnSettingsDefaultConformed);
	DECLARE_MULTICAST_DELEGATE(FOnPageSettingsUpdated);
} // namespace Metasound::Engine
#endif // WITH_EDITORONLY_DATA

UENUM()
enum class EMetaSoundMessageLevel : uint8
{
	Error,
	Warning,
	Info
};

USTRUCT()
struct METASOUNDENGINE_API FDefaultMetaSoundAssetAutoUpdateSettings
{
	GENERATED_BODY()

	/** MetaSound to prevent from AutoUpdate. */
	UPROPERTY(EditAnywhere, Category = "AutoUpdate", meta = (AllowedClasses = "/Script/MetasoundEngine.MetaSound, /Script/MetasoundEngine.MetaSoundSource"))
	FSoftObjectPath MetaSound;
};

UCLASS(Hidden)
class METASOUNDENGINE_API UMetaSoundQualityHelper : public UObject
{
	GENERATED_BODY()

public:
	/**
	* Returns a list of quality settings to present to a combobox
	* */
	UFUNCTION(meta = (DeprecatedFunction, DeprecationMessage = "Use UMetaSoundSettings::GetQualityList instead"))
	static TArray<FName> GetQualityList() { return { }; };
};

USTRUCT()
struct METASOUNDENGINE_API FMetaSoundPageSettings
{
	GENERATED_BODY()
	
	UPROPERTY()
	FGuid UniqueId;

	/** Name of this page's setting. This will appear in the MetaSound Asset Editor's 'Page Selector'.
		The names should be unique and adequately describe the Entry. "High", "Low" etc. **/
	UPROPERTY(EditAnywhere, Category = "Pages")
	FName Name;

#if WITH_EDITORONLY_DATA
	// When true, page data defined on serialized MetaSounds are included in cooked build (for the assigned platform(s)).
	UPROPERTY(EditAnywhere, config, Category = "Pages")
	FPerPlatformBool IsCooked = true;
#endif //WITH_EDITORONLY_DATA
};

USTRUCT()
struct METASOUNDENGINE_API FMetaSoundQualitySettings
{
	GENERATED_BODY()
	
	/** A hidden GUID that will be generated once when adding a new entry. This prevents orphaning of renamed entries. **/
	UPROPERTY()
	FGuid UniqueId;

	/** Name of this quality setting. This will appear in the quality dropdown list.
		The names should be unique but are not guaranteed to be (use guid for unique match) **/
	UPROPERTY(EditAnywhere, Category = "Quality")
	FName Name;

	/** Sample Rate (in Hz). NOTE: A Zero value will have no effect and use the Device Rate. **/
	UPROPERTY(EditAnywhere, config, Category = "Quality", meta = (ClampMin = "0", ClampMax="96000"))
	FPerPlatformInt SampleRate = 0;

	/** Block Rate (in Hz). NOTE: A Zero value will have no effect and use the Default (100)  **/
	UPROPERTY(EditAnywhere, config, Category = "Quality", meta = (ClampMin = "0", ClampMax="1000"))
	FPerPlatformFloat BlockRate = 0.f;
};


UCLASS(config = MetaSound, defaultconfig, meta = (DisplayName = "MetaSounds"))
class METASOUNDENGINE_API UMetaSoundSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** If true, AutoUpdate is enabled, increasing load times.  If false, skips AutoUpdate on load, but can result in MetaSounds failing to load, 
	  * register, and execute if interface differences are present. */
	UPROPERTY(EditAnywhere, config, Category = AutoUpdate)
	bool bAutoUpdateEnabled = true;

	/** List of native MetaSound classes whose node references should not be AutoUpdated. */
	UPROPERTY(EditAnywhere, config, Category = AutoUpdate, meta = (DisplayName = "DenyList", EditCondition = "bAutoUpdateEnabled"))
	TArray<FMetasoundFrontendClassName> AutoUpdateDenylist;

	/** List of MetaSound assets whose node references should not be AutoUpdated. */
	UPROPERTY(EditAnywhere, config, Category = AutoUpdate, meta = (DisplayName = "Asset DenyList", EditCondition = "bAutoUpdateEnabled"))
	TArray<FDefaultMetaSoundAssetAutoUpdateSettings> AutoUpdateAssetDenylist;

	/** If true, warnings will be logged if updating a node results in existing connections being discarded. */
	UPROPERTY(EditAnywhere, config, Category = AutoUpdate, meta = (DisplayName = "Log Warning on Dropped Connection", EditCondition = "bAutoUpdateEnabled"))
	bool bAutoUpdateLogWarningOnDroppedConnection = true;

	/** Directories to scan & automatically register MetaSound post initial asset scan on engine start-up.
	  * May speed up subsequent calls to playback MetaSounds post asset scan but increases application load time.
	  * See 'MetaSoundAssetSubsystem::RegisterAssetClassesInDirectories' to dynamically register or 
	  * 'MetaSoundAssetSubsystem::UnregisterAssetClassesInDirectories' to unregister asset classes.
	  */
	UPROPERTY(EditAnywhere, config, Category = Registration, meta = (RelativePath, LongPackageName))
	TArray<FDirectoryPath> DirectoriesToRegister;
		
	UPROPERTY(Transient)
	int32 DenyListCacheChangeID = 0;

private:
#if WITH_EDITORONLY_DATA
	Metasound::Engine::FOnSettingsDefaultConformed OnDefaultConformed;
	Metasound::Engine::FOnPageSettingsUpdated OnPageSettingsUpdated;
#endif //WITH_EDITORONLY_DATA

	/** Page Name to target when attempting to execute MetaSound. If target page is not implemented (or cooked in a runtime build)
	  * for the active platform, uses order of cooked pages (see 'Page Settings' for order) falling back to lower index-ordered page
	  * implemented in MetaSound asset. */
	UPROPERTY(EditAnywhere, config, Category = Pages)
	FName TargetPageName = Metasound::Frontend::DefaultPageName;

	/** Array of possible page settings that can be added to a MetaSound object. Order
	  * defines default fallback logic whereby a higher index-ordered page
	  * implemented in a MetaSound asset is higher priority (see 'Target Page').
	  */
	UPROPERTY(EditAnywhere, config, Category = Pages)
	TArray<FMetaSoundPageSettings> PageSettings;

	/** Array of possible quality settings for Metasounds to chose from */
	UPROPERTY(EditAnywhere, config, Category = Quality)
	TArray<FMetaSoundQualitySettings> QualitySettings;

public:
	// Returns the page settings with the provided name. If there are multiple settings
	// with the same name, selection within the duplicates is undefined.
	const FMetaSoundPageSettings* FindPageSettings(FName Name) const;

	// Returns the page settings with the unique ID given.
	const FMetaSoundPageSettings* FindPageSettings(const FGuid& InPageID) const;

	// Returns the quality settings with the provided name. If there are multiple settings
	// with the same name, selection within the duplicates is undefined.
	const FMetaSoundQualitySettings* FindQualitySettings(FName Name) const;

	// Returns the quality settings with the unique ID given.
	const FMetaSoundQualitySettings* FindQualitySettings(const FGuid& InQualityID) const;

	// Returns the target page name.
	const FName& GetTargetPageName() const { return TargetPageName; }

	// Returns the target page ID.
	const FGuid& GetTargetPageID() const;

	const TArray<FMetaSoundPageSettings>& GetPageSettings() const { return PageSettings; }
	const TArray<FMetaSoundQualitySettings>& GetQualitySettings() const { return QualitySettings; }

#if WITH_EDITORONLY_DATA
	Metasound::Engine::FOnSettingsDefaultConformed& GetOnDefaultConformedDelegate();
	Metasound::Engine::FOnPageSettingsUpdated& GetOnPageSettingsUpdatedDelegate();

	static FName GetPageSettingPropertyName();
	static FName GetQualitySettingPropertyName();
#endif // WITH_EDITORONLY_DATA

	// Sets the target page to the given name. Returns true if associated page settings were found
	// and target set, false if not found and not set.
	bool SetTargetPage(FName PageName);

#if WITH_EDITOR
public:
	/* Returns a list of quality settings to present to a combobox. Ex:
	 * UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(GetOptions="MetasoundEngine.MetaSoundSettings.GetQualityList"))
	 * FName QualitySetting;
	*/
	UFUNCTION()
	static TArray<FName> GetQualityList();

private:
	void ConformPageSettingsDefault(bool bNotifyDefaultConformed);

	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostInitProperties() override;
#endif // WITH_EDITOR
};
