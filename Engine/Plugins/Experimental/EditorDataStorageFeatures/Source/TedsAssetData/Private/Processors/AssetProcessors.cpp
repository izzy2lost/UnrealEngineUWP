// Copyright Epic Games, Inc. All Rights Reserved.

#include "Processors/AssetProcessors.h"

#include "AssetDefinition.h"
#include "AssetDefinitionRegistry.h"
#include "CollectionManagerTypes.h"
#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserDataModule.h"
#include "Elements/Columns/TypedElementFolderColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "Experimental/ContentBrowserExtensionUtils.h"
#include "TedsAssetDataColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Framework/TypedElementIndexHasher.h"

void UTedsAssetDataFactory::RegisterQueries(IEditorDataStorageProvider& DataStorage)
{
	using namespace UE::Editor::DataStorage::Queries;

	DataStorage.RegisterQuery(
		Select(
			TEXT("TedsAssetDataFactory: Sync folder color from world"),
			FProcessor(EQueryTickPhase::PostPhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::SyncExternalToDataStorage))
			.SetExecutionMode(EExecutionMode::GameThread),
			[](IQueryContext& Context, const RowHandle* Rows, const FAssetPathColumn_Experimental* AssetPathColumn, FSlateColorColumn* ColorColumn)
			{
				const int32 NumOfRowToProcess = Context.GetRowCount();

				for (int32 Index = 0; Index < NumOfRowToProcess; ++Index)
				{
					if (TOptional<FLinearColor> Color = UE::Editor::ContentBrowser::ExtensionUtils::GetFolderColor(AssetPathColumn[Index].Path))
					{
						ColorColumn[Index].Color = Color.GetValue();
					}
				}
			}
		)
		.Where()
			.All<FFolderTag, FUpdatedPathTag>()
		.Compile()
		);

	DataStorage.RegisterQuery(
		Select(
			TEXT("TedsAssetDataFactory: Sync folder color back to world"),
			FProcessor(EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::SyncDataStorageToExternal))
			.SetExecutionMode(EExecutionMode::GameThread),
			[](IQueryContext& Context, const RowHandle* Rows, const FAssetPathColumn_Experimental* PathColumn, const FSlateColorColumn* ColorColumn)
			{
				const int32 NumOfRowToProcess = Context.GetRowCount();

				for (int32 Index = 0; Index < NumOfRowToProcess; ++Index)
				{
					UE::Editor::ContentBrowser::ExtensionUtils::SetFolderColor(PathColumn[Index].Path, ColorColumn[Index].Color.GetSpecifiedColor());
				}
			}
		)
		.Where()
			.All<FFolderTag, FTypedElementSyncBackToWorldTag>()
		.Compile()
		);
}

void UTedsAssetDataFactory::PreRegister(IEditorDataStorageProvider& DataStorage)
{
	if (FContentBrowserModule* ContentBrowserModule = FModuleManager::Get().GetModulePtr<FContentBrowserModule>("ContentBrowser"))
	{
		ContentBrowserModule->GetOnSetFolderColor().AddUObject(this, &UTedsAssetDataFactory::OnSetFolderColor, &DataStorage);
	}
}

void UTedsAssetDataFactory::PreShutdown(IEditorDataStorageProvider& DataStorage)
{
	if (FContentBrowserModule* ContentBrowserModule = FModuleManager::Get().GetModulePtr<FContentBrowserModule>("ContentBrowser"))
	{
		ContentBrowserModule->GetOnSetFolderColor().RemoveAll(this);
	}
}

void UTedsAssetDataFactory::OnSetFolderColor(const FString& Path, IEditorDataStorageProvider* DataStorage)
{
	const UE::Editor::DataStorage::IndexHash PathHash = UE::Editor::DataStorage::GenerateIndexHash(FName(Path));
	const UE::Editor::DataStorage::RowHandle Row = DataStorage->FindIndexedRow(PathHash);

	if(DataStorage->IsRowAvailable(Row))
	{
		DataStorage->AddColumn<FUpdatedPathTag>(Row);
	}
}
