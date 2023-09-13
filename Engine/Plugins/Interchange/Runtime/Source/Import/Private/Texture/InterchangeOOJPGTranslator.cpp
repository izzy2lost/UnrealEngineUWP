// Copyright Epic Games, Inc. All Rights Reserved. 
#include "Texture/InterchangeOOJPGTranslator.h"

#include "Algo/Find.h"
#include "Containers/StaticArray.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageCoreUtils.h"
#include "InterchangeImportLog.h"
#include "InterchangeTextureNode.h"
#include "Memory/SharedBuffer.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Texture/TextureTranslatorUtilities.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeOOJPGTranslator)

static bool GInterchangeEnableOOJPGImport = true;
static FAutoConsoleVariableRef CCvarInterchangeEnableOOJPGImport(
	TEXT("Interchange.FeatureFlags.Import.OOJPG"),
	GInterchangeEnableOOJPGImport,
	TEXT("Whether OOJPG support is enabled."),
	ECVF_Default);

TArray<FString> UInterchangeOOJPGTranslator::GetSupportedFormats() const
{
	TArray<FString> Formats;

	if (GInterchangeEnableOOJPGImport || GIsAutomationTesting)
	{
		Formats.Reserve(3);

		Formats.Add(TEXT("ooj;OOJPEG image"));
		bool bEnableOodleJPEG = false;
		GConfig->GetBool(TEXT("TextureImporter"), TEXT("EnableOodleJPEG"), bEnableOodleJPEG, GEditorIni);
		if (bEnableOodleJPEG)
		{
			Formats.Add(TEXT("jpg;JPEG image"));
			Formats.Add(TEXT("jpeg;JPEG image"));
		}
	}

	return Formats;
}

bool UInterchangeOOJPGTranslator::Translate(UInterchangeBaseNodeContainer& BaseNodeContainer) const
{
	return UE::Interchange::FTextureTranslatorUtilities::Generic2DTextureTranslate(GetSourceData(), BaseNodeContainer);
}

TOptional<UE::Interchange::FImportImage> UInterchangeOOJPGTranslator::GetTexturePayloadData(const FString& /*PayloadKey*/, TOptional<FString>& AlternateTexturePath) const
{
	using namespace UE::Interchange;

	TArray64<uint8> SourceDataBuffer;
	if (!FTextureTranslatorUtilities::LoadSourceBuffer(*this, TEXT("OOJPEG"), SourceDataBuffer))
	{
		return {};
	}

	const bool bImportRaw = false;
	return GetTexturePayloadImplementation(MoveTemp(SourceDataBuffer), bImportRaw);

}

bool UInterchangeOOJPGTranslator::SupportCompressedTexturePayloadData() const
{
	return true;
}

TOptional<UE::Interchange::FImportImage> UInterchangeOOJPGTranslator::GetCompressedTexturePayloadData(const FString& /*PayloadKey*/, TOptional<FString>& /*AlternateTexturePath*/) const
{
	using namespace UE::Interchange;

	TArray64<uint8> SourceDataBuffer;
	if (!FTextureTranslatorUtilities::LoadSourceBuffer(*this, TEXT("OOJPEG"), SourceDataBuffer))
	{
		return {};
	}

	const bool bImportRaw = true;
	return GetTexturePayloadImplementation(MoveTemp(SourceDataBuffer), bImportRaw);
}

TOptional<UE::Interchange::FImportImage> UInterchangeOOJPGTranslator::GetTexturePayloadImplementation(TArray64<uint8>&& SourceDataBuffer, bool bShouldImportRaw) const
{
	using namespace UE::Interchange;

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

	//
	// OOJPG
	//
	ETextureSourceCompressionFormat TscfFormat = ETextureSourceCompressionFormat::TSCF_OOJPEG; 
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::OOJPEG);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(SourceDataBuffer.GetData(), SourceDataBuffer.Num()))
	{
		TscfFormat = ETextureSourceCompressionFormat::TSCF_JPEG; 
		ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
		if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(SourceDataBuffer.GetData(), SourceDataBuffer.Num()))
		{
			FTextureTranslatorUtilities::LogError(*this, NSLOCTEXT("InterchangeOOJPGTranslator", "DecodingFailed", "Failed to decode OOJPEG."));
			return TOptional<UE::Interchange::FImportImage>();
		}
	}

	// Select the texture's source format
	ERawImageFormat::Type RawFormat = ImageWrapper->GetClosestRawImageFormat();
	check( RawFormat != ERawImageFormat::Invalid );
	ETextureSourceFormat TextureFormat = FImageCoreUtils::ConvertToTextureSourceFormat(RawFormat);

	UE::Interchange::FImportImage PayloadData;

	const bool bShouldAllocateRawDataBuffer = false;

	PayloadData.Init2DWithParams(
		ImageWrapper->GetWidth(),
		ImageWrapper->GetHeight(),
		TextureFormat,
		ImageWrapper->GetSRGB(),
		bShouldAllocateRawDataBuffer
	);

	TArray64<uint8> RawData;
	if (bShouldImportRaw)
	{
		PayloadData.RawData = MakeUniqueBufferFromArray(ImageWrapper->GetCompressed());
		PayloadData.RawDataCompressionFormat = TscfFormat;
	}
	else if (ImageWrapper->GetRaw(RawData))
	{
		PayloadData.RawData = MakeUniqueBufferFromArray(MoveTemp(RawData));
	}
	else
	{
		FTextureTranslatorUtilities::LogError(*this, NSLOCTEXT("InterchangeOOJPGTranslator", "DecodingFailed", "Failed to decode OOJPEG."));
		return TOptional<UE::Interchange::FImportImage>();
	}

	return PayloadData;
}
