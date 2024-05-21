// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectCompileRunnable.h"

#include "HAL/FileManager.h"
#include "MuCO/UnrealMutableModelDiskStreamer.h"
#include "MuCO/UnrealToMutableTextureConversionUtils.h"
#include "MuCO/CustomizableObject.h"
#include "MuR/Model.h"
#include "MuT/Compiler.h"
#include "MuT/ErrorLog.h"
#include "MuT/UnrealPixelFormatOverride.h"
#include "Serialization/MemoryWriter.h"
#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Trace/Trace.inl"

class ITargetPlatform;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

#define UE_MUTABLE_CORE_REGION	TEXT("Mutable Core")


TAutoConsoleVariable<bool> CVarMutableCompilerConcurrency(
	TEXT("mutable.ForceCompilerConcurrency"),
	true,
	TEXT("Force the use of multithreading when compiling CustomizableObjects both in editor and cook commandlets."),
	ECVF_Default);

TAutoConsoleVariable<bool> CVarMutableCompilerDiskCache(
	TEXT("mutable.ForceCompilerDiskCache"),
	false,
	TEXT("Force the use of disk cache to reduce memory usage when compiling CustomizableObjects both in editor and cook commandlets."),
	ECVF_Default);

TAutoConsoleVariable<bool> CVarMutableCompilerFastCompression(
	TEXT("mutable.ForceFastTextureCompression"),
	false,
	TEXT("Force the use of lower quality but faster compression during cook."),
	ECVF_Default);


FCustomizableObjectCompileRunnable::FCustomizableObjectCompileRunnable(mu::Ptr<mu::Node> Root)
	: MutableRoot(Root)
	, bThreadCompleted(false)
{
	PrepareUnrealCompression();
}


mu::Ptr<mu::Image> FCustomizableObjectCompileRunnable::LoadResourceReferenced(int32 ID)
{
	MUTABLE_CPUPROFILER_SCOPE(LoadResourceReferenced);

	mu::Ptr<mu::Image> Image;
	if (!ReferencedTextures.IsValidIndex(ID))
	{
		// The id is not valid for this CO
		check(false);
		return Image;
	}

	// Find the texture id
	FMutableSourceTextureData& TextureData = ReferencedTextures[ID];

	// In the editor the src data can be directly accessed
	Image = new mu::Image();
	int32 MipmapsToSkip = 0;
	EUnrealToMutableConversionError Error = ConvertTextureUnrealSourceToMutable(Image.get(), TextureData, MipmapsToSkip);

	if (Error != EUnrealToMutableConversionError::Success)
	{
		// This could happen in the editor, because some source textures may have changed while there was a background compilation.
		// We just show a warning and move on. This cannot happen during cooks, so it is fine.
		UE_LOG(LogMutable, Warning, TEXT("Failed to load some source texture data for texture ID [%d]. Some textures may be corrupted."), ID);
	}

	return Image;
}


uint32 FCustomizableObjectCompileRunnable::Run()
{
	TRACE_BEGIN_REGION(UE_MUTABLE_CORE_REGION);

	UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] FCustomizableObjectCompileRunnable::Run start."), FPlatformTime::Seconds());

	uint32 Result = 1;
	ErrorMsg = FString();

	// Translate CO compile options into mu::CompilerOptions
	mu::Ptr<mu::CompilerOptions> CompilerOptions = new mu::CompilerOptions();

	bool bUseConcurrency = !Options.bIsCooking;
	if (CVarMutableCompilerConcurrency->GetBool())
	{
		bUseConcurrency = true;
	}

	CompilerOptions->SetUseConcurrency(bUseConcurrency);

	bool bUseDiskCache = Options.bUseDiskCompilation;
	if (CVarMutableCompilerDiskCache->GetBool())
	{
		bUseDiskCache = true;
	}

	CompilerOptions->SetUseDiskCache(bUseDiskCache);

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
	bool bUseHighQualityCompression = (Options.TextureCompression == ECustomizableObjectTextureCompression::HighQuality);
	if (CVarMutableCompilerFastCompression->GetBool())
	{
		bUseHighQualityCompression = false;
	}

	if (bUseHighQualityCompression)
	{
		CompilerOptions->SetImagePixelFormatOverride( UnrealPixelFormatFunc );
	}

	CompilerOptions->SetReferencedResourceCallback([this](int32 ID, TSharedPtr<mu::Ptr<mu::Image>> ResolvedImage, bool bRunImmediatlyIfPossible)
		{
			UE::Tasks::FTask LaunchTask = UE::Tasks::Launch(TEXT("ConstantGeneratorLaunchTasks"),
				[ID,ResolvedImage,this]()
				{
					mu::Ptr<mu::Image> Result = LoadResourceReferenced(ID);
					*ResolvedImage = Result;
				},
				LowLevelTasks::ETaskPriority::BackgroundLow
			);

			return LaunchTask;
		}
	);

	const int32 MinResidentMips = UTexture::GetStaticMinTextureResidentMipCount();
	CompilerOptions->SetDataPackingStrategy( MinResidentMips, Options.EmbeddedDataBytesLimit, Options.PackagedDataBytesLimit );

	// We always compile for progressive image generation.
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

	CompilerOptions->LogStats();

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


void FCustomizableObjectCompileRunnable::Tick()
{
	check(IsInGameThread());

	constexpr double MaxSecondsPerFrame = 0.4;

	double MaxTime = FPlatformTime::Seconds() + MaxSecondsPerFrame;

	FReferenceResourceRequest Request;
	while (PendingResourceReferenceRequests.Dequeue(Request))
	{
		*Request.ResolvedImage = LoadResourceReferenced(Request.ID);
		Request.CompletionEvent->Trigger();

		// Simple time limit enforcement to avoid blocking the game thread if there are many requests.
		double CurrentTime = FPlatformTime::Seconds();
		if (CurrentTime >= MaxTime)
		{
			break;
		}
	}
}


FCustomizableObjectSaveDDRunnable::FCustomizableObjectSaveDDRunnable(UCustomizableObject* CustomizableObject, const FCompilationOptions& InOptions, TSharedPtr<mu::Model> InModel)
{
	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectSaveDDRunnable::FCustomizableObjectSaveDDRunnable)
		
	Model = InModel;
	Options = InOptions;
	
	CustomizableObjectHeader.InternalVersion = CustomizableObject->GetPrivate()->CurrentSupportedVersion;
	CustomizableObjectHeader.VersionId = Options.bIsCooking? FGuid::NewGuid() : CustomizableObject->GetPrivate()->GetVersionId();

	if (!Options.bIsCooking)
	{
		// We will be saving all compilation data in two separate files, write CO Data
		FolderPath = CustomizableObject->GetPrivate()->GetCompiledDataFolderPath();
		CompileDataFullFileName = FolderPath + CustomizableObject->GetPrivate()->GetCompiledDataFileName(true, InOptions.TargetPlatform);
		StreamableDataFullFileName = FolderPath + CustomizableObject->GetPrivate()->GetCompiledDataFileName(false, InOptions.TargetPlatform);

		// Serialize Customizable Object's data
		FMemoryWriter64 MemoryWriter(ModelBytes);
		CustomizableObject->GetPrivate()->SaveCompiledData(MemoryWriter, Options.bIsCooking);
	}
#if WITH_EDITORONLY_DATA
	else
	{
		// Do a copy of the Morph and Clothing Data generated at compile time. Only needed when cooking.
		
		constexpr bool bGetCookedFalse = false;
		FModelResources& ModelResources = CustomizableObject->GetPrivate()->GetModelResources(bGetCookedFalse);
		
		static_assert(TCanBulkSerialize<FMorphTargetVertexData>::Value);
		const TArray<FMorphTargetVertexData>& MorphVertexData = ModelResources.EditorOnlyMorphTargetReconstructionData;

		MorphDataBytes.SetNum(MorphVertexData.Num() * sizeof(FMorphTargetVertexData));
		FMemory::Memcpy(MorphDataBytes.GetData(), MorphVertexData.GetData(), MorphDataBytes.Num());
		
		static_assert(TCanBulkSerialize<FCustomizableObjectMeshToMeshVertData>::Value);
		const TArray<FCustomizableObjectMeshToMeshVertData>& ClothingVertexData = ModelResources.EditorOnlyClothingMeshToMeshVertData;

		ClothingDataBytes.SetNum(ClothingVertexData.Num() * sizeof(FCustomizableObjectMeshToMeshVertData));
		FMemory::Memcpy(ClothingDataBytes.GetData(), ClothingVertexData.GetData(), ClothingDataBytes.Num());
	}
#endif // WITH_EDITORONLY_DATA
}


uint32 FCustomizableObjectSaveDDRunnable::Run()
{
	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectSaveDDRunnable::Run)

	// MorphDataBytes and ClothingDataBytes has data only if cooking. 
	check(!!Options.bIsCooking || MorphDataBytes.IsEmpty());
	check(!!Options.bIsCooking || ClothingDataBytes.IsEmpty());

	bool bModelSerialized = Model.Get() != nullptr;

	if (Options.bIsCooking)
	{
		// Serialize mu::Model and streamable resources 
		FMemoryWriter64 ModelMemoryWriter(ModelBytes, false, true);
		ModelMemoryWriter << bModelSerialized;
		if (bModelSerialized)
		{
			FUnrealMutableModelBulkWriterCook Streamer(&ModelMemoryWriter, &ModelStreamableData);
			constexpr bool bDropData = true;
			mu::Model::Serialise(Model.Get(), Streamer, bDropData);

			// Morph and Clothing are already in the corresponding buffer copied from the compilation thread.
		}
	}
	else if (bModelSerialized) // Save CO data + mu::Model and streamable resources to disk
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
			TUniquePtr<FArchive> ModelMemoryWriter(FileManager.CreateFileWriter(*CompileDataFullFileName));
			TUniquePtr<FArchive> StreamableMemoryWriter(FileManager.CreateFileWriter(*StreamableDataFullFileName));
			check(ModelMemoryWriter);
			check(StreamableMemoryWriter);

			// Serailize headers to validate data
			*ModelMemoryWriter << CustomizableObjectHeader;
			*StreamableMemoryWriter << CustomizableObjectHeader;

			// Serialize Customizable Object's Data to disk
			ModelMemoryWriter->Serialize(ModelBytes.GetData(), ModelBytes.Num() * sizeof(uint8));
			ModelBytes.Empty();

			// Serialize mu::Model and streamable resources
			*ModelMemoryWriter << bModelSerialized;

			FUnrealMutableModelBulkWriterEditor Streamer(ModelMemoryWriter.Get(), StreamableMemoryWriter.Get());
			constexpr bool bDropData = true;
			mu::Model::Serialise(Model.Get(), Streamer, bDropData);

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


bool FCustomizableObjectSaveDDRunnable::IsCompleted() const
{
	return bThreadCompleted;
}


const ITargetPlatform* FCustomizableObjectSaveDDRunnable::GetTargetPlatform() const
{
	return Options.TargetPlatform;
}

#undef LOCTEXT_NAMESPACE

