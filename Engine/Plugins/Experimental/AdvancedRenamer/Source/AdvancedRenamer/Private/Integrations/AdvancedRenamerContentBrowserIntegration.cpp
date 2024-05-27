// Copyright Epic Games, Inc. All Rights Reserved.

#include "Integrations/AdvancedRenamerContentBrowserIntegration.h"
#include "ContentBrowserDelegates.h"
#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "Delegates/IDelegateInstance.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "IAdvancedRenamerModule.h"
#include "Providers/AdvancedRenamerAssetProvider.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"


#define LOCTEXT_NAMESPACE "AdvancedRenamerContentBrowserIntegration"

namespace UE::AdvancedRenamer::Private
{
	FDelegateHandle ContentBrowserDelegateHandle;

	void OpenAdvancedRenamer(const TArray<FAssetData> AssetArray)
	{
		TSharedRef<FAdvancedRenamerAssetProvider> AssetProvider = MakeShared<FAdvancedRenamerAssetProvider>();
		AssetProvider->SetAssetList(AssetArray);

		TSharedPtr<SWidget> HostWidget = nullptr;

		IAdvancedRenamerModule::Get().OpenAdvancedRenamer(StaticCastSharedRef<IAdvancedRenamerProvider>(AssetProvider), HostWidget);
	}

	void AddMenuEntry(FToolMenuSection& MenuSection, const TArray<FAssetData> SelectedAssets)
	{
		MenuSection.AddMenuEntry(
			"BatchRename",
			LOCTEXT("AdvancedRename", "Batch Rename"),
			LOCTEXT("AdvancedRenameTooltip", "Opens the Batch Renamer Panel to rename all selected assets."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.Rename"),
			FUIAction(FExecuteAction::CreateStatic(&OpenAdvancedRenamer, SelectedAssets)));
	}

	void ExtendAssetContextMenu()
	{
		if (UToolMenus* ToolMenus = UToolMenus::Get())
		{
			FToolMenuOwnerScoped OwnerScoped(TEXT("AdvancedRenamer"));

			if (UToolMenu* Menu = ToolMenus->ExtendMenu("ContentBrowser.AssetContextMenu"))
			{
				if (FToolMenuSection* MenuSection = Menu->FindSection("CommonAssetActions"))
				{
					MenuSection->AddDynamicEntry("CreateVariant", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
					{
						UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
						if (Context)
						{
							const TArray<FAssetData>& SelectedAssets = Context->SelectedAssets;
							if (SelectedAssets.Num() > 0 )
							{
								AddMenuEntry(InSection, SelectedAssets);
							}
						}
					}));
				}
			}
		}
	}
}

void FAdvancedRenamerContentBrowserIntegration::Initialize()
{
	using namespace UE::AdvancedRenamer::Private;

	// Register Content Browser selection extensions
	ExtendAssetContextMenu();
}

void FAdvancedRenamerContentBrowserIntegration::Shutdown()
{
	if (UToolMenus* ToolMenus = UToolMenus::TryGet())
	{
		ToolMenus->UnregisterOwner(TEXT("AdvancedRenamer"));
	}
}

#undef LOCTEXT_NAMESPACE
