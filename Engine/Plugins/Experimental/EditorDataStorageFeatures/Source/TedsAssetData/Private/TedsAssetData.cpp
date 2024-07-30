// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsAssetData.h"

#include "TedsAssetDataColumns.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Async/ParallelFor.h"
#include "Containers/Array.h"
#include "Containers/ChunkedArray.h"
#include "Elements/Common/TypedElementQueryTypes.h"
#include "Elements/Framework/TypedElementIndexHasher.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Engine/Engine.h"

#define TRACK_TEDSASSETDATA_MEMORY 0

namespace UE::EditorDataStorage::AssetData::Private
{
	
constexpr int32 ParallelForMinBatchSize = 1024 * 4;

struct FPopulateAssetDataRowArgs
{
	FAssetData AssetData;

	TypedElementDataStorage::IndexHash ObjectPathHash;
	TypedElementDataStorage::IndexHash PathHash;
	TypedElementDataStorage::RowHandle PathRow = TypedElementDataStorage::InvalidRowHandle;
};

// Only safe if the GT is blocked during the operation
template<typename TAssetData>
FPopulateAssetDataRowArgs ThreadSafe_PopulateAssetDataTableRow(TAssetData&& InAssetData, const ITypedElementDataStorageInterface& Database)
{
	FPopulateAssetDataRowArgs Output;
	Output.ObjectPathHash = TypedElementDataStorage::GenerateIndexHash(InAssetData.GetSoftObjectPath());

	// Looks safe but might not be depending on the implementation of the database
	if (Database.IsRowAssigned(Database.FindIndexedRow(Output.ObjectPathHash)))
	{
		// No need to initialize the rest of the row here. The invalid's asset data will be used as flag to skip the data generated here.
		return Output;
	}

	Output.PathHash = TypedElementDataStorage::GenerateIndexHash(InAssetData.PackagePath);

	// Looks safe but might not be depending on the implementation of the database
	Output.PathRow = Database.FindIndexedRow(Output.PathHash);
	Output.AssetData = MoveTempIfPossible(InAssetData);

	return Output;
}


void PopulateAssetDataTableRow(FPopulateAssetDataRowArgs&& InAssetDataRowArgs, ITypedElementDataStorageInterface& Database, TypedElementDataStorage::RowHandle RowHandle)
{
	if (InAssetDataRowArgs.PathRow !=  TypedElementDataStorage::InvalidRowHandle)
	{
		Database.GetColumn<FAssetsInPathColumn_Experimental>(InAssetDataRowArgs.PathRow)->AssetsRow.Add(RowHandle);
	}
	else
	{
		FUnresolvedAssetsInPathColumn_Experimental UnresolvedAssetsInPathColumn;
		UnresolvedAssetsInPathColumn.Hash = InAssetDataRowArgs.PathHash;
		Database.AddColumn<FUnresolvedAssetsInPathColumn_Experimental>(RowHandle, MoveTemp(UnresolvedAssetsInPathColumn));
	}

	Database.GetColumn<FAssetDataColumn_Experimental>(RowHandle)->AssetData =  MoveTemp(InAssetDataRowArgs.AssetData);
}


struct FPopulatePathRowArgs
{
	FName AssetRegistryPath;
	TypedElementDataStorage::IndexHash AssetRegistryPathHash;
	TypedElementDataStorage::IndexHash ParentAssetRegistryPathHash;
	uint32 PathDepth;

	operator bool() const 
	{
		return !AssetRegistryPath.IsNone();
	}

	void MarkAsInvalid()
	{
		AssetRegistryPath = FName();
	}
};

void GetPathDepthAndParentFolderIndex(FStringView Path, uint32& OutDepth, int32& OutParentFolderIndex)
{
	OutDepth = 0;
	OutParentFolderIndex = INDEX_NONE;

	if (Path.Len() > 1)
	{
		OutParentFolderIndex = 1;
		++OutDepth;
	}

	// Skip the first '/'
	for (int32 Index = 1; Index < Path.Len(); ++Index)
	{
		if (Path[Index] == TEXT('/'))
		{
			++OutDepth;
			OutParentFolderIndex = Index;
		}
	}
}

// Only thread safe if the game thread is blocked 
FPopulatePathRowArgs ThreadSafe_PopulatePathRowArgs(TypedElementDataStorage::IndexHash AssetRegistryPathHash, FName InAssetRegistryPath, FStringView PathAsString)
{
	TypedElementDataStorage::IndexHash ParentAssetRegistryPathHash = TypedElementDataStorage::InvalidRowHandle;
	int32 CharacterIndex;
	uint32 Depth;
	GetPathDepthAndParentFolderIndex(PathAsString, Depth, CharacterIndex);
	if (CharacterIndex != INDEX_NONE)
	{
		FStringView ParentPath = PathAsString.Left(CharacterIndex);
		ParentAssetRegistryPathHash = TypedElementDataStorage::GenerateIndexHash(FName(ParentPath));
	}

	FPopulatePathRowArgs Args;
	Args.AssetRegistryPath = InAssetRegistryPath;
	Args.AssetRegistryPathHash = AssetRegistryPathHash;
	Args.ParentAssetRegistryPathHash = ParentAssetRegistryPathHash;
	Args.PathDepth = Depth;

	return Args;
}

void PopulatePathDataTableRow(FPopulatePathRowArgs&& InPopulatePathRowArgs, ITypedElementDataStorageInterface& Database, TypedElementDataStorage::RowHandle RowHandle)
{
	if (InPopulatePathRowArgs.ParentAssetRegistryPathHash != TypedElementDataStorage::InvalidRowHandle)
	{
		const TypedElementDataStorage::RowHandle ParentRow = Database.FindIndexedRow(InPopulatePathRowArgs.ParentAssetRegistryPathHash);
		if (Database.IsRowAssigned(ParentRow))
		{
			Database.GetColumn<FChildrenAssetPathColumn_Experimental>(ParentRow)->ChildrenRows.Add(RowHandle);
			Database.GetColumn<FParentAssetPathColumn_Experimental>(RowHandle)->ParentRow = ParentRow;
		}
		else 
		{
			// If we were unlucky we may be missing the parent path data.This should resolve it self during a latter event on paths added event.
			FUnresolvedParentAssetPathColumn_Experimental UnresolvedParentRow;
			UnresolvedParentRow.Hash = InPopulatePathRowArgs.ParentAssetRegistryPathHash;
			Database.AddColumn(RowHandle, MoveTemp(UnresolvedParentRow));
		}
	}

	Database.GetColumn<FAssetPathColumn_Experimental>(RowHandle)->Path = InPopulatePathRowArgs.AssetRegistryPath;
}


FTedsAssetData::FTedsAssetData(ITypedElementDataStorageInterface& InDatabase)
	: Database(InDatabase)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::FTedsAssetData);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

	// Register to events from asset registry
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

	AssetRegistry.OnAssetsAdded().AddRaw(this, &FTedsAssetData::OnAssetsAdded);
	AssetRegistry.OnAssetsRemoved().AddRaw(this, &FTedsAssetData::OnAssetsRemoved);
	AssetRegistry.OnAssetsUpdated().AddRaw(this, &FTedsAssetData::OnAssetsUpdated);
	AssetRegistry.OnAssetRenamed().AddRaw(this, &FTedsAssetData::OnAssetRenamed);
	AssetRegistry.OnAssetsUpdatedOnDisk().AddRaw(this, &FTedsAssetData::OnAssetsUpdatedOnDisk);
	AssetRegistry.OnPathsAdded().AddRaw(this, &FTedsAssetData::OnPathsAdded);
	AssetRegistry.OnPathsRemoved().AddRaw(this, &FTedsAssetData::OnPathsRemoved);

	// Register data types to TEDS
	PathsTable = Database.FindTable(FName(TEXT("Editor_AssetRegistryPathsTable")));
	if (PathsTable == TypedElementDataStorage::InvalidTableHandle)
	{
		PathsTable = Database.RegisterTable<FAssetPathColumn_Experimental, FChildrenAssetPathColumn_Experimental, FParentAssetPathColumn_Experimental, FAssetsInPathColumn_Experimental>(FName(TEXT("Editor_AssetRegistryPathsTable")));
	}

	AssetsDataTable = Database.FindTable(FName(TEXT("Editor_AssetRegistryAssetDataTable")));
	if (AssetsDataTable == TypedElementDataStorage::InvalidTableHandle)
	{
		AssetsDataTable = Database.RegisterTable<FAssetDataColumn_Experimental, FUpdatedPathTag, FUpdatedAssetDataTag>(FName(TEXT("Editor_AssetRegistryAssetDataTable")));
	}


	RemoveUpdatedPathTagQuery = Database.RegisterQuery(
			TypedElementQueryBuilder::Select(
				TEXT("FTedsAssetData: Remove Updated Path Tag"),
				TypedElementQueryBuilder::FPhaseAmble(TypedElementQueryBuilder::FPhaseAmble::ELocation::Postamble, TypedElementDataStorage::EQueryTickPhase::FrameEnd),
				[](TypedElementDataStorage::IQueryContext& Context, const TypedElementDataStorage::RowHandle* Rows)
				{
					Context.RemoveColumns<FUpdatedPathTag>(TConstArrayView<TypedElementRowHandle>(Rows, Context.GetRowCount()));
				}
			)
			.Where()
				.All<FUpdatedPathTag>()
			.Compile()
		);

	RemoveUpdatedAssetDataTagQuery = Database.RegisterQuery(
			TypedElementQueryBuilder::Select(
				TEXT("FTedsAssetData: Remove Updated Asset Data Tag"),
				TypedElementQueryBuilder::FPhaseAmble(TypedElementQueryBuilder::FPhaseAmble::ELocation::Postamble, TypedElementDataStorage::EQueryTickPhase::FrameEnd),
				[](TypedElementDataStorage::IQueryContext& Context, const TypedElementDataStorage::RowHandle* Rows)
				{
					Context.RemoveColumns<FUpdatedAssetDataTag>(TConstArrayView<TypedElementRowHandle>(Rows, Context.GetRowCount()));
				}
			)
			.Where()
				.All<FUpdatedAssetDataTag>()
				.Compile()
		);

	UpdateAssetsInPathQuery = Database.RegisterQuery(
		TypedElementQueryBuilder::Select()
			.ReadWrite<FAssetsInPathColumn_Experimental>()
			.Compile()
		);

	ResolveMissingAssetInPathQuery = Database.RegisterQuery(
		TypedElementQueryBuilder::Select(
			TEXT("FTedsAssetData: Resolve Missing Asset In Path"),
			TypedElementQueryBuilder::FProcessor(TypedElementDataStorage::EQueryTickPhase::FrameEnd, Database.GetQueryTickGroupName(TypedElementDataStorage::EQueryTickGroups::Default)),
			[this](TypedElementDataStorage::IQueryContext& Context, TypedElementDataStorage::RowHandle Row, const FUnresolvedAssetsInPathColumn_Experimental& UnresolvedAssetPath)
			{
#if TRACK_TEDSASSETDATA_MEMORY
				LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

				const TypedElementDataStorage::RowHandle PathRow = Context.FindIndexedRow(UnresolvedAssetPath.Hash);
				if (Context.IsRowAssigned(PathRow))
				{
					Context.RemoveColumns<FUnresolvedAssetsInPathColumn_Experimental>(Row);
					Context.RunSubquery(0, PathRow, 
							TypedElementQueryBuilder::CreateSubqueryCallbackBinding([Row](FAssetsInPathColumn_Experimental& AssetsInPath)
							{
#if TRACK_TEDSASSETDATA_MEMORY
								LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

								AssetsInPath.AssetsRow.Add(Row);
							})
						);
				}
			}
		)
		.DependsOn()
			.SubQuery(UpdateAssetsInPathQuery)
		.Compile()
	);

	UpdateParentToChildrenAssetPathQuery = Database.RegisterQuery(
		TypedElementQueryBuilder::Select()
			.ReadWrite<FChildrenAssetPathColumn_Experimental>()
			.Compile()
		);

	ResolveMissingParentPathQuery = Database.RegisterQuery(
		TypedElementQueryBuilder::Select(
			TEXT("FTedsAssetData: Resolve Missing Parent Path Row"),
			TypedElementQueryBuilder::FProcessor(TypedElementDataStorage::EQueryTickPhase::FrameEnd, Database.GetQueryTickGroupName(TypedElementDataStorage::EQueryTickGroups::Default)),
			[this](TypedElementDataStorage::IQueryContext& Context, TypedElementDataStorage::RowHandle Row, const FUnresolvedParentAssetPathColumn_Experimental& UnresolvedParentAssetPath, FParentAssetPathColumn_Experimental& ParentAssetPathColumn)
			{
#if TRACK_TEDSASSETDATA_MEMORY
				LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

				const TypedElementDataStorage::RowHandle ParentPathRow = Context.FindIndexedRow(UnresolvedParentAssetPath.Hash);
				if (Context.IsRowAssigned(ParentPathRow))
				{
					Context.RemoveColumns<FUnresolvedParentAssetPathColumn_Experimental>(Row);
					ParentAssetPathColumn.ParentRow = ParentPathRow;

					Context.RunSubquery(0, ParentPathRow, 
							TypedElementQueryBuilder::CreateSubqueryCallbackBinding([Row](FChildrenAssetPathColumn_Experimental& ChildrenPathColumn)
							{
#if TRACK_TEDSASSETDATA_MEMORY
								LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif
								ChildrenPathColumn.ChildrenRows.Add(Row);
							})
						);
				}
			}
		)
		.DependsOn()
			.SubQuery(UpdateParentToChildrenAssetPathQuery)
		.Compile()
	);

	// Init with the data existing at moment in asset registry

	TArray<FAssetData> AssetsData;
	AssetRegistry.GetAllAssets(AssetsData);

	TChunkedArray<FName> CachedPaths;
	AssetRegistry.EnumerateAllCachedPaths([&CachedPaths](FName Name)
		{
			CachedPaths.AddElement(Name);
			return true;
		});

	TArray<FNameBuilder> NameBuilders;
	TArray<FPopulatePathRowArgs> PopulatePathRowArgs;
	PopulatePathRowArgs.AddUninitialized(CachedPaths.Num());

	// Prepare Path Rows
	ParallelForWithTaskContext(TEXT("Populating TEDS Asset Registry Path"), NameBuilders, CachedPaths.Num(),ParallelForMinBatchSize, [&PopulatePathRowArgs, &CachedPaths, this](FNameBuilder& NameBuilder, int32 Index)
		{
			FName Path = CachedPaths[Index];
			Path.ToString(NameBuilder);
			PopulatePathRowArgs[Index] = ThreadSafe_PopulatePathRowArgs(TypedElementDataStorage::GenerateIndexHash(Path), Path, NameBuilder);
		});

	TArray<TypedElementDataStorage::RowHandle> ReservedRows;
	ReservedRows.AddUninitialized(PopulatePathRowArgs.Num() + AssetsData.Num());
	Database.BatchReserveRows(ReservedRows);

	TArray<TPair<TypedElementDataStorage::IndexHash, TypedElementDataStorage::RowHandle>> IndexesToReservedRows;
	IndexesToReservedRows.AddUninitialized(ReservedRows.Num());

	// Index Reserved Path Rows
	TConstArrayView<TypedElementDataStorage::RowHandle> ReservedPopulatePathRows(ReservedRows.GetData(), PopulatePathRowArgs.Num());
	TArrayView<TPair<TypedElementDataStorage::IndexHash, TypedElementDataStorage::RowHandle>> IndexesToReservedPathRows(IndexesToReservedRows.GetData(), PopulatePathRowArgs.Num());
	ParallelFor(TEXT("Populating TEDS Asset Registry Path Data Indexes"), ReservedPopulatePathRows.Num(), ParallelForMinBatchSize, [&IndexesToReservedPathRows, &ReservedPopulatePathRows, &PopulatePathRowArgs](int32 Index)
		{
			IndexesToReservedPathRows[Index] = TPair<TypedElementDataStorage::IndexHash, TypedElementDataStorage::RowHandle>(PopulatePathRowArgs[Index].AssetRegistryPathHash, ReservedPopulatePathRows[Index]);
		});
	Database.BatchIndexRows(IndexesToReservedPathRows);

	// Populate Path Rows
	Database.BatchAddRow(PathsTable, ReservedPopulatePathRows, [PathRowArgs = MoveTemp(PopulatePathRowArgs), Index = 0, this](TypedElementDataStorage::RowHandle InRowHandle) mutable
		{
			PopulatePathDataTableRow(MoveTemp(PathRowArgs[Index]), Database, InRowHandle);
			++Index;
		});

	// Prepare Asset Data Rows
	TArray<FPopulateAssetDataRowArgs> PopulateAssetDataRowArgs;
	PopulateAssetDataRowArgs.AddDefaulted(AssetsData.Num());
	ParallelFor(TEXT("Populating TEDS Asset Registry Asset Data"), PopulateAssetDataRowArgs.Num(), ParallelForMinBatchSize, [&PopulateAssetDataRowArgs, &AssetsData, this](int32 Index)
		{
			PopulateAssetDataRowArgs[Index] = ThreadSafe_PopulateAssetDataTableRow(MoveTemp(AssetsData[Index]), Database);
		});

	// Index Reserved Asset Data Rows
	TConstArrayView<TypedElementDataStorage::RowHandle> ReservedAssetDataRows(ReservedRows.GetData() + ReservedPopulatePathRows.Num(), AssetsData.Num());
	TArrayView<TPair<TypedElementDataStorage::IndexHash, TypedElementDataStorage::RowHandle>>  IndexesToResevedAssetRows(IndexesToReservedRows.GetData() + ReservedPopulatePathRows.Num(), ReservedAssetDataRows.Num());
	ParallelFor(TEXT("Populating TEDS Asset Registry Asset Data Indexes"), PopulateAssetDataRowArgs.Num(), ParallelForMinBatchSize, [&PopulateAssetDataRowArgs, &ReservedAssetDataRows, &IndexesToResevedAssetRows](int32 Index)
		{
			IndexesToResevedAssetRows[Index] = TPair<TypedElementDataStorage::IndexHash, TypedElementDataStorage::RowHandle>(PopulateAssetDataRowArgs[Index].ObjectPathHash, ReservedAssetDataRows[Index]);
		});
	Database.BatchIndexRows(IndexesToResevedAssetRows);

	// Populate Asset Rows
	Database.BatchAddRow(AssetsDataTable, ReservedAssetDataRows,  [AssetDataRowArgs = MoveTemp(PopulateAssetDataRowArgs), Index = 0, this](TypedElementDataStorage::RowHandle InRowHandle) mutable
		{
			PopulateAssetDataTableRow(MoveTemp(AssetDataRowArgs[Index]), Database, InRowHandle);
			++Index;
		});
}

FTedsAssetData::~FTedsAssetData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::~FTedsAssetData);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

	// Not needed on a editor shut down
	if (!IsEngineExitRequested())
	{
		if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
		{
			Database.UnregisterQuery(ResolveMissingParentPathQuery);
			Database.UnregisterQuery(UpdateParentToChildrenAssetPathQuery);
			Database.UnregisterQuery(ResolveMissingAssetInPathQuery);
			Database.UnregisterQuery(UpdateAssetsInPathQuery);
			Database.UnregisterQuery(RemoveUpdatedAssetDataTagQuery);
			Database.UnregisterQuery(RemoveUpdatedPathTagQuery);


			AssetRegistry->OnAssetsAdded().RemoveAll(this);
			AssetRegistry->OnAssetsRemoved().RemoveAll(this);
			AssetRegistry->OnAssetsUpdated().RemoveAll(this);
			AssetRegistry->OnAssetsUpdatedOnDisk().RemoveAll(this);
			AssetRegistry->OnAssetRenamed().RemoveAll(this);
			AssetRegistry->OnPathsAdded().RemoveAll(this);
			AssetRegistry->OnPathsRemoved().RemoveAll(this);
	
			AssetRegistry->EnumerateAllCachedPaths([this](FName InPath)
				{
					const TypedElementDataStorage::IndexHash PathHash = TypedElementDataStorage::GenerateIndexHash(InPath);
					const TypedElementDataStorage::RowHandle Row = Database.FindIndexedRow(PathHash);
					Database.RemoveRow(Row);
					Database.RemoveIndex(PathHash);
					return true;
				});

			AssetRegistry->EnumerateAllAssets([this](const FAssetData& InAssetData)
				{
					const TypedElementDataStorage::IndexHash AssetPathHash = TypedElementDataStorage::GenerateIndexHash(InAssetData.GetSoftObjectPath());
					const TypedElementDataStorage::RowHandle Row = Database.FindIndexedRow(AssetPathHash);
					Database.RemoveRow(Row);
					Database.RemoveIndex(AssetPathHash);
					return true;
				});
		}
	}
}

void FTedsAssetData::ProcessAllEvents()
{
	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		AssetRegistry->Tick(-1.f);
	}
}

void FTedsAssetData::OnAssetsAdded(TConstArrayView<FAssetData> InAssetsAdded)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::OnAssetsAdded);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

	TArray<int32> Contexts;
	TArray<FPopulateAssetDataRowArgs> PopulateRowArgs;
	PopulateRowArgs.AddDefaulted(InAssetsAdded.Num());

	UE::AssetRegistry::FFiltering::InitializeShouldSkipAsset();

	ParallelForWithTaskContext(TEXT("Populating TEDS Asset Registry Asset Data"), Contexts, InAssetsAdded.Num(), ParallelForMinBatchSize, [&PopulateRowArgs, &InAssetsAdded, this](int32& ValidAssetCount, int32 Index)
		{
			FPopulateAssetDataRowArgs RowArgs;
			const FAssetData& AssetData = InAssetsAdded[Index];

			if (!UE::AssetRegistry::FFiltering::ShouldSkipAsset(AssetData.AssetClassPath, AssetData.PackageFlags))
			{
				RowArgs = ThreadSafe_PopulateAssetDataTableRow(AssetData, Database);
				if (RowArgs.AssetData.IsValid())
				{
					++ValidAssetCount;
				}
			}
			PopulateRowArgs[Index] = MoveTemp(RowArgs);
		});


	int32 NewRowsCount = 0;
	for (int32 Context : Contexts)
	{
		NewRowsCount += Context;
	}

	int32 Index = 0;
	if (NewRowsCount > 0)
	{
		TArray<TPair<TypedElementDataStorage::IndexHash, TypedElementDataStorage::RowHandle>> IndexToRow;
		IndexToRow.Reserve(NewRowsCount);
		Database.BatchAddRow(AssetsDataTable, NewRowsCount, [RowArgs = MoveTemp(PopulateRowArgs), Index = 0, &IndexToRow, this](TypedElementDataStorage::RowHandle InRowHandle) mutable
			{
				FPopulateAssetDataRowArgs ARowArgs = MoveTemp(RowArgs[Index]);
				while (!ARowArgs.AssetData.IsValid())
				{
					++Index;
					ARowArgs = MoveTemp(RowArgs[Index]);
				}

				IndexToRow.Emplace(ARowArgs.ObjectPathHash, InRowHandle);
				PopulateAssetDataTableRow(MoveTemp(ARowArgs), Database, InRowHandle);
				++Index;
			});

		Database.BatchIndexRows(IndexToRow);
	}
}

void FTedsAssetData::OnAssetsRemoved(TConstArrayView<FAssetData> InAssetsRemoved)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::OnAssetsRemoved);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

	for (const FAssetData& Asset : InAssetsRemoved)
	{
		const TypedElementDataStorage::IndexHash AssetHash = TypedElementDataStorage::GenerateIndexHash(Asset.GetSoftObjectPath());
		const TypedElementDataStorage::RowHandle AssetRow = Database.FindIndexedRow(AssetHash);
		if (Database.IsRowAssigned(AssetRow))
		{
			if (const FAssetDataColumn_Experimental* AssetDataColunm = Database.GetColumn<FAssetDataColumn_Experimental>(AssetRow))
			{
				const TypedElementDataStorage::IndexHash FolderPathHash = TypedElementDataStorage::GenerateIndexHash(AssetDataColunm->AssetData.PackagePath);
				const TypedElementDataStorage::RowHandle FolderRow = Database.FindIndexedRow(FolderPathHash);
				if (FAssetsInPathColumn_Experimental* AssetInFolder = Database.GetColumn<FAssetsInPathColumn_Experimental>(FolderRow))
				{
					AssetInFolder->AssetsRow.Remove(AssetRow);
				}
			}
			Database.RemoveRow(Database.FindIndexedRow(AssetHash));
			Database.RemoveIndex(AssetHash);
		}
	}
}

void FTedsAssetData::OnAssetsUpdated(TConstArrayView<FAssetData> InAssetsUpdated)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::OnAssetsUpdated);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

	for (const FAssetData& Asset : InAssetsUpdated)
	{
		const TypedElementDataStorage::IndexHash AssetHash = TypedElementDataStorage::GenerateIndexHash(Asset.GetSoftObjectPath());
		const TypedElementDataStorage::RowHandle Row = Database.FindIndexedRow(AssetHash);
		if (Database.IsRowAssigned(Row))
		{
			Database.GetColumn<FAssetDataColumn_Experimental>(Row)->AssetData = Asset;
			Database.AddColumn<FUpdatedAssetDataTag>(Row);
		}
	}
}

void FTedsAssetData::OnAssetsUpdatedOnDisk(TConstArrayView<FAssetData> InAssetsUpdated)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::OnAssetsUpdatedOnDisk);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

	for (const FAssetData& Asset : InAssetsUpdated)
	{
		const TypedElementDataStorage::IndexHash AssetHash = TypedElementDataStorage::GenerateIndexHash(Asset.GetSoftObjectPath());
		const TypedElementDataStorage::RowHandle Row = Database.FindIndexedRow(AssetHash);
		if (Database.IsRowAssigned(Row))
		{
			Database.GetColumn<FAssetDataColumn_Experimental>(Row)->AssetData = Asset;
			Database.AddColumn<FUpdatedAssetDataTag>(Row);
		}
	}
}

void FTedsAssetData::OnAssetRenamed(const FAssetData& InAsset, const FString& InOldObjectPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::OnAssetRenamed);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

	const TypedElementDataStorage::IndexHash NewAssetHash = TypedElementDataStorage::GenerateIndexHash(InAsset.GetSoftObjectPath());
	const TypedElementDataStorage::IndexHash OldAssetHash = TypedElementDataStorage::GenerateIndexHash(FSoftObjectPath(InOldObjectPath));
	const TypedElementDataStorage::RowHandle Row = Database.FindIndexedRow(OldAssetHash);
	if (Database.IsRowAssigned(Row))
	{
		Database.GetColumn<FAssetDataColumn_Experimental>(Row)->AssetData = InAsset;

		// Update the asset in folder columns
		const TypedElementDataStorage::IndexHash NewFolderHash = TypedElementDataStorage::GenerateIndexHash(InAsset.PackagePath);
		FStringView OldPackagePath(InOldObjectPath);

		{
			int32 CharacterIndex = 0;
			OldPackagePath.FindLastChar(TEXT('/'), CharacterIndex);
			OldPackagePath.LeftInline(CharacterIndex);
		}

		const TypedElementDataStorage::IndexHash OldFolderHash = TypedElementDataStorage::GenerateIndexHash(FName(OldPackagePath));

		if (NewFolderHash != OldFolderHash)
		{
			const TypedElementDataStorage::RowHandle NewPathRow = Database.FindIndexedRow(NewFolderHash);
			if (NewPathRow !=  TypedElementDataStorage::InvalidRowHandle)
			{
				Database.GetColumn<FAssetsInPathColumn_Experimental>(NewPathRow)->AssetsRow.Add(Row);
			}
			else
			{
				FUnresolvedAssetsInPathColumn_Experimental UnresolvedAssetsInPathColumn;
				UnresolvedAssetsInPathColumn.Hash = NewFolderHash;
				Database.AddColumn<FUnresolvedAssetsInPathColumn_Experimental>(Row, MoveTemp(UnresolvedAssetsInPathColumn));
			}

			const TypedElementDataStorage::RowHandle OldPathRow = Database.FindIndexedRow(OldFolderHash);
			if (NewPathRow !=  TypedElementDataStorage::InvalidRowHandle)
			{
				Database.GetColumn<FAssetsInPathColumn_Experimental>(NewPathRow)->AssetsRow.Remove(Row);
			}
			else
			{
				Database.RemoveColumn<FUnresolvedAssetsInPathColumn_Experimental>(Row);
			}
		}

		Database.AddColumn<FUpdatedPathTag>(Row);
		Database.ReindexRow(OldAssetHash, NewAssetHash, Row);
	}
}

void FTedsAssetData::OnPathsAdded(TConstArrayView<FStringView> InPathsAdded)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::OnPathsAdded);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"));
#endif

	TArray<int32> PreparePathRowContexts;
	TArray<FPopulatePathRowArgs> PopulateRowArgs;
	PopulateRowArgs.AddUninitialized(InPathsAdded.Num());

	ParallelForWithTaskContext(TEXT("Populating TEDS Asset Registry Path"), PreparePathRowContexts, InPathsAdded.Num(), ParallelForMinBatchSize, [&PopulateRowArgs, &InPathsAdded, this](int32& WorkerValidCount, int32 Index)
		{
			FStringView Path = InPathsAdded[Index];
			FName PathName(Path);
			const TypedElementDataStorage::IndexHash AssetRegistryPathHash = TypedElementDataStorage::GenerateIndexHash(PathName);
			FPopulatePathRowArgs RowArgs;

			if (Database.FindIndexedRow(AssetRegistryPathHash) != TypedElementDataStorage::InvalidRowHandle)
			{
				RowArgs.MarkAsInvalid();
			}
			else
			{
				RowArgs = ThreadSafe_PopulatePathRowArgs(AssetRegistryPathHash, PathName, Path);
				++WorkerValidCount;
			}

			PopulateRowArgs[Index] = MoveTemp(RowArgs);
		});

	int32 NewRowsCount = 0;
	for (const int32& Context : PreparePathRowContexts)
	{
		NewRowsCount += Context;
	}


	if (NewRowsCount > 0)
	{
		TArray<TypedElementDataStorage::RowHandle> ReservedRow;
		ReservedRow.AddUninitialized(NewRowsCount);
		Database.BatchReserveRows(ReservedRow);

		TArray<TPair<TypedElementDataStorage::IndexHash, TypedElementDataStorage::RowHandle>> IndexesAndRows;
		IndexesAndRows.Reserve(NewRowsCount);

		int32 RowCount = 0;
		for (int32 Index = 0; Index < NewRowsCount; ++Index)
		{
			const FPopulatePathRowArgs* RowArgs = &PopulateRowArgs[RowCount];
			while (!RowArgs)
			{
				++RowCount;
				RowArgs = &PopulateRowArgs[RowCount];
			}

			IndexesAndRows.Emplace(RowArgs->AssetRegistryPathHash, ReservedRow[Index]);
			++RowCount;
		}

		Database.BatchIndexRows(IndexesAndRows);

		int32 Index = 0;
		Database.BatchAddRow(PathsTable, ReservedRow, [&PopulateRowArgs, &Index, this](TypedElementDataStorage::RowHandle InRowHandle)
			{
				FPopulatePathRowArgs RowArgs = MoveTemp(PopulateRowArgs[Index]);
				while (!RowArgs)
				{
					++Index;
					RowArgs = MoveTemp(PopulateRowArgs[Index]);
				}

				PopulatePathDataTableRow(MoveTemp(RowArgs), Database, InRowHandle);
				++Index;
			});
	}
}
 
void FTedsAssetData::OnPathsRemoved(TConstArrayView<FStringView> InPathsRemoved)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTedsAssetData::OnPathsRemoved);

#if TRACK_TEDSASSETDATA_MEMORY
	LLM_SCOPE_BYNAME(TEXT("FTedsAssetData"))
#endif

	for (const FStringView Path : InPathsRemoved)
	{
		const TypedElementDataStorage::IndexHash PathHash = TypedElementDataStorage::GenerateIndexHash(FName(Path));
		Database.RemoveRow(Database.FindIndexedRow(PathHash));
		Database.RemoveIndex(PathHash);
	}
}
}

#undef TRACK_TEDSASSETDATA_MEMORY