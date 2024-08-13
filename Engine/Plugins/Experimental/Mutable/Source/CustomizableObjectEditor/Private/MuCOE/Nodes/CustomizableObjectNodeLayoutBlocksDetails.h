// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CustomizableObjectNodeDetails.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

enum class ECheckBoxState : uint8;
namespace ESelectInfo { enum Type : int; }

class FString;
class IDetailLayoutBuilder;
class STextBlock;
class SWidget;
class UCustomizableObjectNodeLayoutBlocks;
struct EVisibility;

class FCustomizableObjectNodeLayoutBlocksDetails : public FCustomizableObjectNodeDetails
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;


private:

	/** Returns the visibility of some widgets based on the layout strategy. */
	EVisibility FixedStrategyOptionsVisibility() const;

	/** Fills the combo box arrays sources */
	void FillComboBoxOptionsArrays(TSharedPtr<FString>& CurrGridSize, 
		TSharedPtr<FString>& CurrStrategy, 
		TSharedPtr<FString>& CurrAutoBlocks,
		TSharedPtr<FString>& CurrAutoBlocksMerge,
		TSharedPtr<FString>& CurrMaxSize,
		TSharedPtr<FString>& CurrRedMethod);

	/** Layout Options Callbacks */
	void OnGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnMaxGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnReductionMethodChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnLayoutPackingStrategyChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnAutoBlocksChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnAutoBlocksMergeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnIgnoreErrorsCheckStateChanged(ECheckBoxState State);
	void OnLODBoxValueChanged(int32 Value);
	TSharedRef<SWidget> OnGenerateStrategyComboBox(TSharedPtr<FString> InItem) const;
	TSharedRef<SWidget> OnGenerateAutoBlocksComboBox(TSharedPtr<FString> InItem) const;
	TSharedRef<SWidget> OnGenerateAutoBlocksMergeComboBox(TSharedPtr<FString> InItem) const;
	TSharedRef<SWidget> OnGenerateReductionMethodComboBox(TSharedPtr<FString> InItem) const;
	FText GetSelectedLayoutStrategyName() const;
	FText GetSelectedAutoBlocksName() const;
	FText GetSelectedAutoBlocksMergeName() const;
	FText GetSelectedLayoutReductionMethodName() const;
	FText GetSelectedLayoutStrategyTooltip() const;
	FText GetSelectedLayoutReductionMethodTooltip() const;
	FText GetSelectedAutoBlocksTooltip() const;
	FText GetSelectedAutoBlocksMergeTooltip() const;

private:

	/** Weak pointer to the node */
	TWeakObjectPtr<UCustomizableObjectNodeLayoutBlocks> Node;

	// Layout block editor widget
	TSharedPtr<class SCustomizableObjectNodeLayoutBlocksEditor> LayoutBlocksEditor;

	// Widget to select at which LOD layout vertex warnings will start to be ignored
	TSharedPtr<SWidget> LODSelectorWidget;
	TSharedPtr<STextBlock> LODSelectorTextWidget;

	/** List of available layout grid sizes. */
	TArray< TSharedPtr< FString > > LayoutGridSizes;

	/** List of available layout packing strategies. */
	struct FPackingStrategyOption
	{
		ECustomizableObjectTextureLayoutPackingStrategy Value;
		FText Tooltip;
	};
	TArray<TSharedPtr<FString>> LayoutPackingStrategies;
	TArray<FPackingStrategyOption> LayoutPackingStrategiesOptions;

	/** List of available block reduction methods. */
	TArray< TSharedPtr< FString > > BlockReductionMethods;
	TArray<FText> BlockReductionMethodsTooltips;

	struct FAutoBlocksStrategyOption
	{
		ECustomizableObjectLayoutAutomaticBlocksStrategy Value;
		FText Tooltip;
	};
	TArray<TSharedPtr<FString>> AutoBlocksStrategies;
	TArray<FAutoBlocksStrategyOption> AutoBlocksStrategiesOptions;

	struct FAutoBlocksMergeStrategyOption
	{
		ECustomizableObjectLayoutAutomaticBlocksMergeStrategy Value;
		FText Tooltip;
	};
	TArray<TSharedPtr<FString>> AutoBlocksMergeStrategies;
	TArray<FAutoBlocksMergeStrategyOption> AutoBlocksMergeStrategiesOptions;

};
