// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TypedElementDataStorageSharedColumn.h"
#include "Containers/Map.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Misc/MTAccessDetector.h"
#include "StructUtils/PropertyBag.h"
#include "UObject/NameTypes.h"
#include "UObject/Class.h"

#include "DynamicColumnGenerator.generated.h"

class FName;
class UScriptStruct;

// The template struct that is used to generate the DynamicTag column
// It is safe to reinterpret a DynamicTag column to this template to access the Value
USTRUCT()
struct FTedsDynamicTagColumn : public FTedsSharedColumn
{
	GENERATED_BODY()
	
	UPROPERTY()
	FName Value;
};

namespace UE::Editor::DataStorage
{
	using FDynamicTagColumn = FTedsDynamicTagColumn;

	struct FDynamicColumnInfo
	{
		const UScriptStruct* Type;
	};
	
	struct FDynamicColumnGeneratorInfo
	{
		const UScriptStruct* Type;
		bool bNewlyGenerated;
	};

	/**
	 * Utility class that TEDS can use to dynamically generate column types on the fly
	 */
	class FDynamicColumnGenerator
	{
	public:
		/**
		 * Generates a dynamic TEDS column type based on a Template type (if it hasn't been generated before)
		 */
		FDynamicColumnGeneratorInfo GenerateColumn(const FName& ColumnName, const UScriptStruct& Template);

		/**
		 * Looks up a column based on the name given 
		 */
		const UScriptStruct* LookupColumn(const FName& ColumnName) const;
	private:

		struct FGeneratedColumnRecord
		{
			FName Name;
			const UScriptStruct* Template;
			const UScriptStruct* Type;
		};

		UE_MT_DECLARE_RW_ACCESS_DETECTOR(AccessDetector);

		TArray<FGeneratedColumnRecord> GeneratedColumnData;

		struct FGeneratedColumnKey
		{
			FName Name;
			const UScriptStruct& Template;

			friend bool operator==(const FGeneratedColumnKey& Lhs, const FGeneratedColumnKey& Rhs)
			{
				return Lhs.Name == Rhs.Name && &Lhs.Template == &Rhs.Template;
			}

			friend uint32 GetTypeHash(const FGeneratedColumnKey& Key)
			{
				return HashCombineFast(GetTypeHash(Key.Name), PointerHash(&Key.Template));
			}
		};
		// Looks up generated column index by the parameters used to generate it
		// Used to de-duplicate
		TMap<FGeneratedColumnKey, int32> GenerationParamsLookup;
		// Looks up generated column index by name
		TMap<FName, int32> NameLookup;
	};

	class FDynamicTagManager
	{
	public:
		struct FDynamicTagStructLayout
		{
			FName Tag;
		};
		
		explicit FDynamicTagManager(FDynamicColumnGenerator& InColumnGenerator);
		FConstSharedStruct GenerateDynamicTag(const FDynamicTag& InTag, const FName& InValue);
		const UScriptStruct* GenerateColumnType(const FDynamicTag& InTag);
	private:

		UE_MT_DECLARE_RW_ACCESS_DETECTOR(AccessDetector);

		TMap<TPair<FDynamicTag, FName>, FConstSharedStruct> DynamicTagLookup;

		FDynamicColumnGenerator& ColumnGenerator;
	};
} // namespace UE::Editor::DataStorage
