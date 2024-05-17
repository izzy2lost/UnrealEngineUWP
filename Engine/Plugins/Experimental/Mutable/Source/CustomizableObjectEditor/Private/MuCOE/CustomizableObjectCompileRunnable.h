// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Runnable.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectCompilerTypes.h"
#include "MuCO/UnrealToMutableTextureConversionUtils.h"
#include "MuR/Ptr.h"
#include "MuT/Node.h"
#include "MuCOE/CustomizableObjectEditorLogger.h"

#include <atomic>

class ITargetPlatform;
class UCustomizableObject;


class FCustomizableObjectCompileRunnable : public FRunnable
{
public:

	struct FErrorAttachedData
	{
		TArray<float> UnassignedUVs;
	};

	struct FError
	{
		EMessageSeverity::Type Severity;
		ELoggerSpamBin SpamBin = ELoggerSpamBin::ShowAll;
		FText Message;
		TSharedPtr<FErrorAttachedData> AttachedData;
		const void* Context;

		FError(const EMessageSeverity::Type InSeverity, const FText& InMessage, const void* InContext, const ELoggerSpamBin InSpamBin = ELoggerSpamBin::ShowAll) : Severity(InSeverity), SpamBin(InSpamBin), Message(InMessage), Context(InContext) {}
		FError(const EMessageSeverity::Type InSeverity, const FText& InMessage, const TSharedPtr<FErrorAttachedData>& InAttachedData, const void* InContext, const ELoggerSpamBin InSpamBin = ELoggerSpamBin::ShowAll)
			: Severity(InSeverity), SpamBin(InSpamBin), Message(InMessage), AttachedData(InAttachedData), Context(InContext) {}
	};

private:

	mu::Ptr<mu::Node> MutableRoot;
	TArray<FError> ArrayErrors;

	/** */
	struct FReferenceResourceRequest
	{
		int32 ID = -1;
		TSharedPtr<mu::Ptr<mu::Image>> ResolvedImage;
		TSharedPtr<UE::Tasks::FTaskEvent> CompletionEvent;
	};
	TQueue<FReferenceResourceRequest, EQueueMode::Mpsc> PendingResourceReferenceRequests;

	mu::Ptr<mu::Image> LoadResourceReferenced(int32 ID);


public:

	FCustomizableObjectCompileRunnable(mu::Ptr<mu::Node> Root);

	// FRunnable interface
	uint32 Run() override;

	// Own interface

	//
	bool IsCompleted() const;

	//
	const TArray<FError>& GetArrayErrors() const;

	void Tick();

	TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model;

	FCompilationOptions Options;
	
	TArray<FMutableSourceTextureData> ReferencedTextures;

	FString ErrorMsg;

	// Whether the thread has finished running
	std::atomic<bool> bThreadCompleted;
};


class FCustomizableObjectSaveDDRunnable : public FRunnable
{
public:

	FCustomizableObjectSaveDDRunnable(UCustomizableObject* CustomizableObject, const FCompilationOptions& Options, TSharedPtr<mu::Model> InModel);

	// FRunnable interface
	uint32 Run() override;

	//
	bool IsCompleted() const;


	const ITargetPlatform* GetTargetPlatform() const;

private:
	FCompilationOptions Options;

	MutableCompiledDataStreamHeader CustomizableObjectHeader;

	// Paths used to save files to disk
	FString FolderPath;
	FString CompileDataFullFileName;
	FString StreamableDataFullFileName;

	bool bIsCooking = false;

	// Whether the thread has finished running
	std::atomic<bool> bThreadCompleted = false;

public:

	TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model;

	// Bytes where the model is stored
	TArray64<uint8> ModelBytes;

	// Model streamed data
	FModelStreamableData ModelStreamableData;

	// Bytes store streameable files coming form the CO itself.
	TArray64<uint8> MorphDataBytes;

	// Bytes store streameable files coming form the CO itself.
	TArray64<uint8> ClothingDataBytes;
};
