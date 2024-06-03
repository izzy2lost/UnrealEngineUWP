// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphAvidDNxHRNode.h"

#include "ImageWriteTask.h"
#include "MoviePipelineImageQuantization.h"
#include "AvidDNxEncoder/AvidDNxEncoder.h"
#include "Graph/MovieGraphBlueprintLibrary.h"
#include "Graph/MovieGraphConfig.h"
#include "Graph/MovieGraphPipeline.h"
#include "Graph/MovieGraphOCIOHelper.h"
#include "Graph/Nodes/MovieGraphGlobalOutputSettingNode.h"
#include "MovieRenderPipelineCoreModule.h"
#include "Styling/AppStyle.h"

EMovieGraphBranchRestriction UMovieGraphAvidDNxHRNode::GetBranchRestriction() const
{
	return EMovieGraphBranchRestriction::Globals;
}

#if WITH_EDITOR
FText UMovieGraphAvidDNxHRNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText AvidDNxHRNodeName = NSLOCTEXT("MovieGraphNodes", "NodeName_AvidDNxHR", "Avid DNxHR Movie");
	return AvidDNxHRNodeName;
}

FText UMovieGraphAvidDNxHRNode::GetMenuCategory() const
{
	return NSLOCTEXT("MovieGraphNodes", "AvidDNxHRNode_Category", "Output Type");
}

FText UMovieGraphAvidDNxHRNode::GetKeywords() const
{
	static const FText Keywords = NSLOCTEXT("MovieGraphNodes", "AvidDNxHRGraphNode_Keywords", "avid dnxhr mxf mov movie video");
	return Keywords;
}

FLinearColor UMovieGraphAvidDNxHRNode::GetNodeTitleColor() const
{
	static const FLinearColor AvidDNxHRNodeColor = FLinearColor(0.047f, 0.654f, 0.537f);
	return AvidDNxHRNodeColor;
}

FSlateIcon UMovieGraphAvidDNxHRNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon AvidDNxHRIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Cinematics");

	OutColor = FLinearColor::White;
	return AvidDNxHRIcon;
}

TUniquePtr<MovieRenderGraph::IVideoCodecWriter> UMovieGraphAvidDNxHRNode::Initialize_GameThread(UMovieGraphPipeline* InPipeline, TObjectPtr<UMovieGraphEvaluatedConfig> InEvaluatedConfig, const FString& InFileName, FIntPoint InResolution, EImagePixelType InPixelType, ERGBFormat InPixelFormat, uint8 InBitDepth, uint8 InNumChannels, bool bAllowOCIO)
{
	constexpr bool bIncludeCDOs = true;
	constexpr bool bExactMatch = true;
	UMovieGraphGlobalOutputSettingNode* OutputSetting =
		InEvaluatedConfig->GetSettingForBranch<UMovieGraphGlobalOutputSettingNode>(GlobalsPinName, bIncludeCDOs, bExactMatch);
	
	const FFrameRate SourceFrameRate = InPipeline->GetDataSourceInstance()->GetDisplayRate();
	const FFrameRate EffectiveFrameRate = UMovieGraphBlueprintLibrary::GetEffectiveFrameRate(OutputSetting, SourceFrameRate);
	
	FAvidDNxEncoderOptions Options;
	Options.OutputFilename = InFileName;
	Options.Width = InResolution.X;
	Options.Height = InResolution.Y;
	Options.FrameRate = EffectiveFrameRate;
	Options.bCompress = true;
	Options.NumberOfEncodingThreads = 4;
	
	TUniquePtr<FAvidWriter> NewWriter = MakeUnique<FAvidWriter>();
	NewWriter->Writer = MakeUnique<FAvidDNxEncoder>(Options);
	NewWriter->FileName = InFileName;

	// If OCIO is enabled, don't do additional color conversion
	NewWriter->bConvertToSrgb = !(bOverride_OCIOConfiguration && OCIOConfiguration.bIsEnabled && bAllowOCIO);

	CachedPipeline = InPipeline;
	
	return NewWriter;
}

bool UMovieGraphAvidDNxHRNode::Initialize_EncodeThread(MovieRenderGraph::IVideoCodecWriter* InWriter)
{
	const FAvidWriter* CodecWriter = static_cast<FAvidWriter*>(InWriter);
	if(!CodecWriter->Writer->Initialize())
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("Failed to initialize Avid DNxHR writer."));
		return false;
	}
	
	return true;
}

void UMovieGraphAvidDNxHRNode::WriteFrame_EncodeThread(MovieRenderGraph::IVideoCodecWriter* InWriter, FImagePixelData* InPixelData, TArray<FMovieGraphPassData>&& InCompositePasses, TObjectPtr<UMovieGraphEvaluatedConfig> InEvaluatedConfig)
{
	const FAvidWriter* CodecWriter = static_cast<FAvidWriter*>(InWriter);
	
	const UE::MovieGraph::FMovieGraphSampleState* Payload = InPixelData->GetPayload<UE::MovieGraph::FMovieGraphSampleState>();

	// Quantize our 16-bit float data to 8-bit and apply sRGB if needed
	const TUniquePtr<FImagePixelData> QuantizedPixelData = UE::MoviePipeline::QuantizeImagePixelDataToBitDepth(InPixelData, 8, nullptr, InWriter->bConvertToSrgb);

	TArray<FPixelPreProcessor> PixelPreProcessors;

#if WITH_OCIO
	FMovieGraphOCIOHelper::GenerateOcioPixelPreProcessor(Payload, CachedPipeline.Get(), InEvaluatedConfig, OCIOConfiguration, OCIOContext, PixelPreProcessors);
#endif

	// Do a quick composite of renders/burn-ins.
	for (const FMovieGraphPassData& CompositePass : InCompositePasses)
	{
		// We don't need to copy the data here (even though it's being passed to a async system) because we already made a unique copy of the
		// burn in/widget data when we decided to composite it.
		switch (QuantizedPixelData->GetType())
		{
		case EImagePixelType::Color:
			PixelPreProcessors.Add(TAsyncCompositeImage<FColor>(CompositePass.Value->MoveImageDataToNew()));
			break;
		case EImagePixelType::Float16:
			PixelPreProcessors.Add(TAsyncCompositeImage<FFloat16Color>(CompositePass.Value->MoveImageDataToNew()));
			break;
		case EImagePixelType::Float32:
			PixelPreProcessors.Add(TAsyncCompositeImage<FLinearColor>(CompositePass.Value->MoveImageDataToNew()));
			break;
		}
	}

	// This is done on the current thread for simplicity but the composite itself is parallelized.
	FImagePixelData* PixelData = QuantizedPixelData.Get();
	for (const FPixelPreProcessor& PreProcessor : PixelPreProcessors)
	{
		// PreProcessors are assumed to be valid.
		PreProcessor(PixelData);
	}

	const void* Data = nullptr;
	int64 DataSize;
	QuantizedPixelData->GetRawData(Data, DataSize);

	CodecWriter->Writer->WriteFrame((uint8*)Data);
}

void UMovieGraphAvidDNxHRNode::BeginFinalize_EncodeThread(MovieRenderGraph::IVideoCodecWriter* InWriter)
{
	return;
}

void UMovieGraphAvidDNxHRNode::Finalize_EncodeThread(MovieRenderGraph::IVideoCodecWriter* InWriter)
{
	// Write to disk
	const FAvidWriter* CodecWriter = static_cast<FAvidWriter*>(InWriter);
	CodecWriter->Writer->Finalize();
}

const TCHAR* UMovieGraphAvidDNxHRNode::GetFilenameExtension() const
{
	// TODO: This should return "mov" when MOV is supported and selected
	return TEXT("mxf");
}

bool UMovieGraphAvidDNxHRNode::IsAudioSupported() const
{
	// The current Avid DNxHR SDK does not support audio encoding so we don't write audio to the container.
	return false;
}

#endif // WITH_EDITOR