// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementSubsystems.h"

#include "Elements/Framework/TypedElementRegistry.h"

//
// UEditorDataStorageSubsystem
//

UEditorDataStorageSubsystem::~UEditorDataStorageSubsystem()
{
	DataStorage = nullptr;
}

IEditorDataStorageProvider* UEditorDataStorageSubsystem::Get()
{
	if (!DataStorage)
	{
		UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
		checkf(Registry, TEXT("UEditorDataStorageSubsystem created before the Typed Elements Registry is available."));
		DataStorage = Registry->GetMutableDataStorage();
	}
	return DataStorage;
}

const IEditorDataStorageProvider* UEditorDataStorageSubsystem::Get() const
{
	return const_cast<UEditorDataStorageSubsystem*>(this)->Get();
}


//
// UEditorDataStorageUiSubsystem
//

UEditorDataStorageUiSubsystem::~UEditorDataStorageUiSubsystem()
{
	DataStorageUi = nullptr;
}

IEditorDataStorageUiProvider* UEditorDataStorageUiSubsystem::Get()
{
	if (!DataStorageUi)
	{
		UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
		checkf(Registry, TEXT("UEditorDataStorageUiSubsystem created before the Typed Elements Registry is available."));
		DataStorageUi = Registry->GetMutableDataStorageUi();
	}
	return DataStorageUi;
}

const IEditorDataStorageUiProvider* UEditorDataStorageUiSubsystem::Get() const
{
	return const_cast<UEditorDataStorageUiSubsystem*>(this)->Get();
}


//
// UEditorDataStorageCompatibilitySubsystem
//

UEditorDataStorageCompatibilitySubsystem::~UEditorDataStorageCompatibilitySubsystem()
{
	DataStorageCompatibility = nullptr;
}

IEditorDataStorageCompatibilityProvider* UEditorDataStorageCompatibilitySubsystem::Get()
{
	if (!DataStorageCompatibility)
	{
		UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
		checkf(Registry, TEXT("UEditorDataStorageCompatibilitySubsystem created before the Typed Elements Registry is available."));

		DataStorageCompatibility = Registry->GetMutableDataStorageCompatibility();
	}
	return DataStorageCompatibility;
}

const IEditorDataStorageCompatibilityProvider* UEditorDataStorageCompatibilitySubsystem::Get() const
{
	return const_cast<UEditorDataStorageCompatibilitySubsystem*>(this)->Get();
}
