// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementsDataStorage.h"

#include "EditorDataStorageSettings.h"
#include "Elements/Common/TypedElementDataStorageLog.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "ISettingsModule.h"
#include "MassEntityTypes.h"
#include "Misc/CoreDelegates.h"
#include "Templates/IsPolymorphic.h"
#include "TypedElementDatabase.h"
#include "TypedElementDatabaseCompatibility.h"
#include "TypedElementDatabaseUI.h"
#include "TypedElementDataStorageSharedColumn.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "FEditorDataStorageModule"

// MASS uses CDO in a few places, making it a difficult to consistently register Type Element's Columns and Tags
// as they may have not been set up to impersonate MASS' Fragments and Tags yet. There are currently no longer
// any cases where TEDS relies on this but may happen again the future. For the standalone version a static
// can be used to initialize the impersonation before the CDO get a chance to run, but for a cooked editor this will
// not work.
/**
 * Typed Elements provides base classes for columns and tags. These directly map to fragments and tags in MASS.
 * To avoid deep and tight coupling between both systems, columns and tags don't directly inhered from MASS, but
 * are otherwise fully compatible. To allow MASS to do its type safety checks, this class updates the type
 * information so Typed Elements columns and tags present as MASS fragments and tags from MASS's perspective.
 */
void ImpersonateMassTagsAndFragments()
{
	// Have UE::Editor::DataStorage::FColumn impersonate a FMassFragment, which is the actual data storage when using MASS as a backend.
	static_assert(sizeof(UE::Editor::DataStorage::FColumn) == sizeof(FMassFragment),
		"In order for UE::Editor::DataStorage::FColumn to impersonate FMassFragment they need to be identical.");
	static_assert(!TIsPolymorphic<FMassFragment>::Value,
		"In order to be able to impersonate FMassFragment it can't have any virtual functions.");
	static_assert(!TIsPolymorphic<UE::Editor::DataStorage::FColumn>::Value,
		"In order to be able to use UE::Editor::DataStorage::FColumn to impersonate FMassFragment it can't have any virtual functions.");
	UE::Editor::DataStorage::FColumn::StaticStruct()->SetSuperStruct(FMassFragment::StaticStruct());

	// Have UE::Editor::DataStorage::FTag impersonate a FMassTag, which is the tag type when using MASS as a backend.
	static_assert(sizeof(UE::Editor::DataStorage::FTag) == sizeof(FMassTag),
		"In order for UE::Editor::DataStorage::FTag to impersonate FMassTag they need to be identical.");
	static_assert(!TIsPolymorphic<FMassTag>::Value,
		"In order to be able to impersonate FMassTag it can't have any virtual functions.");
	static_assert(!TIsPolymorphic<UE::Editor::DataStorage::FTag>::Value,
		"In order to be able to use UE::Editor::DataStorage::FTag to impersonate FMassTag it can't have any virtual functions.");
	UE::Editor::DataStorage::FTag::StaticStruct()->SetSuperStruct(FMassTag::StaticStruct());

	FTedsSharedColumn::StaticStruct()->SetSuperStruct(FMassConstSharedFragment::StaticStruct());
}

void FEditorDataStorageModule::StartupModule()
{
	// Setup the editor settings;
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		// automation
		SettingsModule->RegisterSettings("Editor", "Advanced", "The Editor Data Storage",
			LOCTEXT("DataStorageSettingsName", "The Editor Data Storage"),
			LOCTEXT("DataStorageSettingsDescription", "Configuration options for the central data storage used by various tools to store their data."),
			GetMutableDefault<UEditorDataStorageSettings>()
		);
	}

	// Load the dependent TypedElementFramework module (holding TypedElementRegistry) here so that it is guaranteed to be available in Shutdown
	// and it is shutdown AFTER FEditorDataStorageModule
	FModuleManager::Get().LoadModule(TEXT("TypedElementFramework"));
	
	ImpersonateMassTagsAndFragments();

	FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddLambda(
		[this]()
		{
			if (!bInitialized)
			{
				UE_LOG(LogEditorDataStorage, Log, TEXT("Initializing"));
				
				DataStorage = NewObject<UEditorDataStorage>();
				DataStorage->Initialize();

				DataStorageCompatibility = NewObject<UEditorDataStorageCompatibility>();
				DataStorageCompatibility->Initialize(DataStorage.Get());

				DataStorageUi = NewObject<UEditorDataStorageUi>();
				DataStorageUi->Initialize(DataStorage.Get(), DataStorageCompatibility.Get());

				ObjectReinstancingManager = NewObject<UTedsObjectReinstancingManager>();
				ObjectReinstancingManager->Initialize(*DataStorage, *DataStorageCompatibility);

				// Register the various DataStorage instances.
				UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
				checkf(Registry, TEXT(
					"FEditorDataStorageModule tried to register itself, but there was no Typed Element Registry to register to."));
				Registry->SetDataStorage(DataStorage.Get());
				Registry->SetDataStorageCompatibility(DataStorageCompatibility.Get());
				Registry->SetDataStorageUi(DataStorageUi.Get());

				// Allow any factories to register their content.
				TArray<UClass*> FactoryClasses;
				GetDerivedClasses(UEditorDataStorageFactory::StaticClass(), FactoryClasses);

				DataStorage->SetFactories(FactoryClasses);
				TArray<UEditorDataStorageFactory*> Factories;
				Factories.Reserve(FactoryClasses.Num());

				// First pass to call all registration without dependencies.
				for (UEditorDataStorage::FactoryIterator Iterator = DataStorage->CreateFactoryIterator(); Iterator; ++Iterator)
				{
					UEditorDataStorageFactory* Factory = *Iterator;
					
					Factory->RegisterTables(*DataStorage);
					Factory->RegisterTables(*DataStorage, *DataStorageCompatibility);
					Factory->RegisterTickGroups(*DataStorage);
					Factory->RegisterRegistrationFilters(*DataStorageCompatibility);
					Factory->RegisterDealiaser(*DataStorageCompatibility);
					Factory->RegisterWidgetPurposes(*DataStorageUi);
				}

				// Second pass to call all registration that would benefit or need the registration in the previous pass.
				for (UEditorDataStorage::FactoryIterator Iterator = DataStorage->CreateFactoryIterator(); Iterator; ++Iterator)
				{
					UEditorDataStorageFactory* Factory = *Iterator;
					
					Factory->RegisterQueries(*DataStorage);
					Factory->RegisterWidgetConstructors(*DataStorage, *DataStorageUi);
				}
				
				UE_LOG(LogEditorDataStorage, Log, TEXT("Initialized"));

				bInitialized = true;
			}
		});
	FCoreDelegates::OnExit.AddRaw(this, &FEditorDataStorageModule::ShutdownModule);
}

void FEditorDataStorageModule::ShutdownModule()
{
	if (bInitialized)
	{
		UE_LOG(LogEditorDataStorage, Log, TEXT("Deinitializing"));

		DataStorage->ResetFactories();

		UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
		if (Registry) // If the registry has already been destroyed there's no point in clearing the reference.
		{
			Registry->SetDataStorage(nullptr);
			Registry->SetDataStorageCompatibility(nullptr);
			Registry->SetDataStorageUi(nullptr);
		}

		if (UObjectInitialized())
		{
			ObjectReinstancingManager->Deinitialize();
			DataStorageUi->Deinitialize();
			DataStorageCompatibility->Deinitialize();
			DataStorage->Deinitialize();
		}

		bInitialized = false;
	}
}

void FEditorDataStorageModule::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (bInitialized)
	{
		Collector.AddReferencedObject(DataStorage);
		Collector.AddReferencedObject(DataStorageCompatibility);
		Collector.AddReferencedObject(DataStorageUi);
		Collector.AddReferencedObject(ObjectReinstancingManager);
	}
}

FString FEditorDataStorageModule::GetReferencerName() const
{
	return TEXT("TEDS: Editor Data Storage Core Module");
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FEditorDataStorageModule, TedsCore)