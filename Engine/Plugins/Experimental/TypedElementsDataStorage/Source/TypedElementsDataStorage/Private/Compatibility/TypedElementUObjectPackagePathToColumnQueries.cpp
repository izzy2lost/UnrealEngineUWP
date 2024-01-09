// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compatibility/TypedElementUObjectPackagePathToColumnQueries.h"

#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementPackageColumns.h"
#include "Elements/Framework/TypedElementIndexHasher.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

static void ResolvePackageReference(ITypedElementDataStorageInterface::IQueryContext& Context, const UPackage* Package, TypedElementRowHandle Row, TypedElementRowHandle PackageRow)
{
	FTypedElementPackageReference PackageReference;
	PackageReference.Row = PackageRow;
	Context.AddColumn(Row, MoveTemp(PackageReference));
				
	FTypedElementPackagePathColumn PathColumn;
	FTypedElementPackageLoadedPathColumn LoadedPathColumn;
		
	Package->GetPathName(nullptr, PathColumn.Path);
	LoadedPathColumn.LoadedPath = Package->GetLoadedPath();
				
	Context.AddColumn(PackageRow, MoveTemp(PathColumn));
	Context.AddColumn(PackageRow, MoveTemp(LoadedPathColumn));
};

void UTypedElementUObjectPackagePathFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;
	
	DataStorage.RegisterQuery(
		Select(
			TEXT("Sync UObject package info to columns"),
			FObserver::OnAdd<FTypedElementUObjectColumn>()
				.ForceToGameThread(true),
			[](DSI::IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementUObjectColumn& Object)
			{
				if (const UObject* ObjectInstance = Object.Object.Get(); ObjectInstance != nullptr)
				{
					const UPackage* Target = ObjectInstance->GetPackage();
					FString Path;
					Target->GetPathName(nullptr, Path);
					if (FString PackageFilename; FPackageName::TryConvertLongPackageNameToFilename(Path, PackageFilename))
					{
						FPaths::NormalizeFilename(PackageFilename);
						FString FullPackageFilename = FPaths::ConvertRelativePathToFull(PackageFilename);
						TypedElementDataStorage::IndexHash Index = TypedElementDataStorage::GenerateIndexHash(FullPackageFilename);
						TypedElementRowHandle PackageRow = Context.FindIndexedRow(Index);
						if (Context.IsRowAvailable(PackageRow))
						{
							const UPackage* Package = ObjectInstance->GetPackage();
							ResolvePackageReference(Context, Package, Row, PackageRow);
						}
						else
						{
							FTypedElementPackageUnresolvedReference UnresolvedPackageReference;
                            UnresolvedPackageReference.Index = Index;
							UnresolvedPackageReference.PathOnDisk = MoveTemp(FullPackageFilename);
                            Context.AddColumn(Row, MoveTemp(UnresolvedPackageReference));
						}
					}
				}
			}
		)
		.Compile()
	);

	DataStorage.RegisterQuery(
		Select(
			TEXT("Resolve package references"),
			FProcessor(DSI::EQueryTickPhase::FrameEnd, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncExternalToDataStorage)),
			[](DSI::IQueryContext& Context, TypedElementRowHandle Row, const FTypedElementUObjectColumn& Object, const FTypedElementPackageUnresolvedReference& UnresolvedPackageReference)
			{
				TypedElementRowHandle PackageRow = Context.FindIndexedRow(UnresolvedPackageReference.Index);
				if (!Context.IsRowAvailable(PackageRow))
				{
					return;
				}
				const UObject* ObjectInstance = Object.Object.Get();
				if (!ObjectInstance)
				{
					return;
				}
				const UPackage* Package = ObjectInstance->GetPackage();
				Context.RemoveColumns(Row, { FTypedElementPackageUnresolvedReference::StaticStruct() });

				ResolvePackageReference(Context, Package, Row, PackageRow);
			}
		)
		.Compile()
	);
}
