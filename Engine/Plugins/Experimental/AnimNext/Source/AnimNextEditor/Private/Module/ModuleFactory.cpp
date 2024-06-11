// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/ModuleFactory.h"
#include "Module/AnimNextModule.h"
#include "Module/AnimNextModule_EditorData.h"
#include "UncookedOnlyUtils.h"

UAnimNextModuleFactory::UAnimNextModuleFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UAnimNextModule::StaticClass();
}

bool UAnimNextModuleFactory::ConfigureProperties()
{
	return true;
}

UObject* UAnimNextModuleFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	EObjectFlags FlagsToUse = Flags | RF_Public | RF_Standalone | RF_Transactional | RF_LoadCompleted;
	if(InParent == GetTransientPackage())
	{
		FlagsToUse &= ~RF_Standalone;
	}

	UAnimNextModule* NewModule = NewObject<UAnimNextModule>(InParent, Class, Name, FlagsToUse);

	// Create internal editor data
	UAnimNextModule_EditorData* EditorData = NewObject<UAnimNextModule_EditorData>(NewModule, TEXT("EditorData"), RF_Transactional);
	NewModule->EditorData = EditorData;
	EditorData->Initialize(/*bRecompileVM*/false);

	// Compile the initial skeleton
	UE::AnimNext::UncookedOnly::FUtils::Compile(NewModule);
	check(!EditorData->bErrorsDuringCompilation);

	return NewModule;
}