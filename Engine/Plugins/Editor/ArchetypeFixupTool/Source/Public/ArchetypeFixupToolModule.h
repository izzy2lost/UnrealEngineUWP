// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"

class ARCHETYPEFIXUPTOOL_API FArchetypeFixupToolModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	bool OpenArchetypeFixupTool() const;
	static FArchetypeFixupToolModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FArchetypeFixupToolModule>("ArchetypeFixupTool");
	}

	// opens the fixup tool 
	TSharedRef<SDockTab> CreateArchetypeFixupTab(const FSpawnTabArgs& TabArgs, TConstArrayView<TObjectPtr<UObject>> Archetypes) const;
};
