// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixDsp/FusionSampler/FusionPatch.h"
#include "HarmonixDsp/FusionSampler/FusionPatchCustomVersion.h"
#include "HarmonixDsp/FusionSampler/SingletonFusionVoicePool.h"
#include "HarmonixDsp/StretcherAndPitchShifterFactoryConfig.h"
#include "EditorFramework/AssetImportData.h"

#include "Harmonix/PropertyUtility.h"

#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/UnrealType.h"

#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "Misc/DataValidation.h"

DEFINE_LOG_CATEGORY_STATIC(LogFusionPatch, Log, All)


TSharedPtr<Audio::IProxyData> UFusionPatch::CreateProxyData(const Audio::FProxyDataInitParams& InitParams)
{
	if (!RenderableFusionPatchData)
	{
		RenderableFusionPatchData = MakeShared<FFusionPatchDataProxy::QueueType>(FusionPatchData);

		(*RenderableFusionPatchData)->InitProxyData(InitParams);
	}
	return MakeShared<FFusionPatchDataProxy>(RenderableFusionPatchData);
}

UFusionPatch::UFusionPatch() : Super()
{
	FusionPatchData.CurrentPresetIndex = 0;
	FFusionPatchSettings& DefaultSettings = FusionPatchData.Presets.Emplace_GetRef();
	FAdsrSettings& VolumeAdsr = DefaultSettings.Adsrs.Volume;
	VolumeAdsr.Target = EAdsrTarget::Volume;
	VolumeAdsr.Depth = 1.0f;
	VolumeAdsr.IsEnabled = true;
}

void UFusionPatch::SetCurrentPresetIndex(int32 Index)
{
	FusionPatchData.SetCurrentPresetIndex(Index);

	UpdateRenderableForNonTrivialChange();
}

void UFusionPatch::AddPreset(FFusionPatchSettings& Preset)
{
	FusionPatchData.AddPreset(Preset);

	UpdateRenderableForNonTrivialChange();
}

void UFusionPatch::UpdatePreset(int32 Index, FFusionPatchSettings& Preset)
{
	FusionPatchData.UpdatePreset(Index, Preset);

	UpdateRenderableForNonTrivialChange();
}

void UFusionPatch::UpdateRenderableForNonTrivialChange()
{
	if (!RenderableFusionPatchData)
	{
		return;
	}

	Audio::FProxyDataInitParams InitParams;

	auto NewSettings = FFusionPatchDataProxy::QueueType::SharedNodePtrType::CreateSharedRenderable(FusionPatchData);
	NewSettings->InitProxyData(InitParams);
	RenderableFusionPatchData->SetNewSettings(NewSettings);
}

#if WITH_EDITORONLY_DATA

void UFusionPatch::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UFusionPatch::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedChainEvent);

	// we only care about changes to FusionPatchData
	const FProperty* MemberChanged = PropertyChangedChainEvent.PropertyChain.GetHead()->GetValue();
	if (MemberChanged->GetFName() != GET_MEMBER_NAME_CHECKED(UFusionPatch, FusionPatchData))
	{
		return;
	}

	for (FKeyzoneSettings& Keyzone : FusionPatchData.GetKeyzones())
	{
		int8 MinNote = Keyzone.MinNote;
		int8 MaxNote = Keyzone.MaxNote;
		int8 MinVelocity = Keyzone.MinVelocity;
		int8 MaxVelocity = Keyzone.MaxVelocity;
		
		//clamp note numbers:
		//MinNote <= MaxNote
		if (MinNote > MaxNote)
		{
			Keyzone.MinNote = MaxNote;
		}

		//clamp velocities
		//MinVelocity < MaxVelocity
 		if (MinVelocity > MaxVelocity)
		{
			Keyzone.MinVelocity = MaxVelocity;
		}
	}

	// don't need to do anything if there isn't a RenderableFusionPatchData to copy data to
	if (!RenderableFusionPatchData)
	{
		return;
	}

	const FProperty* PropertyChanged = PropertyChangedChainEvent.Property;
	const EPropertyChangeType::Type PropertyChangeType = PropertyChangedChainEvent.ChangeType;
	Harmonix::EPostEditAction PostEditAction = Harmonix::GetPropertyPostEditAction(PropertyChanged, PropertyChangeType);

	if (PostEditAction == Harmonix::EPostEditAction::UpdateTrivial)
	{
		FFusionPatchData* CopyToStruct = *RenderableFusionPatchData;
		Harmonix::CopyStructProperty(CopyToStruct, &FusionPatchData, PropertyChangedChainEvent);
	}
	else if (PostEditAction == Harmonix::EPostEditAction::UpdateNonTrivial)
	{
		UpdateRenderableForNonTrivialChange();
	}

	// otherwise, do nothing
}

void UFusionPatch::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && !AssetImportData)
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}

	Super::PostInitProperties();
}

void UFusionPatch::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	if (AssetImportData)
	{
		Context.AddTag(FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden));
	}
}

#endif // WITH_EDITOR_ONLY_DATA

#if WITH_EDITOR

EDataValidationResult UFusionPatch::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const int32 Version = GetLinkerCustomVersion(FFusionPatchCustomVersion::GUID);

	if (Version < FFusionPatchCustomVersion::LatestVersion)
	{
		Context.AddWarning(FText::Format(INVTEXT("Asset saved with outdated version: {0}. Resave asset to latest version: {1}."),
			Version, FFusionPatchCustomVersion::LatestVersion));

		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

#endif // WITH_EDITOR

void UFusionPatch::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFusionPatchCustomVersion::GUID);
	const int32 Version = Ar.CustomVer(FFusionPatchCustomVersion::GUID);

	if (Ar.IsLoading() && Version < FFusionPatchCustomVersion::PitchShifterNameRedirects)
	{
		UE_LOG(LogFusionPatch, Warning, TEXT("Fusion patch was loaded with an outdated version. Asset requires reimport: %s"), *GetPathName());
	}

	const UStretcherAndPitchShifterFactoryConfig* FactoryConfig = GetDefault<UStretcherAndPitchShifterFactoryConfig>();
	for (FKeyzoneSettings& Keyzone : FusionPatchData.Keyzones)
	{
		if (const FPitchShifterNameRedirect* Redirect = FactoryConfig->FindFactoryNameRedirect(Keyzone.TimeStretchConfig.PitchShifter))
		{
			Keyzone.TimeStretchConfig.PitchShifter = Redirect->NewName;
		}
	}
}

void UFusionPatch::PostLoad()
{
	Super::PostLoad();
}

TArray<FKeyzoneSettings>& FFusionPatchData::GetKeyzones()
{
	return Keyzones;
}

const TArray<FKeyzoneSettings>& FFusionPatchData::GetKeyzones() const
{
	return Keyzones;
}

int32 FFusionPatchData::GetCurrentPresetIndex() const
{
	return CurrentPresetIndex;
}

void FFusionPatchData::SetCurrentPresetIndex(int32 Index)
{
	ensureAlwaysMsgf(Presets.Num() > 0, TEXT("Fusion Patch Data presets have not been initialized"));
	ensureAlwaysMsgf(Presets.IsValidIndex(Index), TEXT("assigning Fusion Patch Data to an invalid preset"));
	CurrentPresetIndex = FMath::Clamp(Index, 0, Presets.Num());
}

int32 FFusionPatchData::GetNumPresets() const
{
	return Presets.Num();
}

void FFusionPatchData::InitProxyData(const Audio::FProxyDataInitParams& InitParams)
{
	for (auto& Keyzone : Keyzones)
	{
		Keyzone.InitProxyData(InitParams);
	}
}

void FFusionPatchData::AddPreset(const FFusionPatchSettings& Preset)
{
	// There shouldn't be that many presets, so this should be a pretty fast search
	int32 Index = FindPresetIndex(Preset.Name);

	// replace the preset of the same name with the new preset
	if (Index != INDEX_NONE)
	{
		Presets[Index] = Preset;
	}
	else
	{
		Presets.Add(Preset);
	}
}

int32 FFusionPatchData::FindPresetIndex(FName PresetName) const
{
	return Presets.IndexOfByPredicate([&PresetName](const FFusionPatchSettings& Elem)
		{
			return Elem.Name == PresetName;
		});
}

void FFusionPatchData::UpdatePreset(int32 Index, const FFusionPatchSettings& Preset)
{
	check(Presets.IsValidIndex(Index));
	Presets[Index] = Preset;
}

void FFusionPatchData::DisconnectSampler(const FFusionSampler* Sampler)
{
	for (FKeyzoneSettings& Keyzone : Keyzones)
	{
		if (Keyzone.SingletonFusionVoicePool.IsValid())
		{
			Keyzone.SingletonFusionVoicePool->SamplerDisconnecting(Sampler);
		}
	}
}

bool FFusionPatchData::HasPreset(FName PresetName) const
{
	return Presets.ContainsByPredicate([&PresetName](const FFusionPatchSettings& Elem)
		{
			return Elem.Name == PresetName;
		});
}

const FFusionPatchSettings& FFusionPatchData::GetPreset(int32 Index) const
{
	check(Presets.IsValidIndex(Index));
	return Presets[Index];
}

FFusionPatchSettings& FFusionPatchData::GetPreset(int32 Index)
{
	check(Presets.IsValidIndex(Index));
	return Presets[Index];
}

const FFusionPatchSettings& FFusionPatchData::GetCurrentPreset() const
{
	check(Presets.Num() > 0);
	check(Presets.IsValidIndex(CurrentPresetIndex));
	return Presets[GetCurrentPresetIndex()];
}

FFusionPatchSettings& FFusionPatchData::GetCurrentPreset()
{
	check(Presets.Num() > 0);
	check(Presets.IsValidIndex(CurrentPresetIndex));
	return Presets[CurrentPresetIndex];
}