// Copyright Epic Games, Inc. All Rights Reserved.

#include "DynamicColumnGenerator.h"

#include "AssetDefinitionDefault.h"
#include "TypedElementDataStorageSharedColumn.h"
#include "Elements/Common/TypedElementCommonTypes.h"

namespace UE::Editor::DataStorage
{
	FDynamicColumnGeneratorInfo FDynamicColumnGenerator::GenerateColumn(const FName& ColumnName, const UScriptStruct& Template)
	{
		const FGeneratedColumnKey Key
		{
			.Name = ColumnName,
			.Template = Template
		};
	
		FDynamicColumnGeneratorInfo GeneratedColumnInfo;
		UE_MT_SCOPED_WRITE_ACCESS(AccessDetector);
		
		{
			const int32* GeneratedColumnIndex = GenerationParamsLookup.Find(Key);
			if (GeneratedColumnIndex != nullptr)
			{
				const FGeneratedColumnRecord& GeneratedColumnRecord = GeneratedColumnData[*GeneratedColumnIndex];
				GeneratedColumnInfo.Type = GeneratedColumnRecord.Type;
				GeneratedColumnInfo.bNewlyGenerated = false;
				return GeneratedColumnInfo;
			}		
		}
		
		checkf(
			Template.IsChildOf(FTypedElementDataStorageColumn::StaticStruct()) ||
			Template.IsChildOf(FTypedElementDataStorageTag::StaticStruct()) ||
			Template.IsChildOf(FTedsSharedColumn::StaticStruct()),
			TEXT("Template struct must derive from Column, Tag or SharedColumn"));
	
		{
			checkf(Template.GetCppStructOps() != nullptr && Template.IsNative(), TEXT("Can only create column from native struct"));
			
			UScriptStruct* NewScriptStruct = NewObject<UScriptStruct>(GetTransientPackage(), ColumnName);
			NewScriptStruct->AddToRoot();
	
			NewScriptStruct->SetSuperStruct(&const_cast<UScriptStruct&>(Template));
			
			NewScriptStruct->DeferCppStructOps(FTopLevelAssetPath(GetTransientPackage()->GetFName(), ColumnName), Template.GetCppStructOps());
			NewScriptStruct->Bind();
			NewScriptStruct->PrepareCppStructOps();
			NewScriptStruct->StaticLink(true);
	
			
			const int32 Index = GeneratedColumnData.Emplace(FGeneratedColumnRecord
			{
				.Name = ColumnName,
				.Template = &Template,
				.Type = NewScriptStruct
			});
			
			GenerationParamsLookup.Add(Key, Index);
			NameLookup.Add(ColumnName, Index);
			
			GeneratedColumnInfo.Type = NewScriptStruct;
			GeneratedColumnInfo.bNewlyGenerated = true;
			return GeneratedColumnInfo;
		}
	}
	
	const UScriptStruct* FDynamicColumnGenerator::LookupColumn(const FName& ColumnName) const
	{
		UE_MT_SCOPED_READ_ACCESS(AccessDetector);
		
		if (const int32* Index = NameLookup.Find(ColumnName))
		{
			return GeneratedColumnData[*Index].Type;
		}
		return nullptr;
	}
	
	FDynamicTagManager::FDynamicTagManager(FDynamicColumnGenerator& InColumnGenerator)
		: ColumnGenerator(InColumnGenerator)
	{
	}
	
	FConstSharedStruct FDynamicTagManager::GenerateDynamicTag(const FDynamicTag& InTag, const FName& InValue)
	{
		TPair<FDynamicTag, FName> Pair(InTag, InValue);
	
		UE_MT_SCOPED_WRITE_ACCESS(AccessDetector);
	
		// Common path
		{
			if (FConstSharedStruct* TagStruct = DynamicTagLookup.Find(Pair))
			{
				return *TagStruct;
			}
		}
	
		// 
		{
			const UScriptStruct* ColumnType = GenerateColumnType(InTag);
	
			const UE::Editor::DataStorage::FDynamicTagColumn Overlay
			{
				.Value = InValue
			};
	
			FConstSharedStruct SharedStruct = FConstSharedStruct::Make(ColumnType, reinterpret_cast<const uint8*>(&Overlay));
			
			DynamicTagLookup.Emplace(Pair, SharedStruct);
	
			return SharedStruct;
		}
	}
	
	const UScriptStruct* FDynamicTagManager::GenerateColumnType(const FDynamicTag& Tag)
	{
		const FDynamicColumnGeneratorInfo GeneratedColumnType = ColumnGenerator.GenerateColumn(Tag.GetName(), *FDynamicTagColumn::StaticStruct());
		
		return GeneratedColumnType.Type;
	}
} // namespace UE::Editor::DataStorage
