// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"
#include "MuCOE/SCustomizableObjectLayoutGrid.h"

namespace ESelectInfo { enum Type : int; }

class FReferenceCollector;
class ICustomizableObjectInstanceEditor;
class ISlateStyle;
class SWidget;
class STextComboBox;
class SVerticalBox;
class SHorizontalBox;
class FUICommandList;
class SCustomizableObjectLayoutGrid;

struct FCustomizableObjectLayoutBlock;
struct FGuid;


/**
 * CustomizableObject Editor Preview viewport widget
 */
class SCustomizableObjectNodeLayoutBlocksEditor : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS( SCustomizableObjectNodeLayoutBlocksEditor ){}
	SLATE_END_ARGS()

	SCustomizableObjectNodeLayoutBlocksEditor();

	void Construct(const FArguments& InArgs);
	
	// FSerializableObject interface
	void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SCustomizableObjectNodeLayoutBlocksEditor");
	}
	// End of FSerializableObject interface


	/** Binds commands associated with the viewport client. */
	void BindCommands();

	/**  
	* The optional UVOverrideLayout parameter can be speicifed to show different UVs in the widget instead of the ones in Layout.
	*/
	void SetCurrentLayout( class UCustomizableObjectLayout* Layout, UCustomizableObjectLayout* UVOverrideLayout=nullptr );

private:

	/** Layout whose blocksa re being edited. */
	TObjectPtr<class UCustomizableObjectLayout> CurrentLayout;

	/** If valid, layout use to show the UVs instead of CurrentLayout. */
	TObjectPtr<class UCustomizableObjectLayout> UVOverrideLayout;

	/** */
	TSharedPtr<SCustomizableObjectLayoutGrid> LayoutGridWidget;

	/** The list of UI Commands executable */
	TSharedRef<FUICommandList> UICommandList;

private:

	/** */
	ELayoutGridMode GetGridMode() const;
	FIntPoint GetGridSize() const;
	void OnBlockChanged(FGuid BlockId, FIntRect Block );
	TArray<FCustomizableObjectLayoutBlock> GetBlocks() const;

	/** Callbacks from the layout block editor. */
	TSharedRef<SWidget> BuildLayoutToolBar();

	void OnAddBlock();
	void OnAddBlockAt(const FIntPoint Min, const FIntPoint Max);
	void OnRemoveBlock();

	/** Turn the automatic layout blocks into user-created blocks. */
	void OnConsolidateBlocks();

	/** Sets the block priority from the input text. */
	void OnSetBlockPriority(int32 InValue);

	/** Sets the block reduction symmetry option. */
	void OnSetBlockReductionSymmetry(bool bInValue);

	/** Sets the block reduction ReduceByTwo option. */
	void OnSetBlockReductionByTwo(bool bInValue);

	/** Callback for block mask change. */
	void OnSetBlockMask(class UTexture2D* InValue);

	TSharedPtr<IToolTip> GenerateInfoToolTip() const;

};
