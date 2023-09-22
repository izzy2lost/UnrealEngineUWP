// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UNNERuntimeORTCustomCpuImpl;

class FNNERuntimeORTCpuModule : public IModuleInterface
{

public:
	TWeakObjectPtr<UNNERuntimeORTCustomCpuImpl> NNERuntimeORTCpu{ nullptr };

	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};