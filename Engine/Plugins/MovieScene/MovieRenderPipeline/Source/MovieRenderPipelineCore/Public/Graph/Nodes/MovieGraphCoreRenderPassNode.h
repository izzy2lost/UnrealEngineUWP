// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/Nodes/MovieGraphRenderPassNode.h"

#include "Graph/MovieGraphConfig.h"
#include "Graph/MovieGraphDefaultRenderer.h" // For CameraInfo
#include "Graph/MovieGraphDataTypes.h"
#include "SceneTypes.h"
#include "ShowFlags.h"
#include "Camera/CameraTypes.h"
#include "Engine/EngineTypes.h"
#include "Async/TaskGraphFwd.h"
#include "Styling/AppStyle.h"

#include "MovieGraphCoreRenderPassNode.generated.h"

// Forward Declares
class UMovieGraphDefaultRenderer;
namespace UE::MovieGraph::DefaultRenderer
{
	struct FRenderTargetInitParams;
	struct IMovieGraphOutputMerger;
}
struct FMovieGraphRenderPassSetupData;
struct FMovieGraphRenderPassLayerData;
struct FImageOverlappedAccumulator;
enum EViewModeIndex : int;

// For FViewFamilyContextInitData
class FRenderTarget;
class UWorld;
class FSceneView;
class AActor;
class FSceneViewFamilyContext;
class FCanvas;

namespace UE::MovieGraph
{
	struct FMovieGraphRenderDataAccumulationArgs
	{
	public:
		TWeakPtr<FImageOverlappedAccumulator, ESPMode::ThreadSafe> ImageAccumulator;
		TWeakPtr<IMovieGraphOutputMerger, ESPMode::ThreadSafe> OutputMerger;

		// If it's the first sample then we will reset the accumulator to a clean slate before accumulating into it.
		bool bIsFirstSample;
		// If it's the last sample, then we will trigger moving the data to the output merger. It can be both the first and last sample at the same time.
		bool bIsLastSample;
	};

	void AccumulateSample_TaskThread(TUniquePtr<FImagePixelData>&& InPixelData, const UE::MovieGraph::FMovieGraphSampleState InSampleState, const UE::MovieGraph::FMovieGraphRenderDataAccumulationArgs& InAccumulationParams);
}

/**
 * Stores show flag enable/disable state, as well as per-flag override state.
 *
 * Here, enable/disable state refers to the actual state of the show flag itself (ie, whether it is turned on or off).
 * This is the value that will be applied to renders. "Override" state is whether the flag has been marked as overridden
 * in the UI (ie, whether it is a value that graph traversal should respect). Flags must be marked as overridden in order
 * for their values to deviate from the default.
 */
UCLASS(BlueprintType)
class MOVIERENDERPIPELINECORE_API UMovieGraphShowFlags : public UObject, public IMovieGraphTraversableObject
{
	GENERATED_BODY()

public:
	UMovieGraphShowFlags();

	/** Gets a copy of the show flags. If they need to be modified, use one of the Set*() methods. */
	FEngineShowFlags GetShowFlags() const;

	/** Gets the enable/disable state for a specific show flag. */
	bool IsShowFlagEnabled(const uint32 ShowFlagIndex) const;

	/** Sets the enable/disable state for a specific show flag. The flag must be marked as overridden for this to have an effect. */
	void SetShowFlagEnabled(const uint32 ShowFlagIndex, const bool bIsShowFlagEnabled);

	/** Gets the override state for a specific show flag. */
	bool IsShowFlagOverridden(const uint32 ShowFlagIndex) const;

	/** Sets the override state for a specific show flag. */
	void SetShowFlagOverridden(const uint32 ShowFlagIndex, const bool bIsOverridden);

	/** Gets the indices of all show flags that have been overridden. */
	const TSet<uint32>& GetOverriddenShowFlags() const;

	/**
	 * Applies a default value to the show flags. However, the default will not be applied if a user has already
	 * specified an override for the flag. This should generally only be called from renderers in order for them to
	 * specify defaults that are relevant.
	 */
	void ApplyDefaultShowFlagValue(const uint32 ShowFlagIndex, const bool bShowFlagState);

	// IMovieGraphTraversableObject interface
	virtual void Merge(const IMovieGraphTraversableObject* InSourceObject) override;
	virtual TArray<TPair<FString, FString>> GetMergedProperties() const override;
	// ~IMovieGraphTraversableObject interface
	
private:
	/** Copies the overrides from ShowFlagEnableState to InEngineShowFlags. */
	void CopyOverridesToShowFlags(FEngineShowFlags& InEngineShowFlags) const;

private:
	/**
	 * The show flags which have been marked as overridden in the UI (note this is NOT the enabled/disabled state of the
	 * flag itself).
	 */
	UPROPERTY()
	TSet<uint32> OverriddenShowFlags;

	/**
	 * If the flag has been marked as overridden, this stores the enable/disable state of the flag. Key = show flag
	 * index, value = enable/disable state of the flag.
	 */
	UPROPERTY()
	TMap<uint32, bool> ShowFlagEnableState;

	/**
	 * The default show flag state. Since FEngineShowFlags itself cannot be serialized (it is not a USTRUCT), we need to
	 * keep track of sparse overrides (ShowFlagEnableState) on top of the base state that these flags are initialized
	 * with (ESFIM_Game, plus any additional flags that a specialized renderer specifies).
	 */
	FEngineShowFlags ShowFlags;
};

/** Core functionality for a typical render pass node. */
UCLASS(Abstract)
class MOVIERENDERPIPELINECORE_API UMovieGraphCoreRenderPassNode : public UMovieGraphRenderPassNode
{
	GENERATED_BODY()

public:
	UMovieGraphCoreRenderPassNode();

#if WITH_EDITOR
	virtual FLinearColor GetNodeTitleColor() const override
	{
		return FLinearColor(0.572f, 0.274f, 1.f);
	}

	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override
	{
		static const FSlateIcon DeferredRendererIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "SequenceRecorder.TabIcon");

		OutColor = FLinearColor::White;
		return DeferredRendererIcon;
	}
#endif

public:
	// Note: Since *individual* show flags are overridden instead of the entire ShowFlags property, manually set to
	// overridden so the traversal picks the changes up (otherwise they will be ignored).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_ShowFlags : 1 = 1;

	/** The show flags that should be active during a render for this node. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Show Flags")
	TObjectPtr<UMovieGraphShowFlags> ShowFlags;

protected:
	// UMovieGraphRenderPassNode Interface
	virtual void SetupImpl(const FMovieGraphRenderPassSetupData& InSetupData) override;
	virtual void TeardownImpl() override;
	virtual void RenderImpl(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData) override;
	virtual void GatherOutputPassesImpl(TArray<FMovieGraphRenderDataIdentifier>& OutExpectedPasses) const override;
	// ~UMovieGraphRenderPassNode Interface

	/** Gets the view mode index that should be active for this renderer. */
	virtual EViewModeIndex GetViewModeIndex() const;

	/** Gets the show flags that should be active for this renderer. */
	virtual FEngineShowFlags GetShowFlags() const;

	/** Gets any show flags that should be applied as defaults, before user changes are applied. */
	virtual TArray<uint32> GetDefaultShowFlags() const;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	struct FViewFamilyContextInitData
	{
		FViewFamilyContextInitData()
			: RenderTarget(nullptr)
			, World(nullptr)
			, SceneCaptureSource(ESceneCaptureSource::SCS_MAX)
			, bWorldIsPaused(false)
			, GlobalScreenPercentageFraction(1.0f)
			, OverscanFraction(0.f)
			, FrameIndex(-1)
			, bCameraCut(false)
			, AntiAliasingMethod(EAntiAliasingMethod::AAM_None)
			, SceneViewStateReference(nullptr)
		{
		}

		class FRenderTarget* RenderTarget;
		class UWorld* World;
		FMovieGraphTimeStepData TimeData;
		ESceneCaptureSource SceneCaptureSource;
		bool bWorldIsPaused;
		float GlobalScreenPercentageFraction;
		float OverscanFraction;
		int32 FrameIndex;
		bool bCameraCut;
		EAntiAliasingMethod AntiAliasingMethod;

		FSceneViewStateInterface* SceneViewStateReference;

		// Camera Setup
		UE::MovieGraph::DefaultRenderer::FCameraInfo CameraInfo;
	};

	struct FMovieGraphRenderPass
	{
		void Setup(TWeakObjectPtr<UMovieGraphDefaultRenderer> InRenderer, TWeakObjectPtr<UMovieGraphCoreRenderPassNode> InRenderPassNode, const FMovieGraphRenderPassLayerData& InLayer);
		void Teardown();
		void Render(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData);
		void GatherOutputPassesImpl(TArray<FMovieGraphRenderDataIdentifier>& OutExpectedPasses) const;
		void AddReferencedObjects(FReferenceCollector& Collector);
		FName GetBranchName() const;
		TWeakObjectPtr<UMovieGraphDefaultRenderer> GetRenderer() const;

	protected:
		TSharedRef<FSceneViewFamilyContext> AllocateSceneViewFamilyContext(const FViewFamilyContextInitData& InInitData);
		FSceneView* AllocateSceneView(TSharedPtr<FSceneViewFamilyContext> InViewFamilyContext, FViewFamilyContextInitData& InInitData) const;
		void ApplyMoviePipelineOverridesToViewFamily(TSharedRef<FSceneViewFamilyContext> InOutFamily, const FViewFamilyContextInitData& InInitData);
		void PostRendererSubmission(const UE::MovieGraph::FMovieGraphSampleState& InSampleState, const UE::MovieGraph::DefaultRenderer::FRenderTargetInitParams& InRenderTargetInitParams, FCanvas& InCanvas);

	protected:
		FMovieGraphRenderPassLayerData LayerData;

		/** Unique identifier passed in GatherOutputPasses and with each render that identifies the data produced by this renderer. */
		FMovieGraphRenderDataIdentifier RenderDataIdentifier;

		// Scene View history used by the renderer 
		FSceneViewStateReference SceneViewState;

		TWeakObjectPtr<class UMovieGraphDefaultRenderer> Renderer;
		TWeakObjectPtr<class UMovieGraphCoreRenderPassNode> RenderPassNode;
	};

	TArray<TUniquePtr<FMovieGraphRenderPass>> CurrentInstances;
};