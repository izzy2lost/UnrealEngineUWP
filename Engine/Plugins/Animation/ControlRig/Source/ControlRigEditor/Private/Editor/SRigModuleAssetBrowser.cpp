// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRigModuleAssetBrowser.h"

#include "ContentBrowserModule.h"
#include "ControlRigBlueprint.h"
#include "IContentBrowserSingleton.h"

#include "ControlRigEditor.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "RigModuleAssetBrowser"

void SRigModuleAssetBrowser::Construct(
	const FArguments& InArgs,
	TSharedRef<FControlRigEditor> InEditor)
{
	ControlRigEditor = InEditor;

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			SAssignNew(AssetBrowserBox, SBox)
		]
	];

	RefreshView();
}

void SRigModuleAssetBrowser::RefreshView()
{
	FAssetPickerConfig AssetPickerConfig;
	
	// setup filtering
	AssetPickerConfig.Filter.ClassPaths.Add(UControlRigBlueprint::StaticClass()->GetClassPathName());
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.bAddFilterUI = true;
	AssetPickerConfig.bShowPathInColumnView = true;
	AssetPickerConfig.bShowTypeInColumnView = true;
	AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SRigModuleAssetBrowser::OnShouldFilterAsset);
	AssetPickerConfig.DefaultFilterMenuExpansion = EAssetTypeCategories::Blueprint;
	AssetPickerConfig.OnGetAssetContextMenu = FOnGetAssetContextMenu::CreateSP(this, &SRigModuleAssetBrowser::OnGetAssetContextMenu);
	AssetPickerConfig.GetCurrentSelectionDelegates.Add(&GetCurrentSelectionDelegate);
	AssetPickerConfig.bAllowNullSelection = false;
	AssetPickerConfig.bFocusSearchBoxWhenOpened = false;

	// hide all asset registry columns by default (we only really want the name and path)
	TArray<UObject::FAssetRegistryTag> AssetRegistryTags;
	UControlRigBlueprint::StaticClass()->GetDefaultObject()->GetAssetRegistryTags(AssetRegistryTags);
	for(UObject::FAssetRegistryTag& AssetRegistryTag : AssetRegistryTags)
	{
		AssetPickerConfig.HiddenColumnNames.Add(AssetRegistryTag.Name.ToString());
	}

	// Also hide the type column by default (but allow users to enable it, so don't use bShowTypeInColumnView)
	AssetPickerConfig.HiddenColumnNames.Add(TEXT("Class"));
	AssetPickerConfig.HiddenColumnNames.Add(TEXT("Has Virtualized Data"));

	const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	AssetBrowserBox->SetContent(ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig));
}

TSharedPtr<SWidget> SRigModuleAssetBrowser::OnGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets) const
{
	if (SelectedAssets.Num() <= 0)
	{
		return nullptr;
	}

	UObject* SelectedAsset = SelectedAssets[0].GetAsset();
	if (SelectedAsset == nullptr)
	{
		return nullptr;
	}
	
	FMenuBuilder MenuBuilder(true, MakeShared<FUICommandList>());

	MenuBuilder.BeginSection(TEXT("Asset"), LOCTEXT("AssetSectionLabel", "Asset"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Browse", "Browse to Asset"),
			LOCTEXT("BrowseTooltip", "Browses to the associated asset and selects it in the most recently used Content Browser (summoning one if necessary)"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "SystemWideCommands.FindInContentBrowser.Small"),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedAsset] ()
				{
					if (SelectedAsset)
					{
						const TArray<FAssetData>& Assets = { SelectedAsset };
						const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
						ContentBrowserModule.Get().SyncBrowserToAssets(Assets);
					}
				}),
				FCanExecuteAction::CreateLambda([] () { return true; })
			)
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

bool SRigModuleAssetBrowser::OnShouldFilterAsset(const struct FAssetData& AssetData)
{
	// is this an control rig blueprint asset?
	if (!AssetData.IsInstanceOf(UControlRigBlueprint::StaticClass()))
	{
		return true;
	}
	
	static const UEnum* ControlTypeEnum = StaticEnum<EControlRigType>();
	const FString ControlRigTypeStr = AssetData.GetTagValueRef<FString>(TEXT("ControlRigType"));
	if (ControlRigTypeStr.IsEmpty())
	{
		return true;
	}

	const EControlRigType ControlRigType = (EControlRigType)(ControlTypeEnum->GetValueByName(*ControlRigTypeStr));
	return ControlRigType != EControlRigType::RigModule;
}

#undef LOCTEXT_NAMESPACE
