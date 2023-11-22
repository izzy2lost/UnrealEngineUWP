// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#ifdef WITH_NNE_RUNTIME_IREE
#include "NNERuntimeIREECpu.h"
#include "NNERuntimeIREEGpu.h"
#include "NNERuntimeIREERdg.h"
#endif // WITH_NNE_RUNTIME_IREE

class FNNERuntimeIREEModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

#ifdef WITH_NNE_RUNTIME_IREE
private:
	TWeakObjectPtr<UNNERuntimeIREECpu> NNERuntimeIREECpu;
	TWeakObjectPtr<UNNERuntimeIREECuda> NNERuntimeIREECuda;
	TWeakObjectPtr<UNNERuntimeIREEVulkan> NNERuntimeIREEVulkan;
	TWeakObjectPtr<UNNERuntimeIREERdg> NNERuntimeIREERdg;
#endif // WITH_NNE_RUNTIME_IREE
};
