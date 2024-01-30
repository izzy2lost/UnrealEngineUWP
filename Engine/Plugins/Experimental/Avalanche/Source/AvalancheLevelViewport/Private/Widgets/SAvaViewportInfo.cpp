// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaViewportInfo.h"
#include "AvaViewportUtils.h"
#include "AvaVisibleArea.h"
#include "EditorModeManager.h"
#include "Styling/StyleColors.h"
#include "Toolkits/IToolkitHost.h"
#include "ViewportClient/IAvaViewportClient.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SAvaViewportInfo"

TSharedRef<SAvaViewportInfo> SAvaViewportInfo::CreateInstance(const TSharedRef<IToolkitHost>& InToolkitHost)
{
	return SNew(SAvaViewportInfo, InToolkitHost);
}

void SAvaViewportInfo::PrivateRegisterAttributes(struct FSlateAttributeDescriptor::FInitializer&)
{
}

void SAvaViewportInfo::Construct(const FArguments& Args, const TSharedRef<IToolkitHost>& InToolkitHost)
{
	ToolkitHostWeak = InToolkitHost;

	ChildSlot
	[
		SNew(SBox)
		.Padding(FMargin(3.f, 3.f, 3.f, 10.f))
		[
			SNew(SGridPanel)
			+SGridPanel::Slot(1, 0)
			.Padding(FMargin(3.f))
			.HAlign(EHorizontalAlignment::HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Viewport","Viewport"))
			]
			+SGridPanel::Slot(2, 0)
			.Padding(FMargin(3.f))
			.HAlign(EHorizontalAlignment::HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Virtual","Virtual"))
			]
			+SGridPanel::Slot(0, 1)
			.Padding(FMargin(3.f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Size","Size:"))
			]
			+SGridPanel::Slot(1, 1)
			.Padding(FMargin(5.f, 3.f, 5.f, 3.f))
			.HAlign(EHorizontalAlignment::HAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SAvaViewportInfo::GetViewportSize)
				.ColorAndOpacity(FStyleColors::AccentBlue.GetSpecifiedColor())
			]
			+SGridPanel::Slot(2, 1)
			.Padding(FMargin(5.f, 3.f, 5.f, 3.f))
			.HAlign(EHorizontalAlignment::HAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SAvaViewportInfo::GetVirtualViewportSize)
				.ColorAndOpacity(FStyleColors::AccentGreen.GetSpecifiedColor())
			]
			+SGridPanel::Slot(0, 2)
			.Padding(FMargin(3.f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("VisibleArea","Visible Area:"))
			]
			+SGridPanel::Slot(1, 2)
			.Padding(FMargin(5.f, 3.f, 5.f, 3.f))
			.HAlign(EHorizontalAlignment::HAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SAvaViewportInfo::GetViewportVisibleAreaSize)
				.ColorAndOpacity(FStyleColors::AccentBlue.GetSpecifiedColor())
			]
			+SGridPanel::Slot(2, 2)
			.Padding(FMargin(5.f, 3.f, 5.f, 3.f))
			.HAlign(EHorizontalAlignment::HAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SAvaViewportInfo::GetCanvasVisibleAreaSize)
				.ColorAndOpacity(FStyleColors::AccentGreen.GetSpecifiedColor())
			]
			+SGridPanel::Slot(0, 3)
			.Padding(FMargin(3.f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CenterOffset","Center Offset:"))
			]
			+SGridPanel::Slot(1, 3)
			.Padding(FMargin(5.f, 3.f, 5.f, 3.f))
			.HAlign(EHorizontalAlignment::HAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SAvaViewportInfo::GetViewportZoomOffset)
				.ColorAndOpacity(FStyleColors::AccentBlue.GetSpecifiedColor())
			]
			+SGridPanel::Slot(2, 3)
			.Padding(FMargin(5.f, 3.f, 5.f, 3.f))
			.HAlign(EHorizontalAlignment::HAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SAvaViewportInfo::GetCanvasZoomOffset)
				.ColorAndOpacity(FStyleColors::AccentGreen.GetSpecifiedColor())
			]
			+SGridPanel::Slot(0, 4)
			.Padding(FMargin(3.f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Cursor","Cursor:"))
			]
			+SGridPanel::Slot(1, 4)
			.Padding(FMargin(5.f, 3.f, 5.f, 3.f))
			.HAlign(EHorizontalAlignment::HAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SAvaViewportInfo::GetMouseLocation)
				.ColorAndOpacity(FStyleColors::AccentBlue.GetSpecifiedColor())
			]
			+SGridPanel::Slot(2, 4)
			.Padding(FMargin(5.f, 3.f, 5.f, 3.f))
			.HAlign(EHorizontalAlignment::HAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SAvaViewportInfo::GetVirtualMouseLocation)
				.ColorAndOpacity(FStyleColors::AccentGreen.GetSpecifiedColor())
			]
			+SGridPanel::Slot(0, 5)
			.Padding(FMargin(3.f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Zoom","Zoom:"))
			]
			+SGridPanel::Slot(1, 5)
			.Padding(FMargin(3.f))
			.HAlign(EHorizontalAlignment::HAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SAvaViewportInfo::GetZoomLevel)
			]
		]
	];
}

FVector2f SAvaViewportInfo::GetViewportSizeForActiveViewport() const
{
	static const FVector2f Invalid = FVector2f::ZeroVector;

	TSharedPtr<IToolkitHost> ToolkitHost = ToolkitHostWeak.Pin();

	if (!ToolkitHost.IsValid())
	{
		return Invalid;
	}

	FEditorViewportClient* EditorViewportClient = ToolkitHost->GetEditorModeManager().GetFocusedViewportClient();

	if (TSharedPtr<IAvaViewportClient> AvaViewportClient = FAvaViewportUtils::GetAsAvaViewportClient(EditorViewportClient))
	{
		return AvaViewportClient->GetViewportSize();
	}

	return Invalid;
}

FIntPoint SAvaViewportInfo::GetVirtualSizeForActiveViewport() const
{
	static const FIntPoint Invalid = FIntPoint::ZeroValue;

	TSharedPtr<IToolkitHost> ToolkitHost = ToolkitHostWeak.Pin();

	if (!ToolkitHost.IsValid())
	{
		return Invalid;
	}

	FEditorViewportClient* EditorViewportClient = ToolkitHost->GetEditorModeManager().GetFocusedViewportClient();

	if (TSharedPtr<IAvaViewportClient> AvaViewportClient = FAvaViewportUtils::GetAsAvaViewportClient(EditorViewportClient))
	{
		return AvaViewportClient->GetVirtualViewportSize();
	}

	return Invalid;
}

FAvaVisibleArea SAvaViewportInfo::GetVisibleAreaForActiveViewport() const
{
	static const FAvaVisibleArea Invalid = FAvaVisibleArea();

	TSharedPtr<IToolkitHost> ToolkitHost = ToolkitHostWeak.Pin();

	if (!ToolkitHost.IsValid())
	{
		return Invalid;
	}

	FEditorViewportClient* EditorViewportClient = ToolkitHost->GetEditorModeManager().GetFocusedViewportClient();

	if (TSharedPtr<IAvaViewportClient> AvaViewportClient = FAvaViewportUtils::GetAsAvaViewportClient(EditorViewportClient))
	{
		return AvaViewportClient->GetZoomedVisibleArea();
	}

	return Invalid;
}

FVector2f SAvaViewportInfo::GetMouseLocationOnViewport() const
{
	static const FVector2f Invalid = FVector2f(-1, -1);

	TSharedPtr<IToolkitHost> ToolkitHost = ToolkitHostWeak.Pin();

	if (!ToolkitHost.IsValid())
	{
		return Invalid;
	}

	FEditorViewportClient* EditorViewportClient = ToolkitHost->GetEditorModeManager().GetFocusedViewportClient();

	if (TSharedPtr<IAvaViewportClient> AvaViewportClient = FAvaViewportUtils::GetAsAvaViewportClient(EditorViewportClient))
	{
		return AvaViewportClient->GetConstrainedViewportMousePosition();
	}

	return Invalid;
}

FText SAvaViewportInfo::GetViewportSize() const
{
	static const FText Default = LOCTEXT("NoSize", "-");

	const FVector2f& ViewportSize = GetViewportSizeForActiveViewport();

	if (FAvaViewportUtils::IsValidViewportSize(ViewportSize))
	{
		return FText::Format(
			LOCTEXT("Size", "{0} x {1}"), 
			FMath::RoundToInt(ViewportSize.X), 
			FMath::RoundToInt(ViewportSize.Y)
		);
	}

	return Default;
}

FText SAvaViewportInfo::GetVirtualViewportSize() const
{
	static const FText Default = LOCTEXT("NoSize", "-");

	const FIntPoint VirtualSize = GetVirtualSizeForActiveViewport();

	if (FAvaViewportUtils::IsValidViewportSize(VirtualSize))
	{
		return FText::Format(
			LOCTEXT("Size", "{0} x {1}"),
			VirtualSize.X,
			VirtualSize.Y
		);
	}

	return Default;
}

FText SAvaViewportInfo::GetViewportVisibleAreaSize() const
{
	static const FText Default = LOCTEXT("NoSize", "-");

	const FAvaVisibleArea VisibleArea = GetVisibleAreaForActiveViewport();

	if (VisibleArea.IsValid() && VisibleArea.IsZoomedView())
	{
		return FText::Format(
			LOCTEXT("Size", "{0} x {1}"),
			FMath::RoundToInt(VisibleArea.VisibleSize.X),
			FMath::RoundToInt(VisibleArea.VisibleSize.Y)
		);
	}

	return Default;
}

FText SAvaViewportInfo::GetCanvasVisibleAreaSize() const
{
	static const FText Default = LOCTEXT("NoSize", "-");

	const FAvaVisibleArea VisibleArea = GetVisibleAreaForActiveViewport();

	if (VisibleArea.IsValid() && VisibleArea.IsZoomedView())
	{
		const FIntPoint CanvasSize = GetVirtualSizeForActiveViewport();

		if (FAvaViewportUtils::IsValidViewportSize(CanvasSize))
		{
			const FVector2f VisibleSize = VisibleArea.VisibleSize * static_cast<float>(CanvasSize.X) / VisibleArea.AbsoluteSize.X;

			return FText::Format(
				LOCTEXT("Size", "{0} x {1}"),
				FMath::RoundToInt(VisibleSize.X),
				FMath::RoundToInt(VisibleSize.Y)
			);
		}
	}

	return Default;
}

FText SAvaViewportInfo::GetViewportZoomOffset() const
{
	static const FText Default = LOCTEXT("NoCoordinates", "-");

	const FAvaVisibleArea VisibleArea = GetVisibleAreaForActiveViewport();

	if (VisibleArea.IsValid() && VisibleArea.IsOffset())
	{
		return FText::Format(
			LOCTEXT("Coordinates", "{0}, {1}"),
			FMath::RoundToInt(VisibleArea.Offset.X),
			FMath::RoundToInt(VisibleArea.Offset.Y)
		);
	}

	return Default;
}

FText SAvaViewportInfo::GetCanvasZoomOffset() const
{
	static const FText Default = LOCTEXT("NoCoordinates", "-");

	const FAvaVisibleArea VisibleArea = GetVisibleAreaForActiveViewport();

	if (VisibleArea.IsValid() && VisibleArea.IsOffset())
	{
		const FIntPoint CanvasSize = GetVirtualSizeForActiveViewport();

		if (FAvaViewportUtils::IsValidViewportSize(CanvasSize))
		{
			const FVector2f CanvasOffset = VisibleArea.Offset * static_cast<float>(CanvasSize.X) / VisibleArea.AbsoluteSize.X;

			return FText::Format(
				LOCTEXT("Coordinates", "{0}, {1}"),
				FMath::RoundToInt(CanvasOffset.X),
				FMath::RoundToInt(CanvasOffset.Y)
			);
		}
	}

	return Default;
}

FText SAvaViewportInfo::GetMouseLocation() const
{
	static const FText Default = LOCTEXT("NoCoordinates", "-");

	const FVector2f MouseLocation = GetMouseLocationOnViewport();

	// -1 is the invalid value
	if (MouseLocation.X < 0 || MouseLocation.Y < 0)
	{
		return Default;
	}

	return FText::Format(
		LOCTEXT("Coordinates", "{0}, {1}"),
		FMath::RoundToInt(MouseLocation.X),
		FMath::RoundToInt(MouseLocation.Y)
	);
}

FText SAvaViewportInfo::GetVirtualMouseLocation() const
{
	static const FText Default = LOCTEXT("NoCoordinates", "-");

	FVector2f MouseLocation = GetMouseLocationOnViewport();

	// -1 is the invalid value
	if (MouseLocation.X < 0 || MouseLocation.Y < 0)
	{
		return Default;
	}

	const FVector2f ViewportSize = GetViewportSizeForActiveViewport();

	if (!FAvaViewportUtils::IsValidViewportSize(ViewportSize))
	{
		return Default;
	}

	const FIntPoint CanvasSize = GetVirtualSizeForActiveViewport();

	if (!FAvaViewportUtils::IsValidViewportSize(CanvasSize))
	{
		return Default;
	}

	MouseLocation *= static_cast<float>(CanvasSize.X) / ViewportSize.X;

	return FText::Format(
		LOCTEXT("Coordinates", "{0}, {1}"),
		FMath::RoundToInt(MouseLocation.X),
		FMath::RoundToInt(MouseLocation.Y)
	);
}

FText SAvaViewportInfo::GetZoomLevel() const
{
	static const FText Default = LOCTEXT("No value", "-");

	const FAvaVisibleArea VisibleArea = GetVisibleAreaForActiveViewport();

	if (VisibleArea.IsValid())
	{
		const int32 Percentage = FMath::RoundToInt(VisibleArea.GetVisibleAreaFraction() * 100.f);

		return FText::Format(
			LOCTEXT("PercentFormat", "{0}%"),
			FText::AsNumber(Percentage)
		);
	}

	return Default;
}

#undef LOCTEXT_NAMESPACE
