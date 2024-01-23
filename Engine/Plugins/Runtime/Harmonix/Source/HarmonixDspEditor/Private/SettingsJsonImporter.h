// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "JsonImporterHelper.h"

class FJsonObject;

class FSettingsJsonImporter : public FJsonImporter
{
public:

	static bool TryParseJson(TSharedPtr<FJsonObject> JsonObj, struct FKeyzoneSettings& OutSettings);

	static bool TryParseJson(TSharedPtr<FJsonObject> JsonObj, struct FPannerDetails& OutPanDetails);

	static bool TryParseJson(TSharedPtr<FJsonObject> JsonObj, struct FTimeStretchConfig& TimeStretchConfig);

	static bool TryParseJson(TSharedPtr<FJsonObject> JsonObj, struct FFusionPatchSettings& PatchSettings);

	static bool TryParseJson(TSharedPtr<FJsonObject> JsonObj, struct FAdsrSettings& AdsrSettings);

	static bool TryParseJson(TSharedPtr<FJsonObject> JsonObj, struct FLfoSettings& LfoSettings);

	static bool TryParseJson(TSharedPtr<FJsonObject> JsonObj, struct FBiquadFilterSettings& FilterSettings);

	static bool TryParseJson(TSharedPtr<FJsonObject> JsonObj, struct FModulatorSettings& ModulatorSettings);

	static bool TryParseJson(TSharedPtr<FJsonObject> JsonObj, struct FDelaySettings& DelaySettings);

	static bool TryParseJson(TSharedPtr<FJsonObject> JsonObj, struct FBitCrusherSettings& BitCrusherSettings);

	static bool TryParseJson(TSharedPtr<FJsonObject> JsonObj, struct FVocoderSettings& VocoderSettings);

	static bool TryParseJson(TSharedPtr<FJsonObject> JsonObj, struct FDistortionSettingsV1& DistortionSettings);

	static bool TryParseJson(TSharedPtr<FJsonObject> JsonObj, struct FDistortionFilterSettings& DistortionFilterSettings);

	static bool TryParseJson(TSharedPtr<FJsonObject> JsonObj, struct FPortamentoSettings& PortamentoSettings);
};