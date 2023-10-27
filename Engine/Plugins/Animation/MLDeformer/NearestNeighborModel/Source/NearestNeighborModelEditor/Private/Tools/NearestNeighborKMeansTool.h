// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NearestNeighborMenuTool.h"
#include "NearestNeighborModelHelpers.h"
#include "Widgets/SCompoundWidget.h"

#include "NearestNeighborKMeansTool.generated.h"

class UAnimSequence;
class UGeometryCache;
class UMLDeformerAsset;
class UNearestNeighborModel;
namespace UE::MLDeformer
{
	class FMLDeformerEditorModel;
}

UCLASS(Blueprintable)
class NEARESTNEIGHBORMODELEDITOR_API UNearestNeighborKMeansData : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, Category = "Input")
	TObjectPtr<const UMLDeformerAsset> NearestNeighborModelAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	int32 SectionIndex = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	int32 NumClusters = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TArray<TObjectPtr<UAnimSequence>> InputPoses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	TObjectPtr<UAnimSequence> ExtractedPoses;
};

namespace UE::NearestNeighborModel
{
	class FNearestNeighborKMeansTool : public FNearestNeighborMenuTool
	{
	public:
		virtual ~FNearestNeighborKMeansTool() = default;
		virtual FName GetToolName() override;
		virtual FText GetToolTip() override;
		virtual UObject* CreateData() override;
		virtual void InitData(UObject& Data, UE::MLDeformer::FMLDeformerEditorToolkit& Toolkit) override;
		virtual TSharedRef<SWidget> CreateAdditionalWidgets(UObject& Data, TWeakPtr<UE::MLDeformer::FMLDeformerEditorModel> InEditorModel) override;
	};
};