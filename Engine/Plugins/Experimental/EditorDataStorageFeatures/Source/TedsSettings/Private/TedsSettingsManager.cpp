// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsSettingsManager.h"

#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Framework/TypedElementIndexHasher.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "ISettingsCategory.h"
#include "ISettingsContainer.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "TedsSettingsColumns.h"
#include "TedsSettingsLog.h"
#include "UObject/UnrealType.h"

FTedsSettingsManager::FTedsSettingsManager()
	: bIsInitialized{ false }
	, SelectAllSettingsQuery{ UE::Editor::DataStorage::InvalidQueryHandle }
	, SettingsContainerTable{ UE::Editor::DataStorage::InvalidTableHandle }
	, SettingsCategoryTable{ UE::Editor::DataStorage::InvalidTableHandle }
{
}

void FTedsSettingsManager::Initialize()
{
	if (!bIsInitialized)
	{
		FModuleManager::Get().LoadModule(TEXT("TypedElementFramework"));

		UTypedElementRegistry* TypedElementRegistry = UTypedElementRegistry::GetInstance();
		check(TypedElementRegistry);

		auto OnDataStorage = [this, TypedElementRegistry]
			{
				ITypedElementDataStorageInterface* DataStorage = TypedElementRegistry->GetMutableDataStorage();
				check(DataStorage);

				RegisterTables(*DataStorage);
				RegisterQueries(*DataStorage);
				RegisterSettings();
			};

		if (TypedElementRegistry->AreDataStorageInterfacesSet())
		{
			OnDataStorage();
		}
		else
		{
			TypedElementRegistry->OnDataStorageInterfacesSet().AddSPLambda(this, OnDataStorage);
		}

		bIsInitialized = true;
	}
}

void FTedsSettingsManager::Shutdown()
{
	if (bIsInitialized)
	{
		UTypedElementRegistry* TypedElementRegistry = UTypedElementRegistry::GetInstance();
		check(TypedElementRegistry);

		TypedElementRegistry->OnDataStorageInterfacesSet().RemoveAll(this);

		if (TypedElementRegistry->AreDataStorageInterfacesSet())
		{
			ITypedElementDataStorageInterface* DataStorage = TypedElementRegistry->GetMutableDataStorage();
			check(DataStorage);

			UnregisterSettings();
			UnregisterQueries(*DataStorage);
		}

		bIsInitialized = false;
	}
}

void FTedsSettingsManager::RegisterTables(ITypedElementDataStorageInterface& DataStorage)
{
	if (SettingsContainerTable == UE::Editor::DataStorage::InvalidTableHandle)
	{
		SettingsContainerTable = DataStorage.RegisterTable(
			TTypedElementColumnTypeList<FNameColumn, FDisplayNameColumn, FDescriptionColumn, FSettingsContainerTag>(),
			FName(TEXT("Editor_SettingsContainerTable")));
	}

	if (SettingsCategoryTable == UE::Editor::DataStorage::InvalidTableHandle)
	{
		SettingsCategoryTable = DataStorage.RegisterTable(
			TTypedElementColumnTypeList<FSettingsContainerReferenceColumn, FNameColumn, FDisplayNameColumn, FDescriptionColumn, FSettingsCategoryTag>(),
			FName(TEXT("Editor_SettingsCategoryTable")));
	}
}

void FTedsSettingsManager::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace UE::Editor::DataStorage::Queries;

	if (SelectAllSettingsQuery == UE::Editor::DataStorage::InvalidQueryHandle)
	{
		SelectAllSettingsQuery = DataStorage.RegisterQuery(
			Select()
				.ReadOnly<FSettingsContainerReferenceColumn, FSettingsCategoryReferenceColumn, FNameColumn>()
			.Where()
				.All<FSettingsSectionTag>()
			.Compile());
	}
}

void FTedsSettingsManager::UnregisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	DataStorage.UnregisterQuery(SelectAllSettingsQuery);
	SelectAllSettingsQuery = UE::Editor::DataStorage::InvalidQueryHandle;
}

void FTedsSettingsManager::RegisterSettings()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TedsSettingsManager.RegisterSettings);

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	check(SettingsModule);

	TArray<FName> ContainerNames;
	SettingsModule->GetContainerNames(ContainerNames);

	for (FName ContainerName : ContainerNames)
	{
		RegisterSettingsContainer(ContainerName);
	}

	SettingsModule->OnContainerAdded().AddSP(this, &FTedsSettingsManager::RegisterSettingsContainer);
}

void FTedsSettingsManager::RegisterSettingsContainer(const FName& ContainerName)
{
	using namespace UE::Editor::DataStorage;

	TRACE_CPUPROFILER_EVENT_SCOPE(TedsSettingsManager.RegisterSettingsContainer);

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	check(SettingsModule);

	UTypedElementRegistry* TypedElementRegistry = UTypedElementRegistry::GetInstance();
	check(TypedElementRegistry);

	ITypedElementDataStorageInterface* DataStorage = TypedElementRegistry->GetMutableDataStorage();
	check(DataStorage);

	UE_LOG(LogTedsSettings, Log, TEXT("Register Settings Container : '%s'"), *ContainerName.ToString());

	ISettingsContainerPtr ContainerPtr = SettingsModule->GetContainer(ContainerName);

	uint64 ContainerIndexHash = GenerateIndexHash(ContainerPtr.Get());
	RowHandle ContainerRow = DataStorage->FindIndexedRow(ContainerIndexHash);
	if (ContainerRow == InvalidRowHandle)
	{
		ContainerRow = DataStorage->AddRow(SettingsContainerTable);
		DataStorage->AddColumn<FNameColumn>(ContainerRow, { .Name = ContainerName });
		DataStorage->AddColumn<FDisplayNameColumn>(ContainerRow, { .DisplayName = ContainerPtr->GetDisplayName() });
		DataStorage->AddColumn<FDescriptionColumn>(ContainerRow, { .Description = ContainerPtr->GetDescription() });
		DataStorage->AddColumn<FSettingsContainerTag>(ContainerRow);

		DataStorage->IndexRow(ContainerIndexHash, ContainerRow);
	}

	TArray<ISettingsCategoryPtr> Categories;
	ContainerPtr->GetCategories(Categories);

	for (ISettingsCategoryPtr CategoryPtr : Categories)
	{
		const bool bQueryExistingRows = false;
		UpdateSettingsCategory(CategoryPtr, ContainerRow, bQueryExistingRows);
	}

	// OnCategoryModified is called at the same time as OnSectionRemoved so we only bind to OnCategoryModified for add / update / remove
	ContainerPtr->OnCategoryModified().AddSPLambda(this, [this, ContainerPtr, ContainerRow](const FName& ModifiedCategoryName)
		{
			UE_LOG(LogTedsSettings, Log, TEXT("Settings Category modified : '%s->%s'"), *ContainerPtr->GetName().ToString(), *ModifiedCategoryName.ToString());

			ISettingsCategoryPtr CategoryPtr = ContainerPtr->GetCategory(ModifiedCategoryName);

			UpdateSettingsCategory(CategoryPtr, ContainerRow);
		});
}

void FTedsSettingsManager::UnregisterSettings()
{
	using namespace UE::Editor::DataStorage;

	TRACE_CPUPROFILER_EVENT_SCOPE(TedsSettingsManager.UnregisterSettings);

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	check(SettingsModule);

	UTypedElementRegistry* TypedElementRegistry = UTypedElementRegistry::GetInstance();
	check(TypedElementRegistry);

	ITypedElementDataStorageInterface* DataStorage = TypedElementRegistry->GetMutableDataStorage();
	check(DataStorage);

	ITypedElementDataStorageCompatibilityInterface* DataStorageCompatibility = TypedElementRegistry->GetMutableDataStorageCompatibility();
	check(DataStorageCompatibility);

	SettingsModule->OnContainerAdded().RemoveAll(this);

	TArray<FName> ContainerNames;
	SettingsModule->GetContainerNames(ContainerNames);

	for (FName ContainerName : ContainerNames)
	{
		UE_LOG(LogTedsSettings, Log, TEXT("Unregister Settings Container : '%s'"), *ContainerName.ToString());

		ISettingsContainerPtr ContainerPtr = SettingsModule->GetContainer(ContainerName);

		ContainerPtr->OnCategoryModified().RemoveAll(this);

		TArray<ISettingsCategoryPtr> Categories;
		ContainerPtr->GetCategories(Categories);

		for (ISettingsCategoryPtr CategoryPtr : Categories)
		{
			UE_LOG(LogTedsSettings, Log, TEXT("Unregister Settings Category : '%s'"), *CategoryPtr->GetName().ToString());

			TArray<ISettingsSectionPtr> Sections;
			const bool bIgnoreVisibility = true;
			CategoryPtr->GetSections(Sections, bIgnoreVisibility);

			for (ISettingsSectionPtr SectionPtr : Sections)
			{
				if (TStrongObjectPtr<UObject> SettingsObjectPtr = SectionPtr->GetSettingsObject().Pin(); SettingsObjectPtr)
				{
					DataStorageCompatibility->RemoveCompatibleObject(SettingsObjectPtr);

					UE_LOG(LogTedsSettings, Log, TEXT("Removed Settings Section : '%s'"), *SectionPtr->GetName().ToString());
				}
			}

			uint64 CategoryIndexHash = GenerateIndexHash(CategoryPtr.Get());
			RowHandle CategoryRow = DataStorage->FindIndexedRow(CategoryIndexHash);
			if (CategoryRow != InvalidRowHandle)
			{
				DataStorage->RemoveRow(CategoryRow);
				DataStorage->RemoveIndex(CategoryIndexHash);
			}
		}

		uint64 ContainerIndexHash = GenerateIndexHash(ContainerPtr.Get());
		RowHandle ContainerRow = DataStorage->FindIndexedRow(ContainerIndexHash);
		if (ContainerRow != InvalidRowHandle)
		{
			DataStorage->RemoveRow(ContainerRow);
			DataStorage->RemoveIndex(ContainerIndexHash);
		}
	}
}

void FTedsSettingsManager::UpdateSettingsCategory(TSharedPtr<ISettingsCategory> SettingsCategory, UE::Editor::DataStorage::RowHandle ContainerRow, const bool bQueryExistingRows)
{
	using namespace UE::Editor::DataStorage;

	TRACE_CPUPROFILER_EVENT_SCOPE(TedsSettingsManager.UpdateSettingsCategory);

	UTypedElementRegistry* TypedElementRegistry = UTypedElementRegistry::GetInstance();
	check(TypedElementRegistry);

	ITypedElementDataStorageInterface* DataStorage = TypedElementRegistry->GetMutableDataStorage();
	check(DataStorage);

	ITypedElementDataStorageCompatibilityInterface* DataStorageCompatibility = TypedElementRegistry->GetMutableDataStorageCompatibility();
	check(DataStorageCompatibility);

	const FName& ContainerName = DataStorage->GetColumn<FNameColumn>(ContainerRow)->Name;
	const FName& CategoryName = SettingsCategory->GetName();

	UE_LOG(LogTedsSettings, Log, TEXT("Update Settings Category: '%s->%s'"), *ContainerName.ToString(), *CategoryName.ToString());

	uint64 CategoryIndexHash = GenerateIndexHash(SettingsCategory.Get());

	RowHandle CategoryRow = DataStorage->FindIndexedRow(CategoryIndexHash);
	if (CategoryRow == InvalidRowHandle)
	{
		CategoryRow = DataStorage->AddRow(SettingsCategoryTable);

		DataStorage->AddColumn<FSettingsContainerReferenceColumn>(CategoryRow, { .ContainerName = ContainerName, .ContainerRow = ContainerRow });
		DataStorage->AddColumn<FNameColumn>(CategoryRow, { .Name = CategoryName });
		DataStorage->AddColumn<FDisplayNameColumn>(CategoryRow, { .DisplayName = SettingsCategory->GetDisplayName() });
		DataStorage->AddColumn<FDescriptionColumn>(CategoryRow, { .Description = SettingsCategory->GetDescription() });
		DataStorage->AddColumn<FSettingsCategoryTag>(CategoryRow);

		DataStorage->IndexRow(CategoryIndexHash, CategoryRow);
	}

	TArray<RowHandle> OldRowHandles;
	TArray<FName> OldSectionNames;

	// Gather all existing rows for the given { ContainerName, CategoryName } pair.
	if (bQueryExistingRows)
	{
		using namespace UE::Editor::DataStorage::Queries;

		DataStorage->RunQuery(SelectAllSettingsQuery, CreateDirectQueryCallbackBinding(
			[&OldRowHandles, &OldSectionNames, &ContainerName, &CategoryName](
				IDirectQueryContext& Context,
				const FSettingsContainerReferenceColumn* ContainerColumns,
				const FSettingsCategoryReferenceColumn* CategoryColumns,
				const FNameColumn* SectionNameColumns)
			{
				const uint32 RowCount = Context.GetRowCount();

				for (uint32 RowIndex = 0; RowIndex < RowCount; ++RowIndex)
				{
					const FName& TempContainerName = ContainerColumns[RowIndex].ContainerName;
					const FName& TempCategoryName = CategoryColumns[RowIndex].CategoryName;
					if (TempContainerName == ContainerName &&
						TempCategoryName == CategoryName)
					{
						OldRowHandles.Emplace(Context.GetRowHandles()[RowIndex]);
						OldSectionNames.Emplace(SectionNameColumns[RowIndex].Name);
					}
				}
			}));
	}

	TArray<FName> NewSectionNames;
	TArray<ISettingsSectionPtr> NewSections;

	const bool bIgnoreVisibility = true;
	SettingsCategory->GetSections(NewSections, bIgnoreVisibility);

	// Iterate the category and add rows for any section not in the old sections list.
	for (ISettingsSectionPtr SectionPtr : NewSections)
	{
		const FName& SectionName = SectionPtr->GetName();

		if (TStrongObjectPtr<UObject> SettingsObjectPtr = SectionPtr->GetSettingsObject().Pin(); SettingsObjectPtr)
		{
			NewSectionNames.Emplace(SectionName);

			if (OldSectionNames.Contains(SectionName))
			{
				UE_LOG(LogTedsSettings, Verbose, TEXT("Settings Section : '%s' is already in data storage"), *SectionName.ToString());

				continue;
			}

			RowHandle NewRow = DataStorageCompatibility->AddCompatibleObject(SettingsObjectPtr);

			DataStorage->AddColumn<FSettingsContainerReferenceColumn>(NewRow, { .ContainerName = ContainerName, .ContainerRow = ContainerRow });
			DataStorage->AddColumn<FSettingsCategoryReferenceColumn>(NewRow, { .CategoryName = CategoryName, .CategoryRow = CategoryRow });
			DataStorage->AddColumn<FNameColumn>(NewRow, { .Name = SectionName });
			DataStorage->AddColumn<FDisplayNameColumn>(NewRow, { .DisplayName = SectionPtr->GetDisplayName() });
			DataStorage->AddColumn<FDescriptionColumn>(NewRow, { .Description = SectionPtr->GetDescription() });
			DataStorage->AddColumn<FSettingsSectionTag>(NewRow);

			UE_LOG(LogTedsSettings, Log, TEXT("Added Settings Section : '%s'"), *SectionName.ToString());
		}
	}

	// Iterate the old sections and remove rows not in the new sections list.
	for (int32 RowIndex = 0; RowIndex < OldSectionNames.Num(); ++RowIndex)
	{
		const FName& OldSectionName = OldSectionNames[RowIndex];

		if (NewSectionNames.Contains(OldSectionName))
		{
			continue;
		}

		RowHandle OldRowHandle = OldRowHandles[RowIndex];
		check(OldRowHandle != InvalidRowHandle);

		DataStorage->RemoveRow(OldRowHandle);

		UE_LOG(LogTedsSettings, Log, TEXT("Removed Settings Section : '%s'"), *OldSectionName.ToString());
	}
}
