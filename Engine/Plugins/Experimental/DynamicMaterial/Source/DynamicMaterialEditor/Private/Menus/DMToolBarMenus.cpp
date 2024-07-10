// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMToolBarMenus.h"

#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "DesktopPlatformModule.h"
#include "DynamicMaterialEditorModule.h"
#include "DynamicMaterialEditorSettings.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "EngineAnalytics.h"
#include "Framework/Application/SlateApplication.h"
#include "IContentBrowserSingleton.h"
#include "IDesktopPlatform.h"
#include "ISinglePropertyView.h"
#include "Material/DynamicMaterialInstance.h"
#include "Materials/Material.h"
#include "Menus/DMMenuContext.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "PropertyEditorModule.h"
#include "Slate/SDMEditor.h"
#include "Slate/SDMSlot.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Utils/DMBlueprintFunctionLibrary.h"
#include "Utils/DMMaterialSnapshotLibrary.h"
#include "Utils/DMPrivate.h"
#include "Widgets/SWidget.h"

#define LOCTEXT_NAMESPACE "FDMToolBarMenus"

namespace UE::DynamicMaterialEditor::Private
{
	static FName ToolBarEditorLayoutMenuName = TEXT("MaterialDesigner.EditorLayout");
	static FName ToolBarMaterialExportSectionName = TEXT("MaterialExport");
	static FName ToolBarMaterialDesignerSettingsSectionName = TEXT("MaterialDesignerSettings");

	void OpenMaterialEditorFromContext(UDMMenuContext* InMenuContext)
	{
		if (IsValid(InMenuContext))
		{
			if (UDynamicMaterialModelEditorOnlyData* const ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(InMenuContext->GetModel()))
			{
				if (FEngineAnalytics::IsAvailable())
				{
					FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MaterialDesigner.OpenedGeneratedMaterial"));
				}

				ModelEditorOnlyData->OpenMaterialEditor();
			}
		}
	}

	void ExportMaterialInstanceFromInstance(TWeakObjectPtr<UDynamicMaterialInstance> InMaterialInstanceWeak)
	{
		if (UDynamicMaterialInstance* MaterialInstance = InMaterialInstanceWeak.Get())
		{
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			FString PackageName, AssetName;
			AssetTools.CreateUniqueAssetName(MaterialInstance->GetName(), TEXT(""), PackageName, AssetName);

			IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
			const FContentBrowserItemPath CurrentPath = ContentBrowser.GetCurrentPath();
			const FString PathStr = CurrentPath.HasInternalPath() ? CurrentPath.GetInternalPathString() : "/Game";

			FSaveAssetDialogConfig SaveAssetDialogConfig;
			SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
			SaveAssetDialogConfig.DefaultPath = PathStr;
			SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;
			SaveAssetDialogConfig.DefaultAssetName = AssetName;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

			if (!SaveObjectPath.IsEmpty())
			{
				UDMBlueprintFunctionLibrary::ExportMaterialInstance(MaterialInstance->GetMaterialModelBase(), SaveObjectPath);

				if (FEngineAnalytics::IsAvailable())
				{
					FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MaterialDesigner.ExportedMaterialInstance"));
				}
			}
		}
	}

	void ExportMaterialModelFromModel(TWeakObjectPtr<UDynamicMaterialModelBase> InMaterialModelBaseWeak)
	{
		UDynamicMaterialModelBase* MaterialModelBase = InMaterialModelBaseWeak.Get();

		if (!MaterialModelBase)
		{
			return;
		}

		UMaterial* GeneratedMaterial = MaterialModelBase->GetGeneratedMaterial();

		if (!GeneratedMaterial)
		{
			UE::DynamicMaterialEditor::Private::LogError(TEXT("Failed to find a generated material to export."), true, MaterialModelBase);
			return;
		}

		IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
		const FContentBrowserItemPath CurrentPath = ContentBrowser.GetCurrentPath();
		const FString PathStr = CurrentPath.HasInternalPath() ? CurrentPath.GetInternalPathString() : "/Game";

		FSaveAssetDialogConfig SaveAssetDialogConfig;
		SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
		SaveAssetDialogConfig.DefaultPath = PathStr;
		SaveAssetDialogConfig.DefaultAssetName = GeneratedMaterial->GetName();
		SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		const FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

		if (SaveObjectPath.Len() == 0)
		{
			return;
		}

		UDMBlueprintFunctionLibrary::ExportGeneratedMaterial(MaterialModelBase, SaveObjectPath);

		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MaterialDesigner.ExportedGeneratedMaterial"));
		}
	}

	void SnapshotMaterial(TWeakObjectPtr<UDynamicMaterialModelBase> InMaterialModelBaseWeak, FIntPoint InTextureSize)
	{
		UDynamicMaterialModelBase* MaterialModelBase = InMaterialModelBaseWeak.Get();

		if (!IsValid(MaterialModelBase))
		{
			return;
		}

		UMaterialInterface* Material = MaterialModelBase->GetGeneratedMaterial();

		if (UDynamicMaterialInstance* MaterialInstance = MaterialModelBase->GetDynamicMaterialInstance())
		{
			if (!IsValid(MaterialInstance->Parent.Get()))
			{
				UE_LOG(LogDynamicMaterialEditor, Warning, TEXT("Unable to find world to find material instance parent."));
				return;
			}

			Material = MaterialInstance;
		}

		if (!Material)
		{
			UE_LOG(LogDynamicMaterialEditor, Warning, TEXT("Unable to find material to snapshot."));
			return;
		}

		TArray<FString> OutFilenames;

		if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
		{
			DesktopPlatform->SaveFileDialog(
				FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
				LOCTEXT("SaveSnapshotAs", "Save Snapshot As").ToString(),
				FPaths::ProjectSavedDir(),
				Material->GetName() + "_Snapshot_" + FString::FromInt(InTextureSize.X) + "x" + FString::FromInt(InTextureSize.Y),
				TEXT("HDR File (*.hdr)|*.hdr|EXR File (*.exr)|*.exr|PNG File (*.png)|*.png"),
				EFileDialogFlags::None,
				OutFilenames
			);
		}

		if (OutFilenames.Num() == 0)
		{
			return;
		}

		FDMMaterialShapshotLibrary::SnapshotMaterial(Material, InTextureSize, OutFilenames[0]);

		if (FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MaterialDesigner.SnapshotMaterial"));
		}
	}

	void AddToolBarBoolOptionMenuEntry(FToolMenuSection& InSection, const FName& InPropertyName, const FUIAction InAction)
	{
		const FProperty* const OptionProperty = UDynamicMaterialEditorSettings::StaticClass()->FindPropertyByName(InPropertyName);

		if (ensure(OptionProperty))
		{
			InSection.AddMenuEntry(NAME_None,
				OptionProperty->GetDisplayNameText(),
				OptionProperty->GetToolTipText(),
				FSlateIcon(),
				InAction, EUserInterfaceActionType::ToggleButton);
		}
	}

	void AddToolBarIntOptionMenuEntry(FToolMenuSection& InSection, const FName& InPropertyName, 
		TAttribute<bool> InIsEnabledAttribute = TAttribute<bool>(),
		TAttribute<EVisibility> InVisibilityAttribute = TAttribute<EVisibility>())
	{
		InSection.AddDynamicEntry(NAME_None,
			FNewToolMenuSectionDelegate::CreateLambda(
				[InPropertyName, InIsEnabledAttribute, InVisibilityAttribute](FToolMenuSection& InSection)
				{
					const FProperty* const OptionProperty = UDynamicMaterialEditorSettings::StaticClass()->FindPropertyByName(InPropertyName);
					FText DisplayName = FText::GetEmpty();
					FText Tooltip = FText::GetEmpty();

					if (ensure(OptionProperty))
					{
						DisplayName = OptionProperty->GetDisplayNameText();
						Tooltip = OptionProperty->GetToolTipText();
					}

					FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

					FSinglePropertyParams SinglePropertyParams;
					SinglePropertyParams.NamePlacement = EPropertyNamePlacement::Hidden;

					TSharedRef<ISinglePropertyView> SinglePropertyView = PropertyEditor.CreateSingleProperty(UDynamicMaterialEditorSettings::Get(), InPropertyName, SinglePropertyParams).ToSharedRef();
					SinglePropertyView->SetToolTipText(Tooltip);
					SinglePropertyView->SetEnabled(InIsEnabledAttribute);
					SinglePropertyView->SetVisibility(InVisibilityAttribute);

					InSection.AddEntry(FToolMenuEntry::InitWidget(NAME_None,
						SNew(SBox)
						.HAlign(HAlign_Fill)
						[
							SNew(SBox)
								.WidthOverride(80.0f)
								.HAlign(HAlign_Right)
								[
									SinglePropertyView
								]
						],
						DisplayName));
				})
		);
	}

	void CreateSnapshotMaterialMenu(UToolMenu* InMenu)
	{
		const UDMMenuContext* const MenuContext = InMenu->FindContext<UDMMenuContext>();

		if (!MenuContext)
		{
			return;
		}

		UDynamicMaterialModelBase* const MaterialModelBase = MenuContext->GetModelBase();

		if (!MaterialModelBase)
		{
			return;
		}

		TWeakObjectPtr<UDynamicMaterialModelBase> MaterialModelWeak = MaterialModelBase;
		FToolMenuSection& NewSection = InMenu->AddSection("SnapshotMaterial", LOCTEXT("SnapshotMaterial", "Snapshop Material"));

		NewSection.AddMenuEntry(
			NAME_None,
			LOCTEXT("SnapshotMaterial512", "512x512"),
			LOCTEXT("SnapshotMaterial512Tooltip", "Take a snapshot of the material with the current settings and export it as a texture with a resolution of 512x512 pixels."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(
				&UE::DynamicMaterialEditor::Private::SnapshotMaterial,
				MaterialModelWeak,
				FIntPoint(512, 512)
			))
		);

		NewSection.AddMenuEntry(
			NAME_None,
			LOCTEXT("SnapshotMaterial1024", "1024x1024"),
			LOCTEXT("SnapshotMaterial1024Tooltip", "Take a snapshot of the material with the current settings and export it as a texture with a resolution of 1024x1024 pixels."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(
				&UE::DynamicMaterialEditor::Private::SnapshotMaterial,
				MaterialModelWeak,
				FIntPoint(1024, 1024)
			))
		);

		NewSection.AddMenuEntry(
			NAME_None,
			LOCTEXT("SnapshotMaterial2048", "2048x2048"),
			LOCTEXT("SnapshotMaterial2048Tooltip", "Take a snapshot of the material with the current settings and export it as a texture with a resolution of 2048x2048 pixels."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(
				&UE::DynamicMaterialEditor::Private::SnapshotMaterial,
				MaterialModelWeak,
				FIntPoint(2048, 2048)
			))
		);

		NewSection.AddMenuEntry(
			NAME_None,
			LOCTEXT("SnapshotMaterial4096", "4096x4096"),
			LOCTEXT("SnapshotMaterial4096Tooltip", "Take a snapshot of the material with the current settings and export it as a texture with a resolution of 4096x4096 pixels."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(
				&UE::DynamicMaterialEditor::Private::SnapshotMaterial,
				MaterialModelWeak,
				FIntPoint(4096, 4096)
			))
		);
	}

	void AddToolBarExportMenu(UToolMenu* InMenu)
	{
		if (!IsValid(InMenu) || InMenu->ContainsSection(ToolBarMaterialExportSectionName))
		{
			return;
		}

		UDMMenuContext* const MenuContext = InMenu->FindContext<UDMMenuContext>();

		if (!MenuContext)
		{
			return;
		}

		UDynamicMaterialModelBase* const MaterialModelBase = MenuContext->GetModelBase();

		if (!MaterialModelBase)
		{
			return;
		}

		UDynamicMaterialInstance* Instance = MaterialModelBase->GetDynamicMaterialInstance();

		if (!Instance)
		{
			return;
		}

		const bool bAllowInstanceExport = IsValid(Instance->GetOuter()) && !Instance->GetOuter()->IsA<UPackage>();
		const bool bAllowMaterialExport = IsValid(MaterialModelBase->GetGeneratedMaterial());

		if (!bAllowInstanceExport && !bAllowMaterialExport)
		{
			return;
		}

		FToolMenuSection& NewSection = InMenu->AddSection(ToolBarMaterialExportSectionName, LOCTEXT("ExportSection", "Export"));

		NewSection.AddMenuEntry(NAME_None,
			LOCTEXT("OpenInUEMaterialEditor", "Open in Standard Material Editor"),
			LOCTEXT("OpenInUEMaterialEditorTooltip", "Opens the currently editing generated Material Designer Instance material in the standard material editor."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(
				&OpenMaterialEditorFromContext,
				MenuContext
			))
		);

		if (bAllowInstanceExport)
		{
			NewSection.AddMenuEntry(NAME_None,
				LOCTEXT("ExportMaterialInstance", "Export Material Designer Instance"),
				LOCTEXT("ExportMaterialInstanceTooltip", "Export the material instance to an asset."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(
					&UE::DynamicMaterialEditor::Private::ExportMaterialInstanceFromInstance,
					TWeakObjectPtr<UDynamicMaterialInstance>(Instance)
				))
			);
		}

		if (bAllowMaterialExport)
		{
			NewSection.AddMenuEntry(NAME_None,
				LOCTEXT("ExportGeneratedMaterial", "Export Generated Material"),
				LOCTEXT("ExportGeneratedMaterialTooltip", "Export the generated material to an asset."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(
					&UE::DynamicMaterialEditor::Private::ExportMaterialModelFromModel,
					TWeakObjectPtr<UDynamicMaterialModelBase>(MaterialModelBase)
				))
			);
		}

		NewSection.AddSubMenu(
			NAME_None,
			LOCTEXT("SnapshotMaterial", "Snapshop Material"),
			LOCTEXT("SnapshotMaterialTooltip", "Take a snapshot of the material with the current settings and export it as a texture."),
			FNewToolMenuChoice(FNewToolMenuDelegate::CreateStatic(
				&UE::DynamicMaterialEditor::Private::CreateSnapshotMaterialMenu
			))
		);
	}

	void AddToolBarTooltipOptionsSection(UToolMenu* InMenu)
	{
		if (!IsValid(InMenu))
		{
			return;
		}

		FToolMenuSection& NewSection = InMenu->AddSection("TooltipOptions", LOCTEXT("TooltipOptionsSection", "Tooltip Options"));

		AddToolBarBoolOptionMenuEntry(NewSection,
			GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, bShowTooltipPreview),
			FUIAction(
				FExecuteAction::CreateLambda(
					[]()
					{
						UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get();
						Settings->bShowTooltipPreview = !Settings->bShowTooltipPreview;
						Settings->SaveConfig();
					}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda(
					[]()
					{
						return UDynamicMaterialEditorSettings::Get()->bShowTooltipPreview;
					})
			)
		);

		TAttribute<bool> AreTooltipPreviewsEnabledAttribute = TAttribute<bool>::CreateLambda(
			[]()
			{
				return UDynamicMaterialEditorSettings::Get()->bShowTooltipPreview;
			});

		AddToolBarIntOptionMenuEntry(
			NewSection,
			GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, TooltipTextureSize),
			AreTooltipPreviewsEnabledAttribute
		);
	}

	void AddToolBarPreviewOptionsSection(UToolMenu* InMenu)
	{
		if (!IsValid(InMenu))
		{
			return;
		}

		FToolMenuSection& NewSection = InMenu->AddSection("PreviewOptions", LOCTEXT("PreviewOptionsSection", "Preview Options"));

		AddToolBarIntOptionMenuEntry(NewSection,
			GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, LayerPreviewSize)
		);

		AddToolBarIntOptionMenuEntry(NewSection,
			GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, DetailsPreviewSize)
		);
	}

	void AddToolBarAdvancedSection(UToolMenu* InMenu)
	{
		FToolMenuSection& NewSection = InMenu->AddSection("AdvancedSettings", LOCTEXT("AdvancedSettingsSection", "Advanced Settings"));

		InMenu->AddDynamicSection(NAME_None, FNewToolMenuDelegate::CreateStatic(&AddToolBarPreviewOptionsSection));

		InMenu->AddDynamicSection(NAME_None, FNewToolMenuDelegate::CreateStatic(&AddToolBarTooltipOptionsSection));

		NewSection.AddMenuEntry(NAME_None,
			LOCTEXT("ResetAllSettingsToDefaults", "Reset All To Defaults"),
			LOCTEXT("ResetAllSettingsToDefaultsTooltip", "Resets all the Material Designer settings to their default values."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateUObject(
				UDynamicMaterialEditorSettings::Get(), 
				&UDynamicMaterialEditorSettings::ResetAllLayoutSettings)
			)
		);
	}

	void AddToolBarSettingsMenu(UToolMenu* InMenu)
	{
		if (!IsValid(InMenu) || InMenu->ContainsSection(ToolBarMaterialDesignerSettingsSectionName))
		{
			return;
		}

		FToolMenuSection& NewSection = InMenu->AddSection(ToolBarMaterialDesignerSettingsSectionName, LOCTEXT("MaterialDesignerSection", "Material Designer"));

		NewSection.AddSubMenu(
			"AdvancedSettings",
			LOCTEXT("AdvancedSettingsSubMenu", "Advanced Settings"),
			LOCTEXT("AdvancedSettingsSubMenu_ToolTip", "Display advanced Material Designer settings"),
			FNewToolMenuDelegate::CreateStatic(&AddToolBarAdvancedSection)
		);

		NewSection.AddMenuEntry(NAME_None,
			LOCTEXT("OpenSettings", "Material Designer Editor Settings"),
			LOCTEXT("OpenSettingsTooltip", "Opens the Editor Settings and navigates to Material Designer section."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "FoliageEditMode.Settings"),
			FUIAction(FExecuteAction::CreateUObject(
				UDynamicMaterialEditorSettings::Get(),
				&UDynamicMaterialEditorSettings::OpenEditorSettingsWindow)
			)
		);
	}

	void AddToolBarEditorLayoutMenu(UToolMenu* InMenu)
	{
		AddToolBarExportMenu(InMenu);
		AddToolBarSettingsMenu(InMenu);
	}
}

TSharedRef<SWidget> FDMToolBarMenus::MakeEditorLayoutMenu(const TSharedPtr<SDMEditor>& InEditorWidget)
{
	using namespace UE::DynamicMaterialEditor::Private;

	if (!UToolMenus::Get()->IsMenuRegistered(ToolBarEditorLayoutMenuName))
	{
		UToolMenu* const NewToolMenu = UDMMenuContext::GenerateContextMenuDefault(ToolBarEditorLayoutMenuName);

		if (!NewToolMenu)
		{
			return SNullWidget::NullWidget;
		}

		FToolMenuSection& NewSection = NewToolMenu->AddDynamicSection(
			"MaterialDesignerSettings", 
			FNewToolMenuDelegate::CreateStatic(&AddToolBarEditorLayoutMenu)
		);
	}

	FToolMenuContext MenuContext(
		FDynamicMaterialEditorModule::Get().GetCommandList(),
		TSharedPtr<FExtender>(),
		UDMMenuContext::CreateEditor(InEditorWidget)
	);

	return UToolMenus::Get()->GenerateWidget(ToolBarEditorLayoutMenuName, MenuContext);
}

#undef LOCTEXT_NAMESPACE
