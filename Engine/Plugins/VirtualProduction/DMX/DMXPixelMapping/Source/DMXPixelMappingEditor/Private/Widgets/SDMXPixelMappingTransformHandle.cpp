// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SDMXPixelMappingTransformHandle.h"

#include "Components/DMXPixelMappingMatrixCellComponent.h"
#include "Components/DMXPixelMappingRendererComponent.h"
#include "DMXPixelMapping.h"
#include "Framework/Application/SlateApplication.h"
#include "ScopedTransaction.h"
#include "Settings/DMXPixelMappingEditorSettings.h"
#include "Toolkits/DMXPixelMappingToolkit.h"
#include "Views/SDMXPixelMappingDesignerView.h"
#include "Widgets/Images/SImage.h"


#define LOCTEXT_NAMESPACE "SDMXPixelMappingTransformHandle"

void SDMXPixelMappingTransformHandle::Construct(const FArguments& InArgs, TSharedPtr<SDMXPixelMappingDesignerView> InDesignerView, EDMXPixelMappingTransformDirection InTransformDirection, TAttribute<FVector2D> InOffset)
{
	TransformDirection = InTransformDirection;
	DesignerViewWeakPtr = InDesignerView;
	Offset = InOffset;

	Action = EDMXPixelMappingTransformAction::None;
	ScopedTransaction = nullptr;

	DragDirection = ComputeDragDirection(InTransformDirection);
	DragOrigin = ComputeOrigin(InTransformDirection);

	ChildSlot
	[
		SNew(SImage)
		.Visibility(this, &SDMXPixelMappingTransformHandle::GetHandleVisibility)
		.Image(FAppStyle::Get().GetBrush("UMGEditor.TransformHandle"))
	];
}

EVisibility SDMXPixelMappingTransformHandle::GetHandleVisibility() const
{
	return EVisibility::Visible;
}

FReply SDMXPixelMappingTransformHandle::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FVector2D LocalSize = MyGeometry.GetLocalSize();
		const FVector2D LocalCursorPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

		// Only handle dragging the edges of the widget
		constexpr double MouseThreshold = 10.f;
		if (LocalCursorPos.X < LocalSize.X - MouseThreshold ||
			LocalCursorPos.Y < LocalSize.Y - MouseThreshold)
		{
			return FReply::Unhandled();;
		}

		Action = ComputeActionAtLocation(MyGeometry, MouseEvent);

		if (TSharedPtr<FDMXPixelMappingToolkit> Toolkit = DesignerViewWeakPtr.Pin()->GetToolkit())
		{
			const TSet<FDMXPixelMappingComponentReference> SelectedComponentReferences = Toolkit->GetSelectedComponents();
			for (const FDMXPixelMappingComponentReference& ComponentReference : SelectedComponentReferences)
			{
				UDMXPixelMappingBaseComponent* Component = ComponentReference.GetComponent();

				if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(Component))
				{
					FMargin Offsets;
					FVector2D Size = OutputComponent->GetSize();
					Offsets.Right = Size.X;
					Offsets.Bottom = Size.Y;
					StartingOffsets = Offsets;
				}

				MouseDownPosition = MouseEvent.GetScreenSpacePosition();

				ScopedTransaction = MakeShareable<FScopedTransaction>(new FScopedTransaction(LOCTEXT("ResizePixelMappingComponent", "PixelMapping: Resize Component")));
				Component->Modify();

				return FReply::Handled().CaptureMouse(SharedThis(this));
			}
		}
	}

	return FReply::Unhandled();
}

FReply SDMXPixelMappingTransformHandle::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( HasMouseCapture() && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		Action = EDMXPixelMappingTransformAction::None;
		
		if (TSharedPtr<FDMXPixelMappingToolkit> Toolkit = DesignerViewWeakPtr.Pin()->GetToolkit())
		{		
			const TSet<FDMXPixelMappingComponentReference> SelectedComponentReferences = Toolkit->GetSelectedComponents();
			for (const FDMXPixelMappingComponentReference& ComponentReference : SelectedComponentReferences)
			{
				if (UDMXPixelMappingOutputComponent* ResizedComponent = Cast<UDMXPixelMappingOutputComponent>(ComponentReference.GetComponent()))
				{
					// Set the final size transacted
					const FVector2D Delta = MouseEvent.GetScreenSpacePosition() - MouseDownPosition;
					const FVector2D TranslateAmount = Delta * (1.0f / (DesignerViewWeakPtr.Pin()->GetZoomAmount() * MyGeometry.Scale));

					ResizedComponent->Modify();
					Resize(ResizedComponent, DragDirection, TranslateAmount);

					ScopedTransaction.Reset();

					return FReply::Handled().ReleaseMouseCapture();
				}
			}
		}
	}

	return FReply::Unhandled();
}

FReply SDMXPixelMappingTransformHandle::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (Action != EDMXPixelMappingTransformAction::None && DesignerViewWeakPtr.IsValid())
	{
		if (TSharedPtr<FDMXPixelMappingToolkit> Toolkit = DesignerViewWeakPtr.Pin()->GetToolkit())
		{
			const TSet<FDMXPixelMappingComponentReference> SelectedComponentReferences = Toolkit->GetSelectedComponents();
			for (const FDMXPixelMappingComponentReference& ComponentReference : SelectedComponentReferences)
			{
				if (UDMXPixelMappingOutputComponent* ResizedComponent = Cast<UDMXPixelMappingOutputComponent>(ComponentReference.GetComponent()))
				{
					RequestResize(ResizedComponent, DragDirection);
				}
			}
		}
	}

	return FReply::Unhandled();
}

FCursorReply SDMXPixelMappingTransformHandle::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	switch (TransformDirection)
	{
	case EDMXPixelMappingTransformDirection::BottomRight:
		return FCursorReply::Cursor(EMouseCursor::ResizeSouthEast);
	case EDMXPixelMappingTransformDirection::BottomLeft:
		return FCursorReply::Cursor(EMouseCursor::ResizeSouthWest);
	case EDMXPixelMappingTransformDirection::BottomCenter:
		return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
	case EDMXPixelMappingTransformDirection::CenterRight:
		return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	}

	return FCursorReply::Unhandled();
}

void SDMXPixelMappingTransformHandle::RequestResize(UDMXPixelMappingBaseComponent* BaseComponent, const FVector2D& Direction)
{
	if (!RequestResizeHandle.IsValid() && DesignerViewWeakPtr.IsValid())
	{
		const FVector2D& CursorPosition = FSlateApplication::Get().GetCursorPos();
		const FVector2D Delta = CursorPosition - MouseDownPosition;
		const FVector2D TranslateAmount = Delta * (1.0f / (DesignerViewWeakPtr.Pin()->GetZoomAmount() * GetCachedGeometry().Scale));

		RequestResizeHandle = GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateLambda([this, BaseComponent, Direction, TranslateAmount]()
			{
				Resize(BaseComponent, Direction, TranslateAmount);
				RequestResizeHandle.Invalidate();
			}));
	}

}

void SDMXPixelMappingTransformHandle::Resize(UDMXPixelMappingBaseComponent* BaseComponent, const FVector2D& Direction, const FVector2D& Amount)
{
	if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(BaseComponent))
	{
		FMargin Offsets = StartingOffsets;

		const FVector2D Movement = Amount * Direction;
		if (Direction.X < 0)
		{
			Offsets.Left -= Movement.X;
			Offsets.Right += Movement.X;
		}

		if (Direction.Y < 0)
		{
			Offsets.Top -= Movement.Y;
			Offsets.Bottom += Movement.Y;
		}

		if (Direction.X > 0)
		{
			Offsets.Left += Movement.X;
			Offsets.Right += Amount.X * Direction.X;
		}

		if (Direction.Y > 0)
		{
			Offsets.Top += Movement.Y;
			Offsets.Bottom += Amount.Y * Direction.Y;
		}

		const FVector2D OldSize = OutputComponent->GetSize();
		const FVector2D RequestedSize = FVector2D(Offsets.Right, Offsets.Bottom);
		const FVector2D NewSize = GetSnapSize(OutputComponent, RequestedSize, Direction);
		if (OldSize == NewSize)
		{
			// No unchanged values
			return;
		}
		OutputComponent->Modify();
		OutputComponent->SetSize(NewSize);

		// Scale children only if desired, no division by zero
		const FDMXPixelMappingDesignerSettings& DesignerSettings = GetDefault<UDMXPixelMappingEditorSettings>()->DesignerSettings;
		if (!DesignerSettings.bScaleChildrenWithParent || NewSize == FVector2D::ZeroVector)
		{
			return;
		}

		// Scale children with parent. 
		FVector2D RatioVector = NewSize / OldSize;
		for (UDMXPixelMappingBaseComponent* BaseChild : OutputComponent->GetChildren())
		{
			if (UDMXPixelMappingOutputComponent* Child = Cast<UDMXPixelMappingOutputComponent>(BaseChild))
			{
				Child->Modify();

				// Scale size, at least 1x1 pixel
				FVector2D NewChildSize = Child->GetSize() * RatioVector;
				NewChildSize.X = FMath::Max(NewChildSize.X, 1.f);
				NewChildSize.Y = FMath::Max(NewChildSize.Y, 1.f);
				Child->SetSize(NewChildSize);

				// Scale position
				const FVector2D NewChildPosition = Child->GetPosition();
				const FVector2D NewChildPositionRelative = (NewChildPosition - OutputComponent->GetPosition()) * RatioVector;
				Child->SetPosition(OutputComponent->GetPosition() + NewChildPositionRelative);
			}
		}
	}
}

FVector2D SDMXPixelMappingTransformHandle::GetSnapSize(UDMXPixelMappingOutputComponent* OutputComponent, const FVector2D& RequestedSize, const FVector2D& Direction) const
{
	const TSharedPtr<FDMXPixelMappingToolkit> Toolkit = DesignerViewWeakPtr.Pin()->GetToolkit();
	if (!Toolkit.IsValid() || !OutputComponent || !OutputComponent->GetRendererComponent())
	{
		return RequestedSize;
	}
	UDMXPixelMappingRendererComponent* RendererComponent = OutputComponent->GetRendererComponent();

	const UDMXPixelMapping* PixelMapping = Toolkit->GetDMXPixelMapping();
	if (!PixelMapping)
	{
		return RequestedSize;
	}

	const FVector2D CellSize = [PixelMapping, RendererComponent]()
		{
			if (PixelMapping->bGridSnappingEnabled)
			{
				const FVector2D TextureSize = RendererComponent->GetSize();
				return TextureSize / FVector2D(PixelMapping->SnapGridColumns, PixelMapping->SnapGridRows);
			}

			// Grid snap to pixels if grid snapping is disabled
			return FVector2D(1.f, 1.f);
		}();

	// Grid snap bottom right
	const int32 BottomColumn = (OutputComponent->GetPosition().X + RequestedSize.X) / CellSize.X + 1;
	const int32 RightRow = (OutputComponent->GetPosition().Y + RequestedSize.Y) / CellSize.Y + 1;

	FVector2D NewBottomRight = OutputComponent->GetPosition() + RequestedSize;
	if (Direction.X < 0.f)
	{
		// Snap towards left
		NewBottomRight.X = (BottomColumn - 1) * CellSize.X;
	}
	else if (Direction.X > 0.f)
	{
		// Snap towards right
		NewBottomRight.X = BottomColumn * CellSize.X;
	}

	if (Direction.Y < 0.f)
	{
		// Snap towards top
		NewBottomRight.Y = (RightRow - 1) * CellSize.Y;
	}
	else if (Direction.Y > 0.f)
	{
		// Snap towards bottom
		NewBottomRight.Y = RightRow * CellSize.Y;
	}

	const FVector2D SnapSize = NewBottomRight - OutputComponent->GetPosition();
	return SnapSize;
}

FVector2D SDMXPixelMappingTransformHandle::ComputeDragDirection(EDMXPixelMappingTransformDirection InTransformDirection) const
{
	switch ( InTransformDirection )
	{
	case EDMXPixelMappingTransformDirection::CenterRight:
		return FVector2D(1, 0);

	case EDMXPixelMappingTransformDirection::BottomLeft:
		return FVector2D(-1, 1);
	case EDMXPixelMappingTransformDirection::BottomCenter:
		return FVector2D(0, 1);
	case EDMXPixelMappingTransformDirection::BottomRight:
		return FVector2D(1, 1);
	}

	return FVector2D(0, 0);
}

FVector2D SDMXPixelMappingTransformHandle::ComputeOrigin(EDMXPixelMappingTransformDirection InTransformDirection) const
{
	FVector2D Size(10, 10);

	switch ( InTransformDirection )
	{
	case EDMXPixelMappingTransformDirection::CenterRight:
		return Size * FVector2D(0, 0.5);

	case EDMXPixelMappingTransformDirection::BottomLeft:
		return Size * FVector2D(1, 0);
	case EDMXPixelMappingTransformDirection::BottomCenter:
		return Size * FVector2D(0.5, 0);
	case EDMXPixelMappingTransformDirection::BottomRight:
		return Size * FVector2D(0, 0);
	}

	return FVector2D(0, 0);
}

EDMXPixelMappingTransformAction SDMXPixelMappingTransformHandle::ComputeActionAtLocation(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	FVector2D GrabOriginOffset = LocalPosition - DragOrigin;
	if ( GrabOriginOffset.SizeSquared() < 36.f )
	{
		return EDMXPixelMappingTransformAction::Primary;
	}
	else
	{
		return EDMXPixelMappingTransformAction::Secondary;
	}
}

#undef LOCTEXT_NAMESPACE
