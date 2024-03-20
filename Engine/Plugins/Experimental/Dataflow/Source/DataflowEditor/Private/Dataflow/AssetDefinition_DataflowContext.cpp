// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/AssetDefinition_DataflowContext.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowEditorModule.h"
#include "Dataflow/DataflowEditorUtil.h"
#include "Dataflow/DataflowContent.h"
#include "Dialog/SMessageDialog.h"
#include "IContentBrowserSingleton.h"
#include "Math/Color.h"
#include "Misc/FileHelper.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "UObject/UObjectGlobals.h"


#define LOCTEXT_NAMESPACE "AssetActions_DataflowContext"

namespace DataflowContextDefinitionHelpers
{

	/*
	* BindContextToGraph
	* 
	* On load the Property in the cache will be a nullptr
	* This block will rebind the property in the cache to the 
	* the Property on the UDataflow asset. If there is ever an
	* mis-match, just return false to indicate that the binding
	* has failed.
	*/
	bool BindContextToGraph(TObjectPtr<UObject>& Asset, UDataflow* DataflowAsset)
	{
		using namespace Dataflow;

		TSharedPtr<FGraph> Dataflow = DataflowAsset->GetDataflow();
		if (!Dataflow) return false;

		UDataflowBaseContent* BaseContent = Cast< UDataflowBaseContent>(Asset.Get());
		if (!BaseContent) return false;

		TSharedPtr<FEngineContext>& Context = BaseContent->GetDataflowContext();
		if (!Context) return false;

		TSet<FContextCacheKey> Keys; Context->GetKeys(Keys);
		for (FContextCacheKey Key : Keys)
		{
			bool bValidKey = false;

			if (TUniquePtr<FContextCacheElementBase>* Data = Context->GetBaseData(Key))
			{
				if ((*Data) && !(*Data)->GetProperty())
				{
					if (TSharedPtr<FDataflowNode> Node = Dataflow->FindBaseNode((*Data)->GetNodeGuid()))
					{
						if (FDataflowOutput* Output = Node->FindOutput(Key))
						{
							if (const FProperty* Property = Output->GetProperty())
							{
								(*Data)->SetProperty(Property);
								bValidKey = true;
							}
						}
					}
				}
				else
				{
					bValidKey = true;
				}
			}

			if (!bValidKey)
			{
				return false;
			}
		}
		return true;
	}

	/*
	* ValidateCachedNodeHash
	* 
	* Check that the hashes stored in the cache reflect the hash of the nodes 
	* properties.
	*
	*/
	bool ValidateCachedNodeHash(TObjectPtr<UObject>& Asset, UDataflow* DataflowAsset)
	{
		using namespace Dataflow;

		TSharedPtr<FGraph> Dataflow = DataflowAsset->GetDataflow();
		if (!Dataflow) return false;

		UDataflowBaseContent* BaseContent = Cast< UDataflowBaseContent>(Asset.Get());
		if (!BaseContent) return false;

		TSharedPtr<FEngineContext>& Context = BaseContent->GetDataflowContext();
		if (!Context) return false;

		TSet<FContextCacheKey> Keys; Context->GetKeys(Keys);
		for (FContextCacheKey Key : Keys)
		{
			bool bValidKey = false;
			if (TUniquePtr<FContextCacheElementBase>* Data = Context->GetBaseData(Key))
			{
				if (!(*Data)) return false;
				if (!(*Data)->GetProperty()) return false;
				if(TSharedPtr<FDataflowNode> Node = Dataflow->FindBaseNode((*Data)->GetNodeGuid()))
				{
					if ((*Data)->GetNodeHash() != Node->GetValueHash())
					{
						return false;
					}
					bValidKey = true;
				}
			}
			if (!bValidKey)
			{
				return false;
			}
		}


		return true;
	}

	bool ResetCacheTimestamp(TObjectPtr<UObject>& Asset, UDataflow* DataflowAsset)
	{
		using namespace Dataflow;

		UDataflowBaseContent* BaseContent = Cast< UDataflowBaseContent>(Asset.Get());
		if (!BaseContent) return false;

		BaseContent->SetLastModifiedTimestamp(DataflowAsset->GetRenderingTimestamp().Value+1, false /*bMakeDirty*/);

		return true;
	}

	/*
	* CreateNewDataflowContext
	*
	*/	
	template<class T>
	TObjectPtr<T> CreateNewDataflowContext(const TObjectPtr<UObject>& ContentOwner)
	{
		check(ContentOwner.Get());

		UClass* DataflowClass = T::StaticClass();
		UDataflow* DataflowAsset = Private::GetDataflowAssetFrom(ContentOwner);

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

		FString PackageName = FString::Printf(TEXT("/Game/_GENERATED/Dataflow/%s"), *ContentOwner.GetName());
		if(DataflowAsset)
		{
			PackageName = FString::Printf(TEXT("%s_%s"), *PackageName, *DataflowAsset->GetName());
		}

		UPackage* Package = FindPackage(nullptr, *PackageName);
		if (!Package)
		{
			Package = CreatePackage(*PackageName);
		}

		bool bNeedsNewAsset = true;
		TObjectPtr<UObject> Asset = StaticLoadObject(DataflowClass, Package, *PackageName);
		if (Asset && DataflowAsset)
		{
			// Validate the loaded cache
			bNeedsNewAsset =
				!BindContextToGraph(Asset, DataflowAsset) ||
				!ValidateCachedNodeHash(Asset, DataflowAsset) ||
				!ResetCacheTimestamp(Asset, DataflowAsset);
		}
		
		if(bNeedsNewAsset)
		{
			const FName AssetName(FPackageName::GetLongPackageAssetName(PackageName));
			Asset = NewObject<UObject>(Package, DataflowClass, AssetName, RF_Public | RF_Standalone | RF_Transactional);

			Asset->MarkPackageDirty();
			FAssetRegistryModule::AssetCreated(Asset);

			if (UDataflowBaseContent* BaseContent = Cast< UDataflowBaseContent>(Asset.Get()))
			{
				BaseContent->BuildBaseContent(ContentOwner);
				BaseContent->SetDataflowOwner(ContentOwner);
			}
		}

		return Cast<T>(Asset.Get());
	}

	template TObjectPtr<UDataflowBaseContent> CreateNewDataflowContext(const TObjectPtr<UObject>& ContentOwner);
	template TObjectPtr<UDataflowSkeletalContent> CreateNewDataflowContext(const TObjectPtr<UObject>& ContentOwner);
}


namespace UE::Dataflow::DataflowContext
{
	struct FColorScheme
	{
		static inline const FLinearColor Asset = FColor(180, 120, 110);
		static inline const FLinearColor NodeHeader = FColor(180, 120, 110);
		static inline const FLinearColor NodeBody = FColor(18, 12, 11, 127);
	};
}

FText UAssetDefinition_DataflowContext::GetAssetDisplayName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DataflowContext", "DataflowContext");
}

TSoftClassPtr<UObject> UAssetDefinition_DataflowContext::GetAssetClass() const
{
	return UDataflow::StaticClass();
}

FLinearColor UAssetDefinition_DataflowContext::GetAssetColor() const
{
	return UE::Dataflow::DataflowContext::FColorScheme::Asset;
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_DataflowContext::GetAssetCategories() const
{
	static const auto Categories = { EAssetCategoryPaths::Physics };
	return Categories;
}

UThumbnailInfo* UAssetDefinition_DataflowContext::LoadThumbnailInfo(const FAssetData& InAsset) const
{
	return UE::Editor::FindOrCreateThumbnailInfo(InAsset.GetAsset(), USceneThumbnailInfo::StaticClass());
}


#undef LOCTEXT_NAMESPACE
