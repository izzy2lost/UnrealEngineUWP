// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsSettingsManager.h"

#include "Elements/Columns/TypedElementCompatibilityColumns.h"
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
	, SelectAllSettingsQuery{ TypedElementDataStorage::InvalidQueryHandle }
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

void FTedsSettingsManager::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;

	if (SelectAllSettingsQuery == TypedElementDataStorage::InvalidQueryHandle)
	{
		SelectAllSettingsQuery = DataStorage.RegisterQuery(
			Select()
			.ReadOnly<FTypedElementUObjectColumn, FSettingsContainerColumn, FSettingsCategoryColumn, FSettingsSectionColumn>()
			.Compile());
	}
}

void FTedsSettingsManager::UnregisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	DataStorage.UnregisterQuery(SelectAllSettingsQuery);
	SelectAllSettingsQuery = TypedElementDataStorage::InvalidQueryHandle;
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
	TRACE_CPUPROFILER_EVENT_SCOPE(TedsSettingsManager.RegisterSettingsContainer);

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	check(SettingsModule);

	UE_LOG(LogTedsSettings, Log, TEXT("Register Settings Container : '%s'"), *ContainerName.ToString());

	ISettingsContainerPtr ContainerPtr = SettingsModule->GetContainer(ContainerName);

	TArray<ISettingsCategoryPtr> Categories;
	ContainerPtr->GetCategories(Categories);

	for (ISettingsCategoryPtr CategoryPtr : Categories)
	{
		const bool bQueryExistingRows = false;
		UpdateSettingsCategory(CategoryPtr, ContainerName, bQueryExistingRows);
	}

	// OnCategoryModified is called at the same time as OnSectionRemoved so we only bind to OnCategoryModified for add / update / remove
	ContainerPtr->OnCategoryModified().AddSPLambda(this, [this, ContainerPtr](const FName& ModifiedCategoryName)
		{
			UE_LOG(LogTedsSettings, Log, TEXT("Settings Category modified : '%s->%s'"), *ContainerPtr->GetName().ToString(), *ModifiedCategoryName.ToString());

			ISettingsCategoryPtr CategoryPtr = ContainerPtr->GetCategory(ModifiedCategoryName);

			UpdateSettingsCategory(CategoryPtr, ContainerPtr->GetName());
		});
}

void FTedsSettingsManager::UnregisterSettings()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TedsSettingsManager.UnregisterSettings);

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	check(SettingsModule);

	UTypedElementRegistry* TypedElementRegistry = UTypedElementRegistry::GetInstance();
	check(TypedElementRegistry);

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
		}
	}
}

void FTedsSettingsManager::UpdateSettingsCategory(TSharedPtr<ISettingsCategory> SettingsCategory, const FName& ContainerName, const bool bQueryExistingRows)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TedsSettingsManager.UpdateSettingsCategory);

	const FName& CategoryName = SettingsCategory->GetName();

	UE_LOG(LogTedsSettings, Log, TEXT("Update Settings Category: '%s->%s'"), *ContainerName.ToString(), *CategoryName.ToString());

	UTypedElementRegistry* TypedElementRegistry = UTypedElementRegistry::GetInstance();
	check(TypedElementRegistry);

	ITypedElementDataStorageInterface* DataStorage = TypedElementRegistry->GetMutableDataStorage();
	check(DataStorage);

	ITypedElementDataStorageCompatibilityInterface* DataStorageCompatibility = TypedElementRegistry->GetMutableDataStorageCompatibility();
	check(DataStorageCompatibility);

	TArray<TypedElementDataStorage::RowHandle> OldRowHandles;
	TArray<FName> OldSectionNames;

	// Gather all existing rows for the given { ContainerName, CategoryName } pair.
	if (bQueryExistingRows)
	{
		using namespace TypedElementQueryBuilder;
		using DSI = ITypedElementDataStorageInterface;

		DataStorage->RunQuery(SelectAllSettingsQuery, CreateDirectQueryCallbackBinding(
			[&OldRowHandles, &OldSectionNames, &ContainerName, &CategoryName](
				DSI::IDirectQueryContext& Context,
				const FTypedElementUObjectColumn* ObjectColumns,
				const FSettingsContainerColumn* ContainerColumns,
				const FSettingsCategoryColumn* CategoryColumns,
				const FSettingsSectionColumn* SectionColumns)
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
						OldSectionNames.Emplace(SectionColumns[RowIndex].SectionName);
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

			TypedElementDataStorage::RowHandle NewRow = DataStorageCompatibility->AddCompatibleObject(SettingsObjectPtr);

			DataStorage->AddColumn<FSettingsContainerColumn>(NewRow, { .ContainerName = ContainerName });
			DataStorage->AddColumn<FSettingsCategoryColumn>(NewRow, { .CategoryName = CategoryName });
			DataStorage->AddColumn<FSettingsSectionColumn>(NewRow, { .SectionName = SectionName });

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

		TypedElementDataStorage::RowHandle OldRowHandle = OldRowHandles[RowIndex];
		check(OldRowHandle != TypedElementInvalidRowHandle);

		DataStorage->RemoveRow(OldRowHandle);

		UE_LOG(LogTedsSettings, Log, TEXT("Removed Settings Section : '%s'"), *OldSectionName.ToString());
	}
}
