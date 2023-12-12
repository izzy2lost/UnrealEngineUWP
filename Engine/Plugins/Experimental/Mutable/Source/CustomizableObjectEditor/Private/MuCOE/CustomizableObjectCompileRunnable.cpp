// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectCompileRunnable.h"

#include "HAL/FileManager.h"
#include "MuCO/UnrealMutableModelDiskStreamer.h"
#include "MuCO/UnrealToMutableTextureConversionUtils.h"
#include "MuR/Model.h"
#include "MuT/Compiler.h"
#include "MuT/ErrorLog.h"
#include "MuT/UnrealPixelFormatOverride.h"
#include "Serialization/MemoryWriter.h"
#include "Async/Async.h"
#include "Trace/Trace.inl"

class ITargetPlatform;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

#define UE_MUTABLE_CORE_REGION	TEXT("Mutable Core")


FCustomizableObjectCompileRunnable::FCustomizableObjectCompileRunnable(mu::Ptr<mu::Node> Root)
	: MutableRoot(Root)
	, bThreadCompleted(false)
	, MutableIsDisabled(false)
{
	PrepareUnrealCompression();
}


uint32 FCustomizableObjectCompileRunnable::Run()
{
	TRACE_BEGIN_REGION(UE_MUTABLE_CORE_REGION);

	UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] FCustomizableObjectCompileRunnable::Run start."), FPlatformTime::Seconds());

	uint32 Result = 1;
	ErrorMsg = FString();

	if (MutableIsDisabled)
	{
		bThreadCompleted = true;
		UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] FCustomizableObjectCompileRunnable::Run end. NOTE: Mutable compile is deactivated in Editor. To reactivate it, go to Project Settings -> Plugins -> Mutable and unmark the option Disable Mutable Compile In Editor"), FPlatformTime::Seconds());
		return true;
	}

	// Translate CO compile options into mu::CompilerOptions
	mu::Ptr<mu::CompilerOptions> CompilerOptions = new mu::CompilerOptions();

	CompilerOptions->SetUseDiskCache(Options.bUseDiskCompilation);

	if (Options.OptimizationLevel > 2)
	{
		UE_LOG(LogMutable, Log, TEXT("Mutable compile optimization level out of range. Clamping to maximum."));
		Options.OptimizationLevel = 2;
	}

	switch (Options.OptimizationLevel)
	{
	case 0:
		CompilerOptions->SetOptimisationEnabled(false);
		CompilerOptions->SetConstReductionEnabled(false);
		CompilerOptions->SetOptimisationMaxIteration(1);
		break;

	case 1:
		CompilerOptions->SetOptimisationEnabled(false);
		CompilerOptions->SetConstReductionEnabled(true);
		CompilerOptions->SetOptimisationMaxIteration(1);
		break;

	case 2:
		CompilerOptions->SetOptimisationEnabled(true);
		CompilerOptions->SetConstReductionEnabled(true);
		CompilerOptions->SetOptimisationMaxIteration(0);
		break;

	default:
		CompilerOptions->SetOptimisationEnabled(true);
		CompilerOptions->SetConstReductionEnabled(true);
		CompilerOptions->SetOptimisationMaxIteration(0);
		break;
	}

	// Texture compression override, if necessary
	if (Options.TextureCompression== ECustomizableObjectTextureCompression::HighQuality)
	{
		CompilerOptions->SetImagePixelFormatOverride( UnrealPixelFormatFunc );
	}

	CompilerOptions->SetReferencedResourceCallback([this](int32 ID) 
		{
			mu::Ptr<mu::Image> Image;

			auto LoadFunc = [this,ID]() 
			{
				check(IsInGameThread());

				mu::Ptr<mu::Image> Image;
				if (!ReferencedTextures.IsValidIndex(ID))
				{
					// The id is not valid for this CO
					check(false);
					return Image;
				}

				// Find the texture id
				TSoftObjectPtr<UTexture> TexturePtr = ReferencedTextures[ID];

				// This can cause a stall because of loading the asset.
				UTexture2D* Texture = Cast<UTexture2D>(TexturePtr.LoadSynchronous());
				if (!Texture)
				{
					// Failed to load the texture
					check(false);
					return Image;
				}

				// In the editor the src data can be directly accessed
				Image = new mu::Image();
				int32 MipmapsToSkip = 0;
				bool bIsNormalComposite = false; // TODO?
				EUnrealToMutableConversionError Error = ConvertTextureUnrealSourceToMutable(Image.get(), Texture, bIsNormalComposite, MipmapsToSkip);
				check(Error == EUnrealToMutableConversionError::Success);
				return Image;
			};

			// This runs in a random thread
			if (IsInGameThread())
			{
				Image = LoadFunc();
			}
			else
			{
				UE::Tasks::FTaskEvent Completion(TEXT("SetReferencedResourceCallback"));

				AsyncTask(ENamedThreads::GameThread, [&Image, &Completion, LoadFunc]() 
					{
						Image = LoadFunc();
						Completion.Trigger();
					});

				Completion.Wait();
			}
			return Image;
		});

	// Minimum resident mip count.
	const int MinResidentMips = UTexture::GetStaticMinTextureResidentMipCount();
	// Data smaller than this will always be loaded, as part of the customizable object compiled model.
	const int MinRomSize = 128;
	CompilerOptions->SetDataPackingStrategy(MinRomSize, MinResidentMips);

	// At object compilation time we don't know if we will want progressive images or not. Assume we will. 
	// TODO: Per-state setting?
	CompilerOptions->SetEnableProgressiveImages(true);
	
	CompilerOptions->SetImageTiling(Options.ImageTiling);

	mu::Ptr<mu::Compiler> Compiler = new mu::Compiler(CompilerOptions);

	UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] FCustomizableObjectCompileRunnable Compile start."), FPlatformTime::Seconds());
	Model = Compiler->Compile(MutableRoot);

	// Dump all the log messages from the compiler
	mu::Ptr<const mu::ErrorLog> pLog = Compiler->GetLog();
	for (int i = 0; i < pLog->GetMessageCount(); ++i)
	{
		const FString& Message = pLog->GetMessageText(i);
		const mu::ErrorLogMessageType MessageType = pLog->GetMessageType(i);
		const mu::ErrorLogMessageAttachedDataView MessageAttachedData = pLog->GetMessageAttachedData(i);

		if (MessageType == mu::ELMT_WARNING || MessageType == mu::ELMT_ERROR)
		{
			const EMessageSeverity::Type Severity = MessageType == mu::ELMT_WARNING ? EMessageSeverity::Warning : EMessageSeverity::Error;
			const ELoggerSpamBin SpamBin = [&] {
				switch (pLog->GetMessageSpamBin(i)) {
				case mu::ErrorLogMessageSpamBin::ELMSB_UNKNOWN_TAG:
					return ELoggerSpamBin::TagsNotFound;
				case mu::ErrorLogMessageSpamBin::ELMSB_ALL:
				default:
					return ELoggerSpamBin::ShowAll;
			}
			}();

			if (MessageAttachedData.m_unassignedUVs && MessageAttachedData.m_unassignedUVsSize > 0) 
			{			
				TSharedPtr<FErrorAttachedData> ErrorAttachedData = MakeShared<FErrorAttachedData>();
				ErrorAttachedData->UnassignedUVs.Reset();
				ErrorAttachedData->UnassignedUVs.Append(MessageAttachedData.m_unassignedUVs, MessageAttachedData.m_unassignedUVsSize);
				ArrayErrors.Add(FError(Severity, FText::AsCultureInvariant(Message), ErrorAttachedData, pLog->GetMessageContext(i), SpamBin));
			}
			else
			{
				ArrayErrors.Add(FError(Severity, FText::AsCultureInvariant(Message), pLog->GetMessageContext(i), SpamBin));
			}
		}
	}

	Compiler = nullptr;

	bThreadCompleted = true;

	UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] FCustomizableObjectCompileRunnable::Run end."), FPlatformTime::Seconds());

	TRACE_END_REGION(UE_MUTABLE_CORE_REGION);

	return Result;
}


bool FCustomizableObjectCompileRunnable::IsCompleted() const
{
	return bThreadCompleted;
}


const TArray<FCustomizableObjectCompileRunnable::FError>& FCustomizableObjectCompileRunnable::GetArrayErrors() const
{
	return ArrayErrors;
}


FCustomizableObjectSaveDDRunnable::FCustomizableObjectSaveDDRunnable(UCustomizableObject* CustomizableObject, const FCompilationOptions& InOptions)
{
	Model = CustomizableObject->GetModel();
	Options = InOptions;
	
	CustomizableObjectHeader.InternalVersion = CustomizableObject->GetCurrentSupportedVersion();
	CustomizableObjectHeader.VersionId = Options.bIsCooking? FGuid::NewGuid() : CustomizableObject->GetVersionId();

	if (!Options.bIsCooking)
	{
		// We will be saving all compilation data in two separate files, write CO Data
		FolderPath = CustomizableObject->GetCompiledDataFolderPath();
		CompileDataFullFileName = FolderPath + CustomizableObject->GetCompiledDataFileName(true, InOptions.TargetPlatform);
		StreamableDataFullFileName = FolderPath + CustomizableObject->GetCompiledDataFileName(false, InOptions.TargetPlatform);

		// Serialize Customizable Object's data
		FMemoryWriter64 MemoryWriter(Bytes);
		CustomizableObject->SaveCompiledData(MemoryWriter, Options.bIsCooking);
	}
}


uint32 FCustomizableObjectSaveDDRunnable::Run()
{
	bool bModelSerialized = Model.Get() != nullptr;

	if (Options.bIsCooking)
	{
		// Serialize mu::Model and streamable resources 
		FMemoryWriter64 ModelMemoryWriter(Bytes, false, true);
		FMemoryWriter64 StreamableMemoryWriter(BulkDataBytes, false, true);

		ModelMemoryWriter << bModelSerialized;
		if (bModelSerialized)
		{
			FUnrealMutableModelBulkWriter Streamer(&ModelMemoryWriter, &StreamableMemoryWriter);
			mu::Model::Serialise(Model.Get(), Streamer);
		}
	}
	else if(bModelSerialized) // Save CO data + mu::Model and streamable resources to disk
	{
		// Create folder...
		IFileManager& FileManager = IFileManager::Get();
		FileManager.MakeDirectory(*FolderPath, true);

		// Delete files...
		bool bFilesDeleted = true;
		if (FileManager.FileExists(*CompileDataFullFileName)
			&& !FileManager.Delete(*CompileDataFullFileName, true, false, true))
		{
			UE_LOG(LogMutable, Error, TEXT("Failed to delete compiled data in file [%s]."), *CompileDataFullFileName);
			bFilesDeleted = false;
		}

		if (FileManager.FileExists(*StreamableDataFullFileName)
			&& !FileManager.Delete(*StreamableDataFullFileName, true, false, true))
		{
			UE_LOG(LogMutable, Error, TEXT("Failed to delete streamed data in file [%s]."), *StreamableDataFullFileName);
			bFilesDeleted = false;
		}

		// Store current compiled data
		if (bFilesDeleted)
		{
			// Create file writers...
			TUniquePtr<FArchive> ModelMemoryWriter( FileManager.CreateFileWriter(*CompileDataFullFileName) );
			TUniquePtr<FArchive> StreamableMemoryWriter( FileManager.CreateFileWriter(*StreamableDataFullFileName) );
			check(ModelMemoryWriter);
			check(StreamableMemoryWriter);

			// Serailize headers to validate data
			*ModelMemoryWriter << CustomizableObjectHeader;
			*StreamableMemoryWriter << CustomizableObjectHeader;

			// Serialize Customizable Object's Data to disk
			ModelMemoryWriter->Serialize(Bytes.GetData(), Bytes.Num() * sizeof(uint8));
			Bytes.Empty();

			// Serialize mu::Model and streamable resources
			*ModelMemoryWriter << bModelSerialized;

			FUnrealMutableModelBulkWriter Streamer(ModelMemoryWriter.Get(), StreamableMemoryWriter.Get());
			mu::Model::Serialise(Model.Get(), Streamer);

			// Save to disk
			ModelMemoryWriter->Flush();
			StreamableMemoryWriter->Flush();

			ModelMemoryWriter->Close();
			StreamableMemoryWriter->Close();
		}
		else
		{
			// Remove old data if there.
			Model.Reset();
		}
	}

	bThreadCompleted = true;

	return 1;
}


TArray64<uint8>& FCustomizableObjectSaveDDRunnable::GetModelBytes()
{
	return Bytes;
}


TArray64<uint8>& FCustomizableObjectSaveDDRunnable::GetBulkBytes()
{
	return BulkDataBytes;
}


bool FCustomizableObjectSaveDDRunnable::IsCompleted() const
{
	return bThreadCompleted;
}


const ITargetPlatform* FCustomizableObjectSaveDDRunnable::GetTargetPlatform() const
{
	return Options.TargetPlatform;
}

#undef LOCTEXT_NAMESPACE

