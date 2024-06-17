// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/UnrealString.h"
#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Framework/TypedElementIndexHasher.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "HAL/IConsoleManager.h"
#include "TedsAssetDataColumns.h"
#include "UObject/NameTypes.h"


DEFINE_LOG_CATEGORY_STATIC(LogTEDSAssetRegistry, Log, All)

static FAutoConsoleCommand CCMDTestFolderRowData(
	TEXT("TEDS.Debug.ShowDataOfAssetFolder"),
	TEXT("Print some debug information on the specified path."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
		{
			const ITypedElementDataStorageInterface* Database = UTypedElementRegistry::GetInstance()->GetDataStorage();
			IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

			for (const FString& Path : InArgs)
			{
				FName PathAsName(*Path);
				TypedElementDataStorage::RowHandle RowHandle = Database->FindIndexedRow(TypedElementDataStorage::GenerateIndexHash(PathAsName));

				if (Database->IsRowAssigned(RowHandle))
				{
					UE_LOG(LogTEDSAssetRegistry, Display, TEXT("The path isn't indexed."));
					return;
				}


				UE_LOG(LogTEDSAssetRegistry, Display, TEXT("Found some information for the path (%s) in the database."), *Path);

				if (const FAssetPathColumn_Experimental* AssetPath = Database->GetColumn<FAssetPathColumn_Experimental>(RowHandle))
				{
					UE_LOG(LogTEDSAssetRegistry, Display, TEXT("Path stored in the database as (%s)."), *AssetPath->Path.ToString());
				}

				if (const FParentAssetPathColumn_Experimental* ParentAssetPath = Database->GetColumn<FParentAssetPathColumn_Experimental>(RowHandle))
				{
					if (const FAssetPathColumn_Experimental* AssetPath = Database->GetColumn<FAssetPathColumn_Experimental>(ParentAssetPath->ParentRow))
					{
						UE_LOG(LogTEDSAssetRegistry, Display, TEXT("	Parent Path: %s"), *AssetPath->Path.ToString());
					}
				}


				if (const FChildrenAssetPathColumn_Experimental* ChildrenPath = Database->GetColumn<FChildrenAssetPathColumn_Experimental>(RowHandle))
				{
					UE_LOG(LogTEDSAssetRegistry, Display, TEXT("	Path as %i children"), ChildrenPath->ChildrenRows.Num());

					for (TypedElementDataStorage::RowHandle Row : ChildrenPath->ChildrenRows)
					{
						if (const FAssetPathColumn_Experimental* AssetPath = Database->GetColumn<FAssetPathColumn_Experimental>(Row))
						{
							UE_LOG(LogTEDSAssetRegistry, Display, TEXT("		Children Path: %s"), *AssetPath->Path.ToString());
						}
					}
				}


				if (const FAssetsInPathColumn_Experimental* AssetInPath = Database->GetColumn<FAssetsInPathColumn_Experimental>(RowHandle))
				{
					UE_LOG(LogTEDSAssetRegistry, Display, TEXT("	Asset in Paths"));

					for (TypedElementDataStorage::RowHandle AssetRow : AssetInPath->AssetsRow)
					{
						if (const FAssetDataColumn_Experimental* AssetData = Database->GetColumn<FAssetDataColumn_Experimental>(AssetRow))
						{
							UE_LOG(LogTEDSAssetRegistry, Display, TEXT("		Asset Name: %s"), *AssetData->AssetData.AssetName.ToString());
						}
						else
						{
							UE_LOG(LogTEDSAssetRegistry, Display, TEXT("		Asset Row pointed to stale asset."));
						}
					}
				}


				// Check for asset that haven't processed their path asset in path column yet
				TArray<FAssetData> Assets;
				AssetRegistry.GetAssetsByPath(PathAsName, Assets);
				for (const FAssetData& Asset : Assets)
				{
					TypedElementDataStorage::RowHandle AssetRow = Database->FindIndexedRow(TypedElementDataStorage::GenerateIndexHash(Asset.GetSoftObjectPath()));

					if (const FUnresolvedAssetsInPathColumn_Experimental* UnresolvedAssetsInPathColumn = Database->GetColumn<FUnresolvedAssetsInPathColumn_Experimental>(AssetRow))
					{
							UE_LOG(LogTEDSAssetRegistry, Display, TEXT("		Unresolved asset in path asset. Asset Name: %s"), *Asset.AssetName.ToString());
					}
				}
			}
		}));


static FAutoConsoleCommand CCMDTestFolderAssetRegistryData(
	TEXT("TEDS.Debug.ShowAssetRegistryDataOfFolder"),
	TEXT("Print some debug information on the specified path."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
		{
			IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

			FNameBuilder NameBuilder;
			for (const FString& Path : InArgs)
			{
				UE_LOG(LogTEDSAssetRegistry, Display, TEXT("Displaying asset registry info on path (%s)"), *Path);
				const FName PathAsName(*Path);
				AssetRegistry.EnumerateSubPaths(PathAsName, [&NameBuilder](FName InPath)
					{
						InPath.AppendString(NameBuilder);
						UE_LOG(LogTEDSAssetRegistry, Display, TEXT("	Children Path: %s"), *NameBuilder);
						NameBuilder.Reset();
						return true;
					}, false);


				TArray<FAssetData> Assets;
				AssetRegistry.GetAssetsByPath(PathAsName, Assets);

				if (!Assets.IsEmpty())
				{ 
					UE_LOG(LogTEDSAssetRegistry, Display, TEXT("	Asset in Path"));

					for (const FAssetData& Asset : Assets)
					{
						Asset.AssetName.AppendString(NameBuilder);
						UE_LOG(LogTEDSAssetRegistry, Display, TEXT("		Asset Name: %s"), *NameBuilder);
						NameBuilder.Reset();

						Asset.GetFullName(NameBuilder);
						UE_LOG(LogTEDSAssetRegistry, Display, TEXT("		Asset Full Name: %s"), *NameBuilder);
						NameBuilder.Reset();

						Asset.PackageName.AppendString(NameBuilder);
						UE_LOG(LogTEDSAssetRegistry, Display, TEXT("		Asset Reported Package Path: %s"), *NameBuilder);
						NameBuilder.Reset();
					}
				}
			}
		}));