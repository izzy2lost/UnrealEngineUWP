// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXPixelMappingDesignerToolbar.h"

#include "DMXPixelMapping.h"
#include "DMXPixelMappingEditorCommands.h"
#include "Styling/AppStyle.h"
#include "Styling/ToolBarStyle.h"
#include "SViewportToolBarComboMenu.h"
#include "Toolkits/DMXPixelMappingToolkit.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SDMXPixelMappingSnapGridMenu.h"


#define LOCTEXT_NAMESPACE "SDMXPixelMappingDesignerToolbar"

namespace UE::DMX
{
	void SDMXPixelMappingDesignerToolbar::Construct(const FArguments& InArgs, const TSharedRef<FDMXPixelMappingToolkit>& InToolkit)
	{
		WeakToolkit = InToolkit;

		constexpr TCHAR ToolBarStyleName[] = TEXT("EditorViewportToolBar");
		const FToolBarStyle& ToolBarStyle = FAppStyle::Get().GetWidgetStyle<FToolBarStyle>(ToolBarStyleName);

		const TSharedRef<FUICommandList> CommandList = InToolkit->GetToolkitCommands();

		const FUICommandInfo* ToggleSnapGridCommand = FDMXPixelMappingEditorCommands::Get().ToggleGridSnapping.Get();
		check(ToggleSnapGridCommand);

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			.ForegroundColor(FAppStyle::GetSlateColor("DefaultForeground"))
			[
				SNew(SHorizontalBox)

				// Grid snapping
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.f, 2.0f)
				[
					SNew(SViewportToolBarComboMenu)
					.Cursor(EMouseCursor::Default)
					.IsChecked(this, &SDMXPixelMappingDesignerToolbar::GetSnapGridEnabledCheckState)
					.OnCheckStateChanged(this, &SDMXPixelMappingDesignerToolbar::OnSnapGridCheckStateChanged)
					.Label(this, &SDMXPixelMappingDesignerToolbar::GetSnapGridLabel)
					.OnGetMenuContent(this, &SDMXPixelMappingDesignerToolbar::GenerateSnapGridMenu)
					.ToggleButtonToolTip(ToggleSnapGridCommand->GetDescription())
					.MenuButtonToolTip(LOCTEXT("SnapGridMenuTooltip", "Grid Snapping Settings"))
					.Icon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.LocationGridSnap"))
					.ParentToolBar(SharedThis(this))
				]

				// Zoom to fit
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.f, 2.0f)
				[
					SNew(SButton)
					.ButtonStyle(&ToolBarStyle.ButtonStyle)
					.ToolTipText(LOCTEXT("ZoomToFit_ToolTip", "Zoom To Fit"))
					.OnClicked(InArgs._OnZoomToFitClicked)
					.ContentPadding(ToolBarStyle.ButtonPadding)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("UMGEditor.ZoomToFit"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
			]
		];
	}

	TSharedRef<SWidget> SDMXPixelMappingDesignerToolbar::GenerateSnapGridMenu()
	{
		if (!WeakToolkit.IsValid())
		{
			return SNullWidget::NullWidget;
		}

		const TSharedRef<SWidget> SnapGridMenu = 
			SNew(SDMXPixelMappingSnapGridMenu, WeakToolkit.Pin().ToSharedRef())
			.IsChecked(this, &SDMXPixelMappingDesignerToolbar::GetSnapGridEnabledCheckState)
			.OnCheckStateChanged(this, &SDMXPixelMappingDesignerToolbar::OnSnapGridCheckStateChanged);

		return SnapGridMenu;
	}

	FText SDMXPixelMappingDesignerToolbar::GetSnapGridLabel() const
	{
		UDMXPixelMapping* PixelMapping = WeakToolkit.IsValid() ? WeakToolkit.Pin()->GetDMXPixelMapping() : nullptr;
		if (PixelMapping)
		{
			if (PixelMapping->bGridSnappingEnabled)
			{
				const FString ColumnString = FString::FromInt(PixelMapping->SnapGridColumns);
				const FString RowString = FString::FromInt(PixelMapping->SnapGridRows);

				return FText::FromString(ColumnString + TEXT("x") + RowString);
			}

			return FText::GetEmpty();
		}

		return FText::GetEmpty();
	}

	ECheckBoxState SDMXPixelMappingDesignerToolbar::GetSnapGridEnabledCheckState() const
	{
		UDMXPixelMapping* PixelMapping = WeakToolkit.IsValid() ? WeakToolkit.Pin()->GetDMXPixelMapping() : nullptr;
		if (PixelMapping)
		{
			return PixelMapping->bGridSnappingEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}

		return ECheckBoxState::Undetermined;
	}

	void SDMXPixelMappingDesignerToolbar::OnSnapGridCheckStateChanged(ECheckBoxState NewCheckBoxState)
	{
		if (const TSharedPtr<FDMXPixelMappingToolkit> Toolkit = WeakToolkit.Pin())
		{
			Toolkit->ToggleGridSnapping();
		}
	}
}


#undef LOCTEXT_NAMESPACE
