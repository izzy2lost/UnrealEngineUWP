// Copyright Epic Games, Inc. All Rights Reserved.

#include "DragDrop/DMXPixelMappingDragDropOp.h"

#include "Components/DMXPixelMappingMatrixComponent.h"
#include "Components/DMXPixelMappingRendererComponent.h"
#include "DMXPixelMapping.h"
#include "DragDrop/DMXPixelMappingGroupChildDragDropHelper.h"
#include "Editor.h"
#include "Toolkits/DMXPixelMappingToolkit.h"


#define LOCTEXT_NAMESPACE "FDMXPixelMappingDragDropOp"

FDMXPixelMappingDragDropOp::~FDMXPixelMappingDragDropOp()
{
	// End the transaction in any case
	GEditor->EndTransaction();
}

TSharedRef<FDMXPixelMappingDragDropOp> FDMXPixelMappingDragDropOp::New(const TSharedRef<FDMXPixelMappingToolkit>& InToolkit, const FVector2D& InGraphSpaceDragOffset, const TArray<TSharedPtr<FDMXPixelMappingComponentTemplate>>& InTemplates, UDMXPixelMappingBaseComponent* InParent)
{
	TSharedRef<FDMXPixelMappingDragDropOp> Operation = MakeShared<FDMXPixelMappingDragDropOp>();
	
	Operation->WeakToolkit = InToolkit;
	Operation->Templates = InTemplates;
	Operation->bWasCreatedAsTemplate = true;
	Operation->Parent = InParent;
	Operation->GraphSpaceDragOffset = InGraphSpaceDragOffset;
		
	Operation->Construct();
	Operation->SetDecoratorVisibility(false);

	// Create a transaction for dragged templates
	Operation->TransactionIndex = GEditor->BeginTransaction(FText::Format(LOCTEXT("DragDropTemplateTransaction", "PixelMapping: Add {0}|plural(one=Component, other=Components)"), InTemplates.Num()));

	return Operation;
}

TSharedRef<FDMXPixelMappingDragDropOp> FDMXPixelMappingDragDropOp::New(const TSharedRef<FDMXPixelMappingToolkit>& InToolkit, const FVector2D& InGraphSpaceDragOffset, const TArray<TWeakObjectPtr<UDMXPixelMappingBaseComponent>>& InDraggedComponents)
{
	TSharedRef<FDMXPixelMappingDragDropOp> Operation = MakeShared<FDMXPixelMappingDragDropOp>();

	Operation->WeakToolkit = InToolkit;
	Operation->bWasCreatedAsTemplate = false;
	Operation->SetDraggedComponents(InDraggedComponents);
	Operation->GraphSpaceDragOffset = InGraphSpaceDragOffset;
	Operation->GroupChildDragDropHelper = FDMXPixelMappingGroupChildDragDropHelper::Create(Operation); // After setting dragged components

	Operation->Construct();
	Operation->SetDecoratorVisibility(false);

	// Create a transaction for dragged components
	Operation->TransactionIndex = GEditor->BeginTransaction(FText::Format(LOCTEXT("DragDropComponentTransaction", "PixelMapping: Drag {0}|plural(one=Component, other=Components)"), InDraggedComponents.Num()));

	return Operation;
}

void FDMXPixelMappingDragDropOp::SetDraggedComponents(const TArray<TWeakObjectPtr<UDMXPixelMappingBaseComponent>>& InDraggedComponents)
{
	DraggedComponents = InDraggedComponents;
	Templates.Reset();

	// Rebuild the group child drag drop helper
	GroupChildDragDropHelper = FDMXPixelMappingGroupChildDragDropHelper::Create(AsShared());
}

void FDMXPixelMappingDragDropOp::LayoutOutputComponents(const FVector2D& GraphSpacePosition)
{
	UDMXPixelMappingOutputComponent* FirstComponent = DraggedComponents.IsEmpty() ? nullptr : Cast<UDMXPixelMappingOutputComponent>(DraggedComponents[0]);
	if (!FirstComponent)
	{
		return;
	}
	const FVector2D Anchor = FirstComponent->GetPosition();

	// Move all to new position
	for (const TWeakObjectPtr<UDMXPixelMappingBaseComponent>& Component : DraggedComponents)
	{
		if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(Component.Get()))
		{
			OutputComponent->PreEditChange(nullptr);
			
			constexpr bool bModifyChildrenRecursive = true;
			Component->ForEachChild([](UDMXPixelMappingBaseComponent* Component)
				{
					Component->Modify();
				}, bModifyChildrenRecursive);

			if (ensureMsgf(OutputComponent->GetClass() != UDMXPixelMappingMatrixComponent::StaticClass(),
				TEXT("Only matrix components can laid out with FDMXPixelMappingDragDropOp::LayoutOutputComponents. Please use FGroupChildDragDropHelper instead (see FDMXPixelMappingDragDropOp::GetGroupChildDragDropHelper().")))
			{
				const FVector2D AnchorOffset = Anchor - OutputComponent->GetPosition();

				const FVector2D NewPosition = FVector2D(GraphSpacePosition - AnchorOffset - GraphSpaceDragOffset).RoundToVector();
				OutputComponent->SetPosition(NewPosition);
			}
		}
	}
	GridSnap();

	for (const TWeakObjectPtr<UDMXPixelMappingBaseComponent>& Component : DraggedComponents)
	{
		if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(Component.Get()))
		{
			OutputComponent->PostEditChange();
		}
	}
}

void FDMXPixelMappingDragDropOp::GridSnap()
{
	const UDMXPixelMapping* PixelMapping = WeakToolkit.IsValid() ? WeakToolkit.Pin()->GetDMXPixelMapping() : nullptr;
	if (!PixelMapping || DraggedComponents.IsEmpty() || PixelMapping->SnapGridColumns == 0 || PixelMapping->SnapGridRows == 0)
	{
		return;
	}

	// Grid snap the first component only, and move the others by same delta.
	UDMXPixelMappingOutputComponent* FirstComponent = Cast<UDMXPixelMappingOutputComponent>(DraggedComponents[0].Get());
	if (!FirstComponent)
	{
		return;
	}

	UDMXPixelMappingRendererComponent* RendererOfFirstComponent = FirstComponent->GetRendererComponent();
	if (!RendererOfFirstComponent)
	{
		return;
	}

	const FVector2D CellSize = [PixelMapping, RendererOfFirstComponent]()
		{
			if (PixelMapping->bGridSnappingEnabled)
			{
				const FVector2D TextureSize = RendererOfFirstComponent->GetSize();
				return TextureSize / FVector2D(PixelMapping->SnapGridColumns, PixelMapping->SnapGridRows);
			}
			
			// Grid snap to pixels if grid snapping is disabled
			return FVector2D(1.f, 1.f);
		}();

	const int32 Column = FirstComponent->GetPosition().X / CellSize.X;
	const int32 Row = FirstComponent->GetPosition().Y / CellSize.Y;

	const FVector2D GridSnapPosition = FVector2D(Column, Row) * CellSize;
	const FVector2D DeltaVector = GridSnapPosition - FirstComponent->GetPosition();

	FirstComponent->SetPosition(GridSnapPosition);
	for (int32 ComponentIndex = 1; ComponentIndex < DraggedComponents.Num(); ComponentIndex++)
	{
		if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(DraggedComponents[ComponentIndex].Get()))
		{
			OutputComponent->SetPosition(OutputComponent->GetPosition() + DeltaVector);
		}
	}
}

#undef LOCTEXT_NAMESPACE
