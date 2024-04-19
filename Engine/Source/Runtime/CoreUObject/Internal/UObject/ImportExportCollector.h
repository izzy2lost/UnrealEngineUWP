// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#if WITH_EDITORONLY_DATA
#include "Containers/Map.h"
#include "Containers/RingBuffer.h"
#include "Containers/Set.h"
#include "Serialization/ArchiveUObject.h"
#include "UObject/Package.h"
#include "UObject/NameTypes.h"
#include "UObject/SoftObjectPath.h"

/** An Archive that records all of the imported packages from a tree of exports. */
class FImportExportCollector : public FArchiveUObject
{
public:
	COREUOBJECT_API explicit FImportExportCollector(UPackage* InRootPackage);

	/**
	 * Mark that a given export (e.g. the export that is doing the collecting) should not be explored
	 * if encountered again. Prevents infinite recursion when the collector is constructed and called during
	 * Serialize.
	 */
	COREUOBJECT_API void AddExportToIgnore(UObject* Export);
	/**
	 * Serialize the given object, following its object references to find other imports and exports,
	 * and recursively serialize any new exports that it references.
	 */
	COREUOBJECT_API void SerializeObjectAndReferencedExports(UObject* RootObject);
	/** Restore the collector to empty. */
	COREUOBJECT_API void Reset();
	const TSet<UObject*>& GetExports() const;
	const TMap<FSoftObjectPath, ESoftObjectPathCollectType>& GetImports() const;
	const TMap<FName, ESoftObjectPathCollectType>& GetImportedPackages() const;

	COREUOBJECT_API virtual FArchive& operator<<(UObject*& Obj) override;
	COREUOBJECT_API virtual FArchive& operator<<(FSoftObjectPath& Value) override;

private:
	void AddImport(const FSoftObjectPath& Path, ESoftObjectPathCollectType CollectType);
	ESoftObjectPathCollectType Union(ESoftObjectPathCollectType A, ESoftObjectPathCollectType B);

	TSet<UObject*> Exports;
	TRingBuffer<UObject*> ExportsExploreQueue;
	TMap<FSoftObjectPath, ESoftObjectPathCollectType> Imports;
	TMap<FName, ESoftObjectPathCollectType> ImportedPackages;
	UPackage* RootPackage;
	FName RootPackageName;
};

///////////////////////////////////////////////////////
// Inline implementations
///////////////////////////////////////////////////////

inline const TSet<UObject*>& FImportExportCollector::GetExports() const
{
	return Exports;
}

inline const TMap<FSoftObjectPath, ESoftObjectPathCollectType>& FImportExportCollector::GetImports() const
{
	return Imports;
}

inline const TMap<FName, ESoftObjectPathCollectType>& FImportExportCollector::GetImportedPackages() const
{
	return ImportedPackages;
}

#endif // WITH_EDITORONLY_DATA
