// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImageSequenceUtils.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImgMediaSource.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"


bool FImageSequenceUtils::GetImageSequencePathAndFiles(const UImgMediaSource* InImgSequence, FString& OutFullSequencePath, TArray<FString>& OutImageFiles)
{
	if (InImgSequence == nullptr)
	{
		return false;
	}

	OutFullSequencePath = InImgSequence->GetFullPath();

	return GetImageSequencePathAndFiles(OutFullSequencePath, OutImageFiles);
}

bool FImageSequenceUtils::GetImageSequencePathAndFiles(const FString& InFullSequencePath, TArray<FString>& OutImageFiles)
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	IFileManager& FileManager = IFileManager::Get();

	bool bIterateResult = FileManager.IterateDirectory(*InFullSequencePath, [&OutImageFiles, &ImageWrapperModule](const TCHAR* InFilenameOrDirectory, bool bInIsDirectory)
	{
		if (!bInIsDirectory)
		{
			EImageFormat Format = ImageWrapperModule.GetImageFormatFromExtension(*FPaths::GetExtension(InFilenameOrDirectory));
			if (Format != EImageFormat::Invalid)
			{
				OutImageFiles.Add(FPaths::GetCleanFilename(InFilenameOrDirectory));
			}
		}

		return true;
	});

	return !OutImageFiles.IsEmpty() && bIterateResult;
}

bool FImageSequenceUtils::GetImageSequenceInfo(const class UImgMediaSource* InImgSequence, FIntVector2& OutDimensions, int32& OutNumImages)
{
	if (InImgSequence == nullptr)
	{
		return false;
	}

	return GetImageSequenceInfo(InImgSequence->GetFullPath(), OutDimensions, OutNumImages);
}

bool FImageSequenceUtils::GetImageSequenceInfo(const FString& InFullSequencePath, FIntVector2& OutDimensions, int32& OutNumImages)
{
	TArray<FString> ImageFiles;
	bool bFoundImages = GetImageSequencePathAndFiles(InFullSequencePath, ImageFiles);

	if (!bFoundImages)
	{
		return false;
	}

	OutNumImages = ImageFiles.Num();

	const FString SampleImagePath = InFullSequencePath / ImageFiles[0];

	TArray<uint8> RawFileData;
	if (FFileHelper::LoadFileToArray(RawFileData, *SampleImagePath))
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(RawFileData.GetData(), RawFileData.Num());
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);

		if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(RawFileData.GetData(), RawFileData.Num()))
		{
			OutDimensions.X = ImageWrapper->GetWidth();
			OutDimensions.Y = ImageWrapper->GetHeight();
			return true;
		}
	}

	return false;
}

bool FImageSequenceUtils::GetTrackingFilePathAndInfo(const class UImgMediaSource* InImgSequence, FString& OutTrackingFilePath, int32& OutFrameOffset, int32& OutNumFrames)
{
	return GetTrackingFilePathAndInfo(InImgSequence->GetFullPath(), OutTrackingFilePath, OutFrameOffset, OutNumFrames);
}

bool FImageSequenceUtils::GetTrackingFilePathAndInfo(const FString& InFullSequencePath, FString& OutTrackingFilePath, int32& OutFrameOffset, int32& OutNumFrames)
{
	TArray<FString> ImageFiles;
	bool bFoundImages = GetImageSequencePathAndFiles(InFullSequencePath, ImageFiles);

	if (ImageFiles.Num() == 0)
	{
		return false;
	}

	ImageFiles.Sort();

	// find an image filename which can be some optional alphabetic or underscore characters followed by some digits 
	// followed by some optional alphabetic or underscore characters with any extension
	const FRegexPattern ImageFilenamePattern(TEXT("^[a-zA-Z_]*([0-9]+)[a-zA-Z_]*\\.[a-zA-Z]+$"));
	FRegexMatcher ImageFilenameMatcher(ImageFilenamePattern, ImageFiles[0]);

	if (ImageFilenameMatcher.FindNext())
	{
		const FString Digits = ImageFilenameMatcher.GetCaptureGroup(1);
		const int32 DigitsStart = ImageFilenameMatcher.GetCaptureGroupBeginning(1);
		OutFrameOffset = FCString::Atoi(*Digits);
		const FString InitialChars = ImageFiles[0].Left(DigitsStart);
		const FString EndChars = ImageFiles[0].Right(ImageFiles[0].Len() - Digits.Len() - DigitsStart);
		FString DigitsSpecifier = TEXT("%0") + FString::FromInt(Digits.Len()) + TEXT("d");
		OutTrackingFilePath = InFullSequencePath / InitialChars + DigitsSpecifier + EndChars;
		OutNumFrames = ImageFiles.Num();
		return true;
	}

	return false;
}

FString FImageSequenceUtils::ExpandFilePathFormat(const FString& InFilePathFormat, int32 InFrameNumber)
{
	static_assert(sizeof(TCHAR) == 2); // Ensure TCHAR is the 16-bit wchar_t type expected by swprintf

	TCHAR Buffer[512];
	swprintf(Buffer, sizeof(Buffer) / sizeof(TCHAR), *InFilePathFormat, InFrameNumber);
	return Buffer;
}
