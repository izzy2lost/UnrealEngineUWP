// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Columns/TypedElementPackageColumns.h"
#include "Elements/Columns/TypedElementTransformColumns.h"
#include "Memento/TypedElementMementoTranslators.h"
#include "TypedElementMementoExamples.generated.h"

/**
 * Default LocalTransform MementoTranslator
 * Implementation will generate a memento structure to put into TEDS at runtime based on UPROPERTY declarations
 */
UCLASS()
class UTypedElementLocalTransformColumnMementoTranslator final : public UTypedElementDefaultMementoTranslator
{
	GENERATED_BODY()
public:
	virtual const UScriptStruct* GetColumnType() const override { return FTypedElementLocalTransformColumn::StaticStruct();	}
};

/**
 * Explicit memento for PackagePath
 */
USTRUCT()
struct FTypedElementPackagePathColumnMemento final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
	
	UPROPERTY()
	FName Path;
};

/**
 * Explicit PackagePath MementoTranslator
 */
UCLASS()
class UTypedElementPackagePathMementoTranslator final : public UTypedElementMementoTranslatorBase
{
	GENERATED_BODY()
public:
	virtual const UScriptStruct* GetColumnType() const override { return FTypedElementPackagePathColumn::StaticStruct(); }
	virtual const UScriptStruct* GetMementoType() const override { return FTypedElementPackagePathColumnMemento::StaticStruct(); }

private:
	virtual void TranslateColumnToMemento(const void* TypedErasedColumn, void* TypeErasedMemento) const override
	{
		// Boilerplate to get the types back
		const FTypedElementPackagePathColumn* Column = static_cast<const FTypedElementPackagePathColumn*>(TypedErasedColumn);
		FTypedElementPackagePathColumnMemento* Memento = static_cast<FTypedElementPackagePathColumnMemento*>(TypeErasedMemento);

		// Explicit mapping between the FString 
		Memento->Path = FName(Column->Path);
	}
};