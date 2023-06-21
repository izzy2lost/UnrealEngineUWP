// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/Nodes/MovieGraphWidgetRendererBaseNode.h"

#include "MovieGraphBurnInNode.generated.h"

// Forward Declares
class SVirtualWindow;
class UMovieGraphBurnInWidget;
class UMovieGraphDefaultRenderer;
struct FMovieGraphRenderPassLayerData;

/** A node which generates a widget burn-in, rendered to a standalone image or composited on top of a render layer. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphBurnInNode : public UMovieGraphWidgetRendererBaseNode
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
#endif

	/**
	 * Gets an existing widget instance of type WidgetClass if one has been created. Otherwise, returns a new instance
	 * of WidgetClass with owner InOwner.
	 */
	TObjectPtr<UMovieGraphBurnInWidget> GetOrCreateBurnInWidget(UClass* WidgetClass, UWorld* InOwner);

public:
	static const FString RendererName;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_BurnInClass : 1;

	/** The widget that the burn-in should use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(MetaClass="/Script/MovieRenderPipelineCore.MovieGraphBurnInWidget", EditCondition="bOverride_BurnInClass"), Category="Widget Settings")
	FSoftClassPath BurnInClass;

protected:
	/** A burn-in pass for a specific render layer. Instances are stored on the UMovieGraphWidgetRendererBaseNode CDO. */
	struct FMovieGraphBurnInPass final : public FMovieGraphWidgetPass
	{
		virtual void Setup(TWeakObjectPtr<UMovieGraphDefaultRenderer> InRenderer, const FMovieGraphRenderPassLayerData& InLayer) override;
		virtual TSharedPtr<SWidget> GetWidget() override;
		virtual void Render(const FMovieGraphTraversalContext& InFrameTraversalContext, const FMovieGraphTimeStepData& InTimeData) override;
		virtual int32 GetCompositingSortOrder() const override;

	private:
		TObjectPtr<UMovieGraphBurnInWidget> GetBurnInWidget() const;
	};

private:
	/** Burn-in widget instances shared with all FMovieGraphBurnInPass instances, keyed by burn-in class. */
	UPROPERTY(Transient)
	TMap<const UClass*, TObjectPtr<UMovieGraphBurnInWidget>> BurnInWidgetInstances;

protected:
	// UMovieGraphWidgetRendererBaseNode Interface
	virtual TUniquePtr<FMovieGraphWidgetPass> GeneratePass() override;
	// ~UMovieGraphWidgetRendererBaseNode Interface
	
	// UMovieGraphRenderPassNode Interface
	virtual FString GetRendererNameImpl() const override { return RendererName; }
	virtual void TeardownImpl() override;
	// ~UMovieGraphRenderPassNode Interface
};