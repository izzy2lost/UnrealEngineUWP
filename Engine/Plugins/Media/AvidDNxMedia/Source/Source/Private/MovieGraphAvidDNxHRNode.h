// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvidDNxEncoder/AvidDNxEncoder.h"
#include "Graph/MovieGraphNode.h"
#include "Graph/Nodes/MovieGraphVideoOutputNode.h"

#include "MovieGraphAvidDNxHRNode.generated.h"

class FAvidDNxEncoder;

/** The container formats available for use with the Avid DNxHR node. */
UENUM(BlueprintType)
enum class EMovieGraphAvidDNxHRFormat : uint8
{
	Mxf UMETA(DisplayName = "Material Exchange Format (MXF)"),
	Mov UMETA(DisplayName = "QuickTime (MOV)")
};

/** A node which can output Avid DNxHR movies. */
UCLASS(BlueprintType, PrioritizeCategories=("FileOutput"))
class UMovieGraphAvidDNxHRNode : public UMovieGraphVideoOutputNode
{
	GENERATED_BODY()

public:
	UMovieGraphAvidDNxHRNode();

	virtual EMovieGraphBranchRestriction GetBranchRestriction() const override;

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
	virtual FText GetKeywords() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
#endif

protected:
	// UMovieGraphVideoOutputNode Interface
	virtual TUniquePtr<MovieRenderGraph::IVideoCodecWriter> Initialize_GameThread(const FMovieGraphVideoNodeInitializationContext& InInitializationContext) override;
	virtual bool Initialize_EncodeThread(MovieRenderGraph::IVideoCodecWriter* InWriter) override;
	virtual void WriteFrame_EncodeThread(MovieRenderGraph::IVideoCodecWriter* InWriter, FImagePixelData* InPixelData, TArray<FMovieGraphPassData>&& InCompositePasses, TObjectPtr<UMovieGraphEvaluatedConfig> InEvaluatedConfig, const FString& InBranchName) override;
	virtual void BeginFinalize_EncodeThread(MovieRenderGraph::IVideoCodecWriter* InWriter) override;
	virtual void Finalize_EncodeThread(MovieRenderGraph::IVideoCodecWriter* InWriter) override;
	virtual const TCHAR* GetFilenameExtension() const override;
	virtual bool IsAudioSupported() const override;
	// ~UMovieGraphVideoOutputNode Interface

protected:
	struct FAvidWriter : public MovieRenderGraph::IVideoCodecWriter
	{
		TUniquePtr<FAvidDNxEncoder> Writer;
	};
	
	/** The pipeline that is running this node. */
	TWeakObjectPtr<UMovieGraphPipeline> CachedPipeline;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_Format : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_Quality : 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_CustomTimecodeStart : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_bDropFrameTimecode : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_OCIOConfiguration : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_OCIOContext : 1;

	/** The format to output the movie to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Avid DNxHR", meta=(EditCondition="bOverride_Format"))
	EMovieGraphAvidDNxHRFormat Format;

	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Avid DNxHR", meta=(EditCondition="bOverride_Quality"))
	EAvidDNxEncoderQuality Quality = EAvidDNxEncoderQuality::HQ_8bit;

	/** Start the timecode at a specific value, rather than the value coming from the Level Sequence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avid DNxHR", meta = (EditCondition = "bOverride_CustomTimecodeStart"))
	FTimecode CustomTimecodeStart;

	/** Whether the embedded timecode track should be written using drop-frame format. Only applicable if the sequence framerate is 29.97. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avid DNxHR", DisplayName = "Use DF Timecode if 29.97 FPS", meta = (EditCondition = "bOverride_bDropFrameTimecode"))
	bool bDropFrameTimecode;

	/**
	* OCIO configuration/transform settings.
	*
	* Note: There are differences from the previous implementation in MRQ given that we are now doing CPU-side processing.
	* 1) This feature only works on desktop platforms when the OpenColorIO library is available.
	* 2) Users are now responsible for setting the renderer output space to Final Color (HDR) in Linear Working Color Space (SCS_FinalColorHDR) by
	*    disabling the Tone Curve setting on the renderer node.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OCIO", DisplayName="OCIO Configuration", meta = (DisplayAfter = "FileNameFormat", EditCondition = "bOverride_OCIOConfiguration"))
	FOpenColorIODisplayConfiguration OCIOConfiguration;

	/**
	* OCIO context of key-value string pairs, typically used to apply shot-specific looks (such as a CDL color correction, or a 1D grade LUT).
	* 
	* Notes:
	* 1) If a configuration asset base context was set, it remains active but can be overridden here with new key-values.
	* 2) Format tokens such as {shot_name} are supported and will get resolved before submission.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OCIO", DisplayName = "OCIO Context", meta = (DisplayAfter = "OCIOConfiguration", EditCondition = "bOverride_OCIOContext"))
	TMap<FString, FString> OCIOContext;
};