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
	FFusionPatchSettings& DefaultSettings = FusionPatchData.Settings;
	FAdsrSettings& VolumeAdsr = DefaultSettings.Adsrs.Volume();
	VolumeAdsr.Target = EAdsrTarget::Volume;
	VolumeAdsr.Depth = 1.0f;
	VolumeAdsr.IsEnabled = true;
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

	for (FKeyzoneSettings& Keyzone : FusionPatchData.Keyzones)
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

	if (Ar.IsLoading())
	{
		if (Version < FFusionPatchCustomVersion::PitchShifterNameRedirects)
		{
			UE_LOG(LogFusionPatch, Warning, TEXT("Fusion patch was loaded with an outdated version. Asset requires reimport: %s"), *GetPathName());
		}

		if (Version < FFusionPatchCustomVersion::DeprecatedPresets)
		{
			if (FusionPatchData.Presets_DEPRECATED.IsValidIndex(0))
			{
				FusionPatchData.Settings = FusionPatchData.Presets_DEPRECATED[0];
			}

			if (FusionPatchData.Presets_DEPRECATED.Num() > 1)
			{
				UE_LOG(LogFusionPatch, Warning, TEXT("Fusion patch (%s) has more than one preset, but presets have been deprecated. Only the first (Default) preset will be loaded"), *GetPathName());
			}
		}

		if (Version < FFusionPatchCustomVersion::DeprecatedUnusedEffectsSettings)
		{
			if (FusionPatchData.Settings.Delay_DEPRECATED.IsEnabled)
			{
				UE_LOG(LogFusionPatch, Warning, TEXT("Fusion Patch (%s) has the \"Delay\" effect enabled, but that effect is no longer supported by the Fusion Sampler! "
					"If you intended to use the Delay effect, rework your metasound to apply the Delay to the output of the FusionSampler node using this patch"),
					*GetPathName());
			}

			if (FusionPatchData.Settings.Distortion_DEPRECATED.IsEnabled)
			{
				UE_LOG(LogFusionPatch, Warning, TEXT("Fusion Patch (%s) has the \"Distortion\" effect enabled, but that effect is no longer supported by the Fusion Sampler! "
					"If you intended to use the Distortion effect, rework your metasound to apply the Distortion to the output of the FusionSampler node using this patch"),
					*GetPathName());
			}

			if (FusionPatchData.Settings.BitCrusher_DEPRECATED.IsEnabled)
			{
				UE_LOG(LogFusionPatch, Warning, TEXT("Fusion Patch (%s) has the \"BitCrusher\" effect enabled, but that effect is no longer supported by the Fusion Sampler! "
					"If you intended to use the BitCrusher effect, rework your metasound to apply the Distortion to the output of the FusionSampler node using this patch"),
					*GetPathName());
			}

			if (FusionPatchData.Settings.Vocoder_DEPRECATED.IsEnabled)
			{
				UE_LOG(LogFusionPatch, Warning, TEXT("Fusion Patch (%s) has the \"Vocoder\" effect enabled, but that effect is no longer supported by the Fusion Sampler! "
					"If you intended to use the Vocoder effect, rework your metasound to apply the Vocoder to the output of the FusionSampler node using this patch"),
					*GetPathName());	
			}
		}
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

void FFusionPatchData::InitProxyData(const Audio::FProxyDataInitParams& InitParams)
{
	for (auto& Keyzone : Keyzones)
	{
		Keyzone.InitProxyData(InitParams);
	}
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