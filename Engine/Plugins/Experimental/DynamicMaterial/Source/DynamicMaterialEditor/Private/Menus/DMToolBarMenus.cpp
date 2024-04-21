// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMToolBarMenus.h"
#include "ContentBrowserModule.h"
#include "DesktopPlatformModule.h"
#include "DMBlueprintFunctionLibrary.h"
#include "DMPrivate.h"
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
#include "Helpers/DMMaterialSnapshotLibrary.h"
#include "Widgets/SWidget.h"

#define LOCTEXT_NAMESPACE "FDMToolBarMenus"

namespace UE::DynamicMaterialEditor::Private
{
	static FName ToolBarEditorLayoutMenuName = TEXT("MaterialDesigner.EditorLayout");
	static FName ToolBarMaterialInstanceSectionName = TEXT("MaterialInstance");
	static FName ToolBarMaterialExportSectionName = TEXT("MaterialExport");
	static FName ToolBarMaterialDesignerSettingsSectionName = TEXT("MaterialDesignerSettings");

	void AddToolBarMaterialInstanceChannelListMenu_Execute(const FToolMenuContext& InContext, FName InPresetName)
	{
		if (UDMMenuContext* Context = InContext.FindContext<UDMMenuContext>())
		{
			if (UDynamicMaterialModel* MaterialModel = Context->GetModel())
			{
				if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
				{
					TSharedPtr<SDMEditor> EditorWidget;

					if (TSharedPtr<SDMSlot> SlotWidget = Context->GetSlotWidget().Pin())
					{
						EditorWidget = SlotWidget->GetEditorWidget();

					}

					EditorOnlyData->SetChannelListPreset(InPresetName);

					if (EditorWidget.IsValid())
					{
						EditorWidget->RefreshSlotPickerList();
						EditorWidget->RefreshSlotWidget();
					}
				}
			}
		}
	}

	ECheckBoxState AddToolBarMaterialInstanceChannelListMenu_GetCheckState(const FToolMenuContext& InContext, FName InPresetName)
	{
		if (UDMMenuContext* Context = InContext.FindContext<UDMMenuContext>())
		{
			if (UDynamicMaterialModel* MaterialModel = Context->GetModel())
			{
				if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
				{
					return InPresetName == EditorOnlyData->GetChannelListPreset()
						? ECheckBoxState::Checked
						: ECheckBoxState::Unchecked;
				}
			}
		}

		return ECheckBoxState::Undetermined;
	}

	void AddToolBarMaterialInstanceChannelListMenu(UToolMenu* InMenu)
	{
		FToolMenuSection& NewSection = InMenu->AddSection("MaterialType", LOCTEXT("MaterialType", "Material Type"));

		for (const TPair<FName, FDMMaterialChannelListPreset>& Preset : GetDefault<UDynamicMaterialEditorSettings>()->ChannelPresets)
		{
			FToolUIAction MaterialChannelListType;
			MaterialChannelListType.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&AddToolBarMaterialInstanceChannelListMenu_Execute, Preset.Key);
			MaterialChannelListType.GetActionCheckState = FToolMenuGetActionCheckState::CreateStatic(&AddToolBarMaterialInstanceChannelListMenu_GetCheckState, Preset.Key);

			NewSection.AddMenuEntry(
				Preset.Key,
				FText::FromName(Preset.Key),
				FText::GetEmpty(),
				TAttribute<FSlateIcon>(),
				FToolUIActionChoice(MaterialChannelListType),
				EUserInterfaceActionType::RadioButton
			);
		}
	}

	void AddToolBarMaterialInstanceDomainMenu_Execute(const FToolMenuContext& InContext, EMaterialDomain InDomain)
	{
		if (UDMMenuContext* Context = InContext.FindContext<UDMMenuContext>())
		{
			if (UDynamicMaterialModel* MaterialModel = Context->GetModel())
			{
				if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
				{
					TSharedPtr<SDMEditor> EditorWidget;

					if (TSharedPtr<SDMSlot> SlotWidget = Context->GetSlotWidget().Pin())
					{
						EditorWidget = SlotWidget->GetEditorWidget();

					}

					EditorOnlyData->SetDomain(InDomain);

					if (EditorWidget.IsValid())
					{
						EditorWidget->RefreshSlotPickerList();
						EditorWidget->RefreshSlotWidget();
					}
				}
			}
		}
	}

	bool AddToolBarMaterialInstanceDomainMenu_CanExecute(const FToolMenuContext& InContext, EMaterialDomain InDomain)
	{
		if (UDMMenuContext* Context = InContext.FindContext<UDMMenuContext>())
		{
			if (UDynamicMaterialModel* MaterialModel = Context->GetModel())
			{
				return !!UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);
			}
		}

		return false;
	}

	ECheckBoxState AddToolBarMaterialInstanceDomainMenu_GetCheckState(const FToolMenuContext& InContext, EMaterialDomain InDomain)
	{
		if (UDMMenuContext* Context = InContext.FindContext<UDMMenuContext>())
		{
			if (UDynamicMaterialModel* MaterialModel = Context->GetModel())
			{
				if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
				{
					return EditorOnlyData->GetDomain() == InDomain
						? ECheckBoxState::Checked
						: ECheckBoxState::Unchecked;
				}
			}
		}

		return ECheckBoxState::Undetermined;
	}

	void AddToolBarMaterialInstanceDomainMenu(UToolMenu* InMenu)
	{
		UEnum* DomainEnum = StaticEnum<EMaterialDomain>();

		FToolMenuSection& NewSection = InMenu->AddSection("MaterialDomains", LOCTEXT("MaterialDomains", "Material Domains"));

		for (EMaterialDomain Domain : UDynamicMaterialModelEditorOnlyData::SupportedDomains)
		{
			FToolUIAction DomainAction;
			DomainAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&AddToolBarMaterialInstanceDomainMenu_Execute, Domain);
			DomainAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&AddToolBarMaterialInstanceDomainMenu_CanExecute, Domain);
			DomainAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateStatic(&AddToolBarMaterialInstanceDomainMenu_GetCheckState, Domain);

			NewSection.AddMenuEntry(
				DomainEnum->GetNameByValue(Domain),
				DomainEnum->GetDisplayNameTextByValue(Domain),
				FText::GetEmpty(),
				TAttribute<FSlateIcon>(),
				FToolUIActionChoice(DomainAction),
				EUserInterfaceActionType::RadioButton
			);
		}
	}

	void AddToolBarMaterialInstanceBlendModeMenu_Execute(const FToolMenuContext& InContext, EBlendMode InBlendMode)
	{
		if (UDMMenuContext* Context = InContext.FindContext<UDMMenuContext>())
		{
			if (UDynamicMaterialModel* MaterialModel = Context->GetModel())
			{
				if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
				{
					TSharedPtr<SDMEditor> EditorWidget;

					if (TSharedPtr<SDMSlot> SlotWidget = Context->GetSlotWidget().Pin())
					{
						EditorWidget = SlotWidget->GetEditorWidget();

					}

					EditorOnlyData->SetBlendMode(InBlendMode);

					if (EditorWidget.IsValid())
					{
						EditorWidget->RefreshSlotPickerList();
						EditorWidget->RefreshSlotWidget();
					}
				}
			}
		}
	}

	bool AddToolBarMaterialInstanceBlendModeMenu_CanExecute(const FToolMenuContext& InContext, EBlendMode InBlendMode)
	{
		if (UDMMenuContext* Context = InContext.FindContext<UDMMenuContext>())
		{
			if (UDynamicMaterialModel* MaterialModel = Context->GetModel())
			{
				return !!UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);
			}
		}

		return false;
	}

	ECheckBoxState AddToolBarMaterialInstanceBlendModeMenu_GetCheckState(const FToolMenuContext& InContext, EBlendMode InBlendMode)
	{
		if (UDMMenuContext* Context = InContext.FindContext<UDMMenuContext>())
		{
			if (UDynamicMaterialModel* MaterialModel = Context->GetModel())
			{
				if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
				{
					return EditorOnlyData->GetBlendMode() == InBlendMode
						? ECheckBoxState::Checked
						: ECheckBoxState::Unchecked;
				}
			}
		}

		return ECheckBoxState::Undetermined;
	}

	void AddToolBarMaterialInstanceBlendModeMenu(UToolMenu* InMenu)
	{
		UEnum* BlendModeEnum = StaticEnum<EBlendMode>();

		FToolMenuSection& NewSection = InMenu->AddSection("MaterialBlendModes", LOCTEXT("MaterialBlendModes", "Material Blend Modes"));

		for (EBlendMode BlendMode : UDynamicMaterialModelEditorOnlyData::SupportedBlendModes)
		{
			FToolUIAction BlendModeAction;
			BlendModeAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&AddToolBarMaterialInstanceBlendModeMenu_Execute, BlendMode);
			BlendModeAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&AddToolBarMaterialInstanceBlendModeMenu_CanExecute, BlendMode);
			BlendModeAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateStatic(&AddToolBarMaterialInstanceBlendModeMenu_GetCheckState, BlendMode);

			NewSection.AddMenuEntry(
				BlendModeEnum->GetNameByValue(BlendMode),
				BlendModeEnum->GetDisplayNameTextByValue(BlendMode),
				FText::GetEmpty(),
				TAttribute<FSlateIcon>(),
				FToolUIActionChoice(BlendModeAction),
				EUserInterfaceActionType::RadioButton
			);
		}
	}

	void AddToolBarMaterialInstanceUnlitMenu_Execute(const FToolMenuContext& InContext)
	{
		if (UDMMenuContext* Context = InContext.FindContext<UDMMenuContext>())
		{
			if (UDynamicMaterialModel* MaterialModel = Context->GetModel())
			{
				if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
				{
					TSharedPtr<SDMEditor> EditorWidget;

					if (TSharedPtr<SDMSlot> SlotWidget = Context->GetSlotWidget().Pin())
					{
						EditorWidget = SlotWidget->GetEditorWidget();

					}

					switch (EditorOnlyData->GetShadingModel())
					{
						case EDMMaterialShadingModel::DefaultLit:
							EditorOnlyData->SetShadingModel(EDMMaterialShadingModel::Unlit);
							break;

						case EDMMaterialShadingModel::Unlit:
							EditorOnlyData->SetShadingModel(EDMMaterialShadingModel::DefaultLit);
							break;
					}

					if (EditorWidget.IsValid())
					{
						EditorWidget->RefreshSlotPickerList();
					}
				}
			}
		}
	}

	bool AddToolBarMaterialInstanceUnlitMenu_CanExecute(const FToolMenuContext& InContext)
	{
		if (UDMMenuContext* Context = InContext.FindContext<UDMMenuContext>())
		{
			if (UDynamicMaterialModel* MaterialModel = Context->GetModel())
			{
				return !!UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);
			}
		}

		return false;
	}

	ECheckBoxState AddToolBarMaterialInstanceUnlitMenu_GetCheckState(const FToolMenuContext& InContext)
	{
		if (UDMMenuContext* Context = InContext.FindContext<UDMMenuContext>())
		{
			if (UDynamicMaterialModel* MaterialModel = Context->GetModel())
			{
				if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
				{
					switch (EditorOnlyData->GetShadingModel())
					{
						case EDMMaterialShadingModel::DefaultLit:
							return ECheckBoxState::Unchecked;

						case EDMMaterialShadingModel::Unlit:
							return ECheckBoxState::Checked;
					}
				}
			}
		}

		return ECheckBoxState::Undetermined;
	}

	void AddToolBarMaterialInstanceAnimatedMenu_Execute(const FToolMenuContext& InContext)
	{
		if (UDMMenuContext* Context = InContext.FindContext<UDMMenuContext>())
		{
			if (UDynamicMaterialModel* MaterialModel = Context->GetModel())
			{
				if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
				{
					TSharedPtr<SDMEditor> EditorWidget;

					if (TSharedPtr<SDMSlot> SlotWidget = Context->GetSlotWidget().Pin())
					{
						EditorWidget = SlotWidget->GetEditorWidget();

					}

					EditorOnlyData->SetPixelAnimationFlag(!EditorOnlyData->IsPixelAnimationFlagSet());

					if (EditorWidget.IsValid())
					{
						EditorWidget->RefreshSlotPickerList();
					}					
				}
			}
		}
	}

	bool AddToolBarMaterialInstanceAnimatedMenu_CanExecute(const FToolMenuContext& InContext)
	{
		if (UDMMenuContext* Context = InContext.FindContext<UDMMenuContext>())
		{
			if (UDynamicMaterialModel* MaterialModel = Context->GetModel())
			{
				return !!UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);
			}
		}

		return false;
	}

	ECheckBoxState AddToolBarMaterialInstanceAnimatedMenu_GetCheckState(const FToolMenuContext& InContext)
	{
		if (UDMMenuContext* Context = InContext.FindContext<UDMMenuContext>())
		{
			if (UDynamicMaterialModel* MaterialModel = Context->GetModel())
			{
				if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
				{
					return EditorOnlyData->IsPixelAnimationFlagSet()
						? ECheckBoxState::Checked
						: ECheckBoxState::Unchecked;
				}
			}
		}

		return ECheckBoxState::Undetermined;
	}

	void AddToolBarMaterialInstanceTwoSidedMenu_Execute(const FToolMenuContext& InContext)
	{
		if (UDMMenuContext* Context = InContext.FindContext<UDMMenuContext>())
		{
			if (UDynamicMaterialModel* MaterialModel = Context->GetModel())
			{
				if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
				{
					TSharedPtr<SDMEditor> EditorWidget;

					if (TSharedPtr<SDMSlot> SlotWidget = Context->GetSlotWidget().Pin())
					{
						EditorWidget = SlotWidget->GetEditorWidget();

					}

					EditorOnlyData->SetTwoSidedFlag(!EditorOnlyData->IsTwoSidedFlagSet());

					if (EditorWidget.IsValid())
					{
						EditorWidget->RefreshSlotPickerList();
					}					
				}
			}
		}
	}

	bool AddToolBarMaterialInstanceTwoSidedMenu_CanExecute(const FToolMenuContext& InContext)
	{
		if (UDMMenuContext* Context = InContext.FindContext<UDMMenuContext>())
		{
			if (UDynamicMaterialModel* MaterialModel = Context->GetModel())
			{
				return !!UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);
			}
		}

		return false;
	}

	ECheckBoxState AddToolBarMaterialInstanceTwoSidedMenu_GetCheckState(const FToolMenuContext& InContext)
	{
		if (UDMMenuContext* Context = InContext.FindContext<UDMMenuContext>())
		{
			if (UDynamicMaterialModel* MaterialModel = Context->GetModel())
			{
				if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
				{
					return EditorOnlyData->IsTwoSidedFlagSet()
						? ECheckBoxState::Checked
						: ECheckBoxState::Unchecked;
				}
			}
		}

		return ECheckBoxState::Undetermined;
	}

	void AddToolBarInstanceMenu(UToolMenu* InMenu)
	{
		if (!IsValid(InMenu) || InMenu->ContainsSection(ToolBarMaterialInstanceSectionName))
		{
			return;
		}

		FToolMenuSection& NewSection = InMenu->AddSection(ToolBarMaterialInstanceSectionName, LOCTEXT("MaterialInstanceSection", "Material Instance"));

		NewSection.AddSubMenu(
			"ChannelList",
			LOCTEXT("MaterialInstanceMaterialType", "Material Type"),
			LOCTEXT("MaterialInstanceMaterialType_ToolTip", "Set the material type (channel list) for the active material designer instance."),
			FNewToolMenuDelegate::CreateStatic(&AddToolBarMaterialInstanceChannelListMenu)
		);

		NewSection.AddSubMenu(
			"Domain",
			LOCTEXT("MaterialInstanceDomain", "Material Domain"),
			LOCTEXT("MaterialInstanceDomain_ToolTip", "Set the material domain for the active material designer instance."),
			FNewToolMenuDelegate::CreateStatic(&AddToolBarMaterialInstanceDomainMenu)
		);

		NewSection.AddSubMenu(
			"BlendMode",
			LOCTEXT("MaterialInstanceBlendMode", "Material Blend Mode"),
			LOCTEXT("MaterialInstanceBlendMode_ToolTip", "Set the material blend mode for the active material designer instance."),
			FNewToolMenuDelegate::CreateStatic(&AddToolBarMaterialInstanceBlendModeMenu)
		);

		FToolUIAction UnlitAction;
		UnlitAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&AddToolBarMaterialInstanceUnlitMenu_Execute);
		UnlitAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&AddToolBarMaterialInstanceUnlitMenu_CanExecute);
		UnlitAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateStatic(&AddToolBarMaterialInstanceUnlitMenu_GetCheckState);

		NewSection.AddMenuEntry(
			"Unlit",
			LOCTEXT("Unlit", "Unlit"),
			LOCTEXT("UnlitTooltip", "Whether this material requires light to be seen."),
			TAttribute<FSlateIcon>(),
			FToolUIActionChoice(UnlitAction),
			EUserInterfaceActionType::ToggleButton
		);

		FToolUIAction AnimatedAction;
		AnimatedAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&AddToolBarMaterialInstanceAnimatedMenu_Execute);
		AnimatedAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&AddToolBarMaterialInstanceAnimatedMenu_CanExecute);
		AnimatedAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateStatic(&AddToolBarMaterialInstanceAnimatedMenu_GetCheckState);

		NewSection.AddMenuEntry(
			"Animated",
			LOCTEXT("Animated", "Animated"),
			LOCTEXT("AnimatedTooltip", "Enables motion vectors in supported materials."),
			TAttribute<FSlateIcon>(),
			FToolUIActionChoice(AnimatedAction),
			EUserInterfaceActionType::ToggleButton
		);

		FToolUIAction TwoSidedAction;
		TwoSidedAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&AddToolBarMaterialInstanceTwoSidedMenu_Execute);
		TwoSidedAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&AddToolBarMaterialInstanceTwoSidedMenu_CanExecute);
		TwoSidedAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateStatic(&AddToolBarMaterialInstanceTwoSidedMenu_GetCheckState);

		NewSection.AddMenuEntry(
			"TwoSided",
			LOCTEXT("TwoSided", "Two Sided"),
			LOCTEXT("TwoSidedTooltip", "Whether this material will render on both the back and front of geometry."),
			TAttribute<FSlateIcon>(),
			FToolUIActionChoice(TwoSidedAction),
			EUserInterfaceActionType::ToggleButton
		);
	}

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
			FSaveAssetDialogConfig SaveAssetDialogConfig;
			SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
			SaveAssetDialogConfig.DefaultPath = "/Game";
			SaveAssetDialogConfig.DefaultAssetName = MaterialInstance->GetName();
			SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

			if (!SaveObjectPath.IsEmpty())
			{
				UDMBlueprintFunctionLibrary::ExportMaterialInstance(MaterialInstance->GetMaterialModel(), SaveObjectPath);

				if (FEngineAnalytics::IsAvailable())
				{
					FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MaterialDesigner.ExportedMaterialInstance"));
				}
			}
		}
	}

	void ExportMaterialModelFromModel(TWeakObjectPtr<UDynamicMaterialModel> InMaterialModelWeak)
	{
		if (UDynamicMaterialModel* MaterialModel = InMaterialModelWeak.Get())
		{
			UMaterial* GeneratedMaterial = MaterialModel->GetGeneratedMaterial();

			if (!GeneratedMaterial)
			{
				UE::DynamicMaterialEditor::Private::LogError(TEXT("Failed to find a generated material to export."), true, MaterialModel);
				return;
			}

			FSaveAssetDialogConfig SaveAssetDialogConfig;
			SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
			SaveAssetDialogConfig.DefaultPath = "/Game";
			SaveAssetDialogConfig.DefaultAssetName = GeneratedMaterial->GetName();
			SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			const FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

			if (SaveObjectPath.Len() == 0)
			{
				return;
			}

			UDMBlueprintFunctionLibrary::ExportGeneratedMaterial(MaterialModel, SaveObjectPath);

			if (FEngineAnalytics::IsAvailable())
			{
				FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MaterialDesigner.ExportedGeneratedMaterial"));
			}
		}
	}

	void SnapshotMaterial(TWeakObjectPtr<UDynamicMaterialModel> InMaterialModelWeak, FIntPoint InTextureSize)
	{
		UDynamicMaterialModel* MaterialModel = InMaterialModelWeak.Get();

		if (!IsValid(MaterialModel))
		{
			return;
		}

		UMaterialInterface* Material = MaterialModel->GetGeneratedMaterial();

		if (UDynamicMaterialInstance* MaterialInstance = MaterialModel->GetDynamicMaterialInstance())
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

		UDynamicMaterialModel* const MaterialModel = MenuContext->GetModel();

		if (!MaterialModel)
		{
			return;
		}

		TWeakObjectPtr<UDynamicMaterialModel> MaterialModelWeak = MaterialModel;
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

		UDynamicMaterialModel* const MaterialModel = MenuContext->GetModel();

		if (!MaterialModel)
		{
			return;
		}

		UDynamicMaterialInstance* Instance = MaterialModel->GetDynamicMaterialInstance();

		if (!Instance)
		{
			return;
		}

		const bool bAllowInstanceExport = IsValid(Instance->GetOuter()) && !Instance->GetOuter()->IsA<UPackage>();
		const bool bAllowMaterialExport = IsValid(MaterialModel->GetGeneratedMaterial());

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
					TWeakObjectPtr<UDynamicMaterialModel>(MaterialModel)
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
		AddToolBarInstanceMenu(InMenu);
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
		UDMMenuContext::CreateSlot(InEditorWidget->GetActiveSlotWidget())
	);

	return UToolMenus::Get()->GenerateWidget(ToolBarEditorLayoutMenuName, MenuContext);
}

#undef LOCTEXT_NAMESPACE
