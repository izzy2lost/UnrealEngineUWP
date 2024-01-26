// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Model/Mix/MixSettings.h"
#include "Misc/OutputDeviceNull.h"
#include "TG_OutputSettings.generated.h"

class UTG_Expression_Output;
class UTG_Node;
class UTextureGraph;

USTRUCT(BlueprintType)
struct TEXTUREGRAPH_API FTG_OutputSettings 
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(BlueprintReadWrite,EditAnywhere, Category = "Basic", DisplayName = "File Name", Meta = (NoResetToDefault))
		FName BaseName;

	UPROPERTY(EditAnywhere, Category = "Basic", Meta = (NoResetToDefault))
		FName OutputName;

	UPROPERTY(BlueprintReadWrite,EditAnywhere, Category = "Basic", DisplayName = "Path", Meta = (NoResetToDefault))
		FName FolderPath;

	UPROPERTY(BlueprintReadWrite,EditAnywhere, Category = "Advanced", Meta = (NoResetToDefault))
		EResolution Width = EResolution::Auto;

	UPROPERTY(BlueprintReadWrite,EditAnywhere, Category = "Advanced", Meta = (NoResetToDefault))
		EResolution Height = EResolution::Auto;

	UPROPERTY(BlueprintReadWrite,EditAnywhere, Category = "Advanced", DisplayName = "Texture Format", Meta = (NoResetToDefault))
		ETG_TextureFormat TextureFormat = ETG_TextureFormat::BGRA8;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced", DisplayName = "Texture Type", Meta = (NoResetToDefault))
		ETG_TexturePresetType TexturePresetType = ETG_TexturePresetType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced", DisplayName = "LOD Texture Group", Meta = (NoResetToDefault, EditCondition = "TexturePresetType == ETG_TexturePresetType::None"))
		TEnumAsByte<enum TextureGroup> LODGroup;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced", DisplayName = "Compression", Meta = (NoResetToDefault, EditCondition = "TexturePresetType == ETG_TexturePresetType::None") )
		TEnumAsByte <enum TextureCompressionSettings> Compression;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Advanced", DisplayName = "sRGB", Meta = (NoResetToDefault, EditCondition = "TexturePresetType == ETG_TexturePresetType::None"))
		bool bSRGB = false;

	FString GetFullOutputName() { return  FString::Format(TEXT("{0}"), { BaseName.ToString()});}

	bool operator==(const FTG_OutputSettings& Other) const
	{
		return OutputName == Other.OutputName && BaseName == Other.BaseName;
	}

	void InitFromString(const FString& StrVal)
	{
		FOutputDeviceNull NullOut;
		FTG_OutputSettings::StaticStruct()->ImportText(*StrVal, this, /*OwnerObject*/nullptr, 0, &NullOut, FTG_OutputSettings::StaticStruct()->GetName(), /*bAllowNativeOverride*/true);
	}

	FString ToString() const
	{
		FString ExportString;
		FTG_OutputSettings::StaticStruct()->ExportText(ExportString, this, this, /*OwnerObject*/nullptr, /*PortFlags*/0, /*ExportRootScope*/nullptr);
		return ExportString;
	}

	void Set(int InWidth, int InHeight, FString Name = "None", FString Path = "None", ETG_TextureFormat Format = ETG_TextureFormat::BGRA8, ETG_TexturePresetType InTextureType = ETG_TexturePresetType::None,
		TextureCompressionSettings InCompression = TextureCompressionSettings::TC_Default, TextureGroup InLodGroup = TextureGroup::TEXTUREGROUP_World, bool InbSRGB = false);

	void OnSetTexturePresetType(ETG_TexturePresetType Type);
};

USTRUCT()
struct TEXTUREGRAPH_API FTG_OutputExpressionInfo
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FName OutputName;

	UPROPERTY()
	TObjectPtr<UTG_Expression_Output> OutputPtr;

	UPROPERTY()
	bool bExport = true;

	FTG_OutputSettings* GetOutputSettings();

	bool operator==(const FTG_OutputExpressionInfo& Other) const
	{
		return OutputName == Other.OutputName;
	}
};

UCLASS()
class TEXTUREGRAPH_API UTG_OutputSettingsSet : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, EditFixedSize, DisplayName = "Settings" , Category = NoCategory, meta = (HideItemCount, NoResetToDefault, EditFixedOrder, ShowOnlyInnerProperties, FullyExpand, TitleProperty = "OutputName"))
	TArray<FTG_OutputExpressionInfo>	OutputExpressionInfos;

	void InitOutputSettings();
	
	FTG_OutputExpressionInfo* GetOutputExpressionInfo(FName OutputName);

	FTG_OutputSettings* GetOutputSetting(FName OutputName);

	void AddOutputSetting(UTextureGraph* InTextureGraph, FName OutputName,UTG_Expression_Output* Output);

	void RemoveOutputSetting(FName OutputName);

	void RenameOutputSetting(FName OldName,FName NewName);

	void UpdateTitle(UTG_Node* Output);

	EResolution GetMaxWidth();

	EResolution GetMaxHeight();

	int32 GetMaxBufferChannels();

	BufferFormat GetMaxBufferFormat();
};
