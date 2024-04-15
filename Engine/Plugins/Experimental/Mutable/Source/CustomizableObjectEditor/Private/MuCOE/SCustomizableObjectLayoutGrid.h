// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/CustomizableObjectLayout.h"
#include "Widgets/SCompoundWidget.h"

class FPaintArgs;
class FSlateRect;
class FSlateWindowElementList;
class FWidgetStyle;
struct FGeometry;
struct FGuid;
struct FKeyEvent;
struct FPointerEvent;

enum class ECheckBoxState : uint8;

struct FRect2D
{
	FVector2f Min;
	FVector2f Size;
};

typedef enum
{
	ELGM_Show,
	ELGM_Edit,
	ELGM_Select
} ELayoutGridMode;

struct FBlockWidgetData
{
	FRect2D Rect;
	FRect2D HandleRect;
};


enum EFixedReductionOptions
{
	EFRO_Symmetry,
	EFRO_RedyceByTwo
};


class SCustomizableObjectLayoutGrid : public SCompoundWidget
{

public:
	DECLARE_DELEGATE_TwoParams(FBlockChangedDelegate, FGuid /*BlockId*/, FIntRect /*Block*/);
	DECLARE_DELEGATE_OneParam(FBlockSelectionChangedDelegate, const TArray<FGuid>& );
	DECLARE_DELEGATE(FDeleteBlockDelegate);
	DECLARE_DELEGATE_TwoParams(FAddBlockAtDelegate, FIntPoint, FIntPoint);
	DECLARE_DELEGATE_OneParam(FSetBlockPriority, int32);
	DECLARE_DELEGATE_OneParam(FSetReduceBlockSymmetrically, bool);
	DECLARE_DELEGATE_OneParam(FSetReduceBlockByTwo, bool);

	SLATE_BEGIN_ARGS( SCustomizableObjectLayoutGrid ){}

		SLATE_ATTRIBUTE( FIntPoint, GridSize )
		SLATE_ATTRIBUTE( TArray<FCustomizableObjectLayoutBlock>, Blocks )
		SLATE_ARGUMENT( TArray<FVector2f>, UVLayout  )
		SLATE_ARGUMENT( TArray<FVector2f>, UnassignedUVLayoutVertices )
		SLATE_ARGUMENT( ELayoutGridMode, Mode  )
		SLATE_ARGUMENT( FColor, SelectionColor  )
		SLATE_EVENT( FBlockChangedDelegate, OnBlockChanged )
		SLATE_EVENT( FBlockSelectionChangedDelegate, OnSelectionChanged )
		SLATE_EVENT(FDeleteBlockDelegate, OnDeleteBlocks)
		SLATE_EVENT(FAddBlockAtDelegate, OnAddBlockAt)
		SLATE_EVENT(FSetBlockPriority, OnSetBlockPriority)
		SLATE_EVENT(FSetReduceBlockSymmetrically, OnSetReduceBlockSymmetrically)
		SLATE_EVENT(FSetReduceBlockByTwo, OnSetReduceBlockByTwo)

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );
	virtual ~SCustomizableObjectLayoutGrid() override;

	// SWidgetInterface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;

	// Own interface

	/** Set the currently selected block */
	void SetSelectedBlock( FGuid block );
	void SetSelectedBlocks( const TArray<FGuid>& blocks );

	/** */
	const TArray<FGuid>& GetSelectedBlocks() const;

	/** Calls the delegate to delete the selected blocks */
	void DeleteSelectedBlocks();

	/** Generates a new block at mouse position */
	void GenerateNewBlock(FVector2D MousePosition);

	/** Duplicates the selected blocks */
	void DuplicateBlocks();

	/** Sets the size of the selected blocks to the size of the Grid */
	void SetBlockSizeToMax();

	void CalculateSelectionRect();

	FColor SelectionColor;

	/** Set the grid and blocks to show in the widget. */
	void SetBlocks( const FIntPoint& GridSize, const TArray<FCustomizableObjectLayoutBlock>& Blocks);

	/** Gets the priority value of the selected blocks */
	TOptional<int32> GetBlockPriorityValue() const;

	/** Callback when the priority of a block changes */
	void OnBlockPriorityChanged(int32 InValue);

	/** Callback when symmetry block reduction option changes */
	void OnReduceBlockSymmetricallyChanged(ECheckBoxState InCheckboxState);

	/** Callback when ReduceByTwo block reduction option changes */
	void OnReduceBlockByTwoChanged(ECheckBoxState InCheckboxState);

	/** Gets block reduction value of the selected blocks */
	ECheckBoxState GetReductionMethodBoolValue(EFixedReductionOptions Option) const;

private:

	bool MouseOnBlock(FGuid BlockId, FVector2D MousePosition, bool CheckResizeBlock = false) const;

private:

	/** A delegate to report block changes */
	FBlockChangedDelegate BlockChangedDelegate;
	FBlockSelectionChangedDelegate SelectionChangedDelegate;
	FDeleteBlockDelegate DeleteBlocksDelegate;
	FAddBlockAtDelegate AddBlockAtDelegate;
	FSetBlockPriority OnSetBlockPriority;
	FSetReduceBlockSymmetrically OnSetReduceBlockSymmetrically;
	FSetReduceBlockByTwo OnSetReduceBlockByTwo;

	/** Size of the grid in blocks */
	TAttribute<FIntPoint> GridSize;

	/** Array with all the blocks of the layout */
	TAttribute< TArray<FCustomizableObjectLayoutBlock> > Blocks;

	/** Array with all the UVs to draw in the layout */
	TArray<FVector2f> UVLayout;

	/** Array with all the unassigned UVs */
	TArray<FVector2f> UnassignedUVLayoutVertices;

	/** Layout mode */
	ELayoutGridMode Mode = ELGM_Show;

	float CellSize = 0.0f;

	/** Map to relate Block ids with blocks data */
	TMap<FGuid,FBlockWidgetData> BlockRects;

	/** Interaction status. */
	TArray<FGuid> SelectedBlocks;
	TArray<FGuid> PossibleSelectedBlocks;

	/** Booleans needed for the Block Management */
	/** Indicates when we have dragged the mouse after click */
	bool HasDragged = false;

	/** Indicates when we are dragging the mouse */
	bool Dragging = false;
	
	/** Indicates when we are resizing a block */
	bool Resizing = false;
	
	/** Indicates when we have to change the mouse cursor */
	bool ResizeCursor = false;
	
	/** Indicates when we are making a selection */
	bool Selecting = false;
	
	/** Indicates when we are padding */
	bool Padding = false;

	/** Position where the drag started */
	FVector2D DragStart;

	/** Position where the layout grid starts to be drawn */
	FVector2D DrawOrigin;

	/** Amount of padding since start dragging */
	FVector2D PaddingAmount = FVector2D::Zero();

	/** Position where the padding started */
	FVector2D PaddingStart;

	/** Distance from the origin in the padding movement */
	FVector2D DistanceFromOrigin = FVector2D::Zero();

	/** Level of zoom */
	int32 Zoom = 1;

	/** Selection Rectangle */
	FRect2D SelectionRect;

	/** Position where the Selection Rectangle started */
	FVector2D InitSelectionRect;

	/** Current mouse position */
	FVector2D CurrentMousePosition;

	/** Custom Slate drawing element. Used to improve the UVs drawing performance. */
	TSharedPtr<class FUVCanvasDrawer> UVCanvasDrawer;
};
