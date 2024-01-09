// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXPixelMappingDesignerToolbar.h"

#include "DMXPixelMapping.h"
#include "DMXPixelMappingEditorCommands.h"
#include "Styling/AppStyle.h"
#include "Styling/ToolBarStyle.h"
#include "SViewportToolBarComboMenu.h"
#include "ToolMenus.h"
#include "Toolkits/DMXPixelMappingToolkit.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SDMXPixelMappingSnapGridMenu.h"


#define LOCTEXT_NAMESPACE "SDMXPixelMappingDesignerToolbar"


namespace UE::DMX
{
	namespace DMXPixelMappingDesignerToolbar::Private
	{
		constexpr TCHAR ToolbarName[] = TEXT("PixelMapping.DesignerToolbar");
	}

	SDMXPixelMappingDesignerToolbar::~SDMXPixelMappingDesignerToolbar()
	{
		using namespace DMXPixelMappingDesignerToolbar::Private;
		if (UToolMenus::Get()->IsMenuRegistered(ToolbarName))
		{
			UToolMenus::Get()->RemoveMenu(ToolbarName);
		}
	}

	void SDMXPixelMappingDesignerToolbar::Construct(const FArguments& InArgs, const TSharedRef<FDMXPixelMappingToolkit>& InToolkit)
	{
		WeakToolkit = InToolkit;

		RegisterToolbarMenu(InArgs);
		
		const FToolMenuContext Context(InToolkit->GetToolkitCommands());

		using namespace DMXPixelMappingDesignerToolbar::Private;
		ChildSlot
		[
			UToolMenus::Get()->GenerateWidget(ToolbarName, Context)
		];
	}

	void SDMXPixelMappingDesignerToolbar::RegisterToolbarMenu(const FArguments& InArgs)
	{
		using namespace UE::DMX;

		UToolMenus* ToolMenus = UToolMenus::Get();

		using namespace DMXPixelMappingDesignerToolbar::Private;
		if (ToolMenus->IsMenuRegistered(ToolbarName))
		{
			return;
		}

		UToolMenu* Toolbar = UToolMenus::Get()->RegisterMenu(ToolbarName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);
		Toolbar->StyleName = "EditorViewportToolbar";
		

		// Transform Handle Modes
		{
			FToolMenuSection& Section = Toolbar->AddSection("TransformHandleMode");

			// Resize mode
			const FCheckBoxStyle& CheckBoxStartStyle = FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("EditorViewportToolBar.ToggleButton.Start");

			TSharedRef<SCheckBox> EnableResizeModeToggleButton = 
				SNew(SCheckBox)
				.Style(&CheckBoxStartStyle)
				.ToolTipText(LOCTEXT("TransformHandleResizeMode", "Resize Components"))
				.OnCheckStateChanged(this, &SDMXPixelMappingDesignerToolbar::OnTransformHandleModeSelected, EDMXPixelMappingTransformHandleMode::Resize)
				.IsChecked(this, &SDMXPixelMappingDesignerToolbar::GetCheckboxStateForTransormHandleMode, EDMXPixelMappingTransformHandleMode::Resize)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("EditorViewport.ScaleMode"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				];

			// Rotate mode
			const FCheckBoxStyle& CheckBoxEndStyle = FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("EditorViewportToolBar.ToggleButton.End");

			TSharedRef<SCheckBox> EnableRotateModeToggleButton = 
				SNew(SCheckBox)
				.Style(&CheckBoxEndStyle)
				.ToolTipText(LOCTEXT("TransformHandleRotateMode", "Rotate Components"))
				.OnCheckStateChanged(this, &SDMXPixelMappingDesignerToolbar::OnTransformHandleModeSelected, EDMXPixelMappingTransformHandleMode::Rotate)
				.IsChecked(this, &SDMXPixelMappingDesignerToolbar::GetCheckboxStateForTransormHandleMode, EDMXPixelMappingTransformHandleMode::Rotate)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("EditorViewport.RotateMode"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				];

			// As a single widget
			const TSharedRef<SWidget> TransformHandleModeWidget =
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(0.f)
				[
					EnableResizeModeToggleButton
				]
				+ SHorizontalBox::Slot()
				.Padding(0.f)
				[
					EnableRotateModeToggleButton
				];

			Section.AddEntry
			(
				FToolMenuEntry::InitWidget
				(
					"TransformHandleModes",
					TransformHandleModeWidget,
					FText::GetEmpty()
				)
			);
		}


		// Grid Snapping
		{
			FToolMenuSection& Section = Toolbar->AddSection("GridSnapping");

			const FUICommandInfo& ToggleSnapGridCommand = *FDMXPixelMappingEditorCommands::Get().ToggleGridSnapping;

			const TSharedRef<SViewportToolBarComboMenu> GridSnappingComboMenu =
				SNew(SViewportToolBarComboMenu)
				.Cursor(EMouseCursor::Default)
				.IsChecked(this, &SDMXPixelMappingDesignerToolbar::GetSnapGridEnabledCheckState)
				.OnCheckStateChanged(this, &SDMXPixelMappingDesignerToolbar::OnSnapGridCheckStateChanged)
				.Label(this, &SDMXPixelMappingDesignerToolbar::GetSnapGridLabel)
				.OnGetMenuContent(this, &SDMXPixelMappingDesignerToolbar::GenerateSnapGridMenu)
				.ToggleButtonToolTip(ToggleSnapGridCommand.GetDescription())
				.MenuButtonToolTip(LOCTEXT("SnapGridMenuTooltip", "Grid Snapping Settings"))
				.Icon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.LocationGridSnap"))
				.ParentToolBar(SharedThis(this));

			Section.AddEntry
			(
				FToolMenuEntry::InitWidget
				(
					"GridSnappingComboButton",
					GridSnappingComboMenu,
					FText::GetEmpty()
				)
			);
		}


		// Zoom to Fit
		{
			FToolMenuSection& Section = Toolbar->AddSection("ZoomToFit");

			constexpr TCHAR ToolBarStyleName[] = TEXT("EditorViewportToolBar");
			const FToolBarStyle& ToolBarStyle = FAppStyle::Get().GetWidgetStyle<FToolBarStyle>(ToolBarStyleName);

			const TSharedRef<SButton> ZoomToFitButton = 
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
				];

			Section.AddEntry
			(
				FToolMenuEntry::InitWidget
				(
					"ZoomToFitButton",
					ZoomToFitButton,
					FText::GetEmpty()
				)
			);
		}
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

	void SDMXPixelMappingDesignerToolbar::OnTransformHandleModeSelected(ECheckBoxState DummyCheckBoxState, UE::DMX::EDMXPixelMappingTransformHandleMode NewTransformHandleMode)
	{
		const TSharedPtr<FDMXPixelMappingToolkit> Toolkit = WeakToolkit.Pin();
		if (!Toolkit.IsValid())
		{
			return;
		}

		Toolkit->SetTransformHandleMode(NewTransformHandleMode);
	}

	ECheckBoxState SDMXPixelMappingDesignerToolbar::GetCheckboxStateForTransormHandleMode(UE::DMX::EDMXPixelMappingTransformHandleMode TransformHandleMode) const
	{
		const TSharedPtr<FDMXPixelMappingToolkit> Toolkit = WeakToolkit.Pin();
		if (!Toolkit.IsValid())
		{
			return ECheckBoxState::Undetermined;
		}

		return Toolkit->GetTransformHandleMode() == TransformHandleMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
}

#undef LOCTEXT_NAMESPACE
