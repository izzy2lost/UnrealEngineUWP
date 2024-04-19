// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/ImportExportCollector.h"

#if WITH_EDITORONLY_DATA

FImportExportCollector::FImportExportCollector(UPackage* InRootPackage)
	: RootPackage(InRootPackage)
	, RootPackageName(InRootPackage->GetFName())
{
	ArIsObjectReferenceCollector = true;
	ArIsModifyingWeakAndStrongReferences = true;
	SetIsSaving(true);
	SetIsPersistent(true);
}

void FImportExportCollector::Reset()
{
	Exports.Reset();
	Imports.Reset();
}

void FImportExportCollector::AddExportToIgnore(UObject* Export)
{
	Exports.Add(Export);
}

void FImportExportCollector::SerializeObjectAndReferencedExports(UObject* RootObject)
{
	*this << RootObject;
	while (!ExportsExploreQueue.IsEmpty())
	{
		UObject* Export = ExportsExploreQueue.PopFrontValue();
		Export->Serialize(*this);
	}
}

FArchive& FImportExportCollector::operator<<(UObject*& Obj)
{
	if (!Obj)
	{
		return *this;
	}
	UPackage* Package = Obj->GetPackage();
	if (!Package)
	{
		return *this;
	}
	if (Package != RootPackage)
	{
		AddImport(FSoftObjectPath(Obj), ESoftObjectPathCollectType::AlwaysCollect);
		return *this;
	}

	bool bAlreadyExists;
	Exports.Add(Obj, &bAlreadyExists);
	if (bAlreadyExists)
	{
		return *this;
	}
	ExportsExploreQueue.Add(Obj);
	return *this;
}

FArchive& FImportExportCollector::operator<<(FSoftObjectPath& Value)
{
	FName CurrentPackage;
	FName PropertyName;
	ESoftObjectPathCollectType CollectType;
	ESoftObjectPathSerializeType SerializeType;
	FSoftObjectPathThreadContext& ThreadContext = FSoftObjectPathThreadContext::Get();
	ThreadContext.GetSerializationOptions(CurrentPackage, PropertyName, CollectType, SerializeType, this);

	if (CollectType != ESoftObjectPathCollectType::NeverCollect && CollectType != ESoftObjectPathCollectType::NonPackage)
	{
		FName PackageName = Value.GetLongPackageFName();
		if (PackageName != RootPackageName && !PackageName.IsNone())
		{
			AddImport(Value, CollectType);
		}
	}
	return *this;
}

void FImportExportCollector::AddImport(const FSoftObjectPath& Path, ESoftObjectPathCollectType CollectType)
{
	ESoftObjectPathCollectType& ExistingImport = Imports.FindOrAdd(
		Path, ESoftObjectPathCollectType::EditorOnlyCollect);
	ExistingImport = Union(ExistingImport, CollectType);

	ESoftObjectPathCollectType& ExistingPackage = ImportedPackages.FindOrAdd(
		Path.GetLongPackageFName(), ESoftObjectPathCollectType::EditorOnlyCollect);
	ExistingPackage = Union(ExistingPackage, CollectType);
}

ESoftObjectPathCollectType FImportExportCollector::Union(ESoftObjectPathCollectType A, ESoftObjectPathCollectType B)
{
	return static_cast<ESoftObjectPathCollectType>(FMath::Max(static_cast<int>(A), static_cast<int>(B)));
}

#endif // WITH_EDITORONLY_DATA
