// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

THIRD_PARTY_INCLUDES_START
#include "onnxruntime_cxx_api.h"
THIRD_PARTY_INCLUDES_END

class FNNEUtilitiesModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
		Ort::InitApi();
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FNNEUtilitiesModule, NNEUtilities)