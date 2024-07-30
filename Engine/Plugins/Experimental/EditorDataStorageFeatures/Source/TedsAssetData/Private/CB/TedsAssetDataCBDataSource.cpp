// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsAssetDataCBDataSource.h"

#include "TedsAssetDataColumns.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "ContentBrowserDataUtils.h"
#include "Elements/Common/TypedElementQueryTypes.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/PathViews.h"
#include "PluginDescriptor.h"
#include "Settings/ContentBrowserSettings.h"
#include "UObject/NameTypes.h"

namespace UE::EditorDataStorage::AssetData::Private
{

FTedsAssetDataCBDataSource::FTedsAssetDataCBDataSource(ITypedElementDataStorageInterface& InDatabase)
	: Database(InDatabase)
{
	using namespace TypedElementQueryBuilder;
	using namespace EditorDataStorage;

	InitVirtualPathProcessor();

	auto GenerateVirtualPaths = [DataSource = static_cast<const FTedsAssetDataCBDataSource*>(this)](const FStringView InAssetPath, FNameBuilder& OutVirtualizedPath) -> bool
	{
		if (!ContentBrowserDataUtils::PathPassesAttributeFilter(InAssetPath, 0, EContentBrowserItemAttributeFilter::IncludeAll))
		{
			return false;
		}

		DataSource->VirtualPathProcessor.ConvertInternalPathToVirtualPath(InAssetPath, OutVirtualizedPath);
		return true;
	};

	ProcessPathQuery = Database.RegisterQuery(
		Select(
			TEXT("FTedsAssetDataCBDataSource: Process Path updates"),
			FProcessor(TypedElementDataStorage::EQueryTickPhase::DuringPhysics, Database.GetQueryTickGroupName(TypedElementDataStorage::EQueryTickGroups::Update)),
			[GenerateVirtualPaths](TypedElementDataStorage::IQueryContext& Context, const TypedElementDataStorage::RowHandle* Rows, const FAssetPathColumn_Experimental* PathColumn)
			{
				int32 NumOfRowToProcess = Context.GetRowCount();

				FNameBuilder InternalPath;
				FNameBuilder VirtualPath;

				for (int32 Index = 0; Index < NumOfRowToProcess; ++Index)
				{
					PathColumn[Index].Path.ToString(InternalPath);

					if (GenerateVirtualPaths(InternalPath, VirtualPath))
					{
						FVirtualPathColumn_Experimental VirtualPathColumn;
						VirtualPathColumn.VirtualPath = *VirtualPath;
						// Todo investigate for a batch add Maybe?
						Context.AddColumn(Rows[Index], MoveTemp(VirtualPathColumn));
					}
				}
			}
		)
		.Where()
			.All<FUpdatedPathTag>()
		.Compile()
		);

	ProcessAssetDataPathUpdateQuery = Database.RegisterQuery(
		Select(
			TEXT("FTedsAssetDataCBDataSource:: Process Asset Data Path Update"),
			FProcessor(TypedElementDataStorage::EQueryTickPhase::DuringPhysics, Database.GetQueryTickGroupName(TypedElementDataStorage::EQueryTickGroups::Update)),
			[GenerateVirtualPaths](TypedElementDataStorage::IQueryContext& Context, const TypedElementDataStorage::RowHandle Row, const FAssetDataColumn_Experimental& AssetDataColumn)
			{
				int32 NumOfRowToProcess = Context.GetRowCount();

				FNameBuilder InternalPath;
				FNameBuilder VirtualPath;

				AssetDataColumn.AssetData.AppendObjectPath(InternalPath);

				if (GenerateVirtualPaths(InternalPath, VirtualPath))
				{
					FVirtualPathColumn_Experimental VirtualPathColumn;
					VirtualPathColumn.VirtualPath = *VirtualPath;
					// Todo investigate for a batch add Maybe?
					Context.AddColumn(Row, MoveTemp(VirtualPathColumn));
				}
			}
		)
		.Where()
			.All<FUpdatedPathTag>()
			.None<FUpdatedAssetDataTag>()
		.Compile()
		);


	// For now just add the columns one by one but this should be rework to work in batch
	auto AddAssetDataColumns = [DataSource = static_cast<const FTedsAssetDataCBDataSource*>(this), AssetRegistry = static_cast<const IAssetRegistry*>(&IAssetRegistry::GetChecked())](const FAssetData& InAssetData, TypedElementDataStorage::IQueryContext& InContext, TypedElementDataStorage::RowHandle Row)
	{
		// Not optimized at all but we would like to have the data in sooner for testing purposes.
		
		if (InAssetData.HasAnyPackageFlags(PKG_NotExternallyReferenceable))
		{
			// Private Asset
			InContext.AddColumns<FAssetTag, FPrivateAssetTag>(Row);
		}
		else
		{
			InContext.AddColumns<FAssetTag, FPublicAssetTag>(Row);
		}

		if (TOptional<FAssetPackageData> PackageData = AssetRegistry->GetAssetPackageDataCopy(InAssetData.PackageName))
		{
			FDiskSizeColumn DiskSizeColumn;
			DiskSizeColumn.DiskSize = PackageData->DiskSize;
			InContext.AddColumn(Row, MoveTemp(DiskSizeColumn));
		}

		FItemNameColumn_Experimental ItemNameColumn;
		ItemNameColumn.Name = InAssetData.AssetName;
	};

	ProcessAssetDataAndPathUpdateQuery = Database.RegisterQuery(
		Select(
			TEXT("FTedsAssetDataCBDataSource: Process Asset Data and Path Updates"),
			FProcessor(TypedElementDataStorage::EQueryTickPhase::DuringPhysics, Database.GetQueryTickGroupName(TypedElementDataStorage::EQueryTickGroups::Update)),
			[GenerateVirtualPaths, AddAssetDataColumns](TypedElementDataStorage::IQueryContext& Context, const TypedElementDataStorage::RowHandle Row, const FAssetDataColumn_Experimental& AssetDataColumn)
			{

				FNameBuilder InternalPath;
				FNameBuilder VirtualPath;

				InternalPath.Reset();
				AssetDataColumn.AssetData.AppendObjectPath(InternalPath);

				if (GenerateVirtualPaths(InternalPath, VirtualPath))
				{
					FVirtualPathColumn_Experimental VirtualPathColumn;
					VirtualPathColumn.VirtualPath = *VirtualPath;
					// Todo investigate for a batch add Maybe?
					Context.AddColumn(Row, MoveTemp(VirtualPathColumn));

					AddAssetDataColumns(AssetDataColumn.AssetData, Context, Row);
				}
			}
		)
		.Where()
			.All<FUpdatedAssetDataTag, FUpdatedPathTag>()
			.Compile()
		);

	ProcessAssetDataUpdateQuery = Database.RegisterQuery(
		Select(
			TEXT("FTedsAssetDataCBDataSource: Process Asset Data updates"),
			FProcessor(TypedElementDataStorage::EQueryTickPhase::DuringPhysics, Database.GetQueryTickGroupName(TypedElementDataStorage::EQueryTickGroups::Update)),
			[AddAssetDataColumns](TypedElementDataStorage::IQueryContext& Context, const TypedElementDataStorage::RowHandle Row, const FAssetDataColumn_Experimental& AssetDataColumn)
			{
				AddAssetDataColumns(AssetDataColumn.AssetData, Context, Row);
			}
		)
		.Where()
			.All<FUpdatedAssetDataTag>()
			.None<FUpdatedPathTag>()
		.Compile()
		);

}

FTedsAssetDataCBDataSource::~FTedsAssetDataCBDataSource()
{
	// Not needed on a editor shut down
	if (!IsEngineExitRequested())
	{
		Database.UnregisterQuery(ProcessAssetDataUpdateQuery);
		Database.UnregisterQuery(ProcessAssetDataAndPathUpdateQuery);
		Database.UnregisterQuery(ProcessAssetDataPathUpdateQuery);
		Database.UnregisterQuery(ProcessPathQuery);

		IPluginManager& PluginManager = IPluginManager::Get();
		PluginManager.OnNewPluginContentMounted().RemoveAll(this);
		PluginManager.OnPluginEdited().RemoveAll(this);
		PluginManager.OnPluginUnmounted().RemoveAll(this);
	}
}

void FTedsAssetDataCBDataSource::InitVirtualPathProcessor()
{
	IPluginManager& PluginManager = IPluginManager::Get();

	PluginManager.OnNewPluginContentMounted().AddRaw(this, &FTedsAssetDataCBDataSource::OnPluginContentMounted);
	PluginManager.OnPluginEdited().AddRaw(this, &FTedsAssetDataCBDataSource::OnPluginContentMounted);
	PluginManager.OnPluginUnmounted().AddRaw(this, &FTedsAssetDataCBDataSource::OnPluginUnmounted);

	TArray<TSharedRef<IPlugin>> EnabledPluginsWithContent = PluginManager.GetEnabledPluginsWithContent();
	VirtualPathProcessor.PluginNameToCachedData.Reserve(EnabledPluginsWithContent.Num());

	for (const TSharedRef<IPlugin>& Plugin : EnabledPluginsWithContent)
	{
		FVirtualPathProcessor::FCachedPluginData& Data = VirtualPathProcessor.PluginNameToCachedData.FindOrAdd(Plugin->GetName());
		Data.LoadedFrom = Plugin->GetLoadedFrom();
		Data.EditorCustomVirtualPath = Plugin->GetDescriptor().EditorCustomVirtualPath;
	}

	const UContentBrowserSettings* ContentBrowserSettings = GetDefault<UContentBrowserSettings>();
	VirtualPathProcessor.bShowAllFolder = ContentBrowserSettings->bShowAllFolder;
	VirtualPathProcessor.bOrganizeFolders = ContentBrowserSettings->bOrganizeFolders;
}

void FTedsAssetDataCBDataSource::OnPluginContentMounted(IPlugin& InPlugin)
{
	FVirtualPathProcessor::FCachedPluginData& Data = VirtualPathProcessor.PluginNameToCachedData.FindOrAdd(InPlugin.GetName());
	Data.LoadedFrom = InPlugin.GetLoadedFrom();
	Data.EditorCustomVirtualPath = InPlugin.GetDescriptor().EditorCustomVirtualPath;
}

void FTedsAssetDataCBDataSource::OnPluginUnmounted(IPlugin& InPlugin)
{
	VirtualPathProcessor.PluginNameToCachedData.Remove(InPlugin.GetName());
}

void FTedsAssetDataCBDataSource::FVirtualPathProcessor::ConvertInternalPathToVirtualPath(const FStringView InternalPath, FStringBuilderBase& OutVirtualPath) const
{
	OutVirtualPath.Reset();

	if (bShowAllFolder)
	{
		OutVirtualPath.AppendChar(TEXT('/'));
		if (InternalPath.Len() == 1 && InternalPath[0] == TEXT('/'))
		{
			return;
		}
	}

	if (bOrganizeFolders && InternalPath.Len() > 1)
	{
		const FStringView MountPoint = FPathViews::GetMountPointNameFromPath(InternalPath);
		const int32 MountPointHash = GetTypeHash(MountPoint);
		if (const FTedsAssetDataCBDataSource::FVirtualPathProcessor::FCachedPluginData* Plugin = PluginNameToCachedData.FindByHash(MountPointHash, MountPoint))
		{
			if (Plugin->LoadedFrom == EPluginLoadedFrom::Engine)
			{
				OutVirtualPath.Append(TEXT("/EngineData/Plugins"));
			}
			else
			{
				OutVirtualPath.Append(TEXT("/Plugins"));
			}

			if (!Plugin->EditorCustomVirtualPath.IsEmpty())
			{
				int32 NumCharsToCopy = Plugin->EditorCustomVirtualPath.Len();
				if (Plugin->EditorCustomVirtualPath[NumCharsToCopy - 1] == TEXT('/'))
				{
					--NumCharsToCopy;
				}

				if (NumCharsToCopy > 0)
				{
					if (Plugin->EditorCustomVirtualPath[0] != TEXT('/'))
					{
						OutVirtualPath.AppendChar(TEXT('/'));
					}

					OutVirtualPath.Append(*Plugin->EditorCustomVirtualPath, NumCharsToCopy);
				}
			}
		}
		else if (MountPoint.Equals(TEXT("Engine")))
		{
			OutVirtualPath.Append(TEXT("/EngineData"));
		}

	}

	OutVirtualPath.Append(InternalPath.GetData(), InternalPath.Len());
}

} // End of Namespace UE::TypedElementDataStorageAssetData::Private
