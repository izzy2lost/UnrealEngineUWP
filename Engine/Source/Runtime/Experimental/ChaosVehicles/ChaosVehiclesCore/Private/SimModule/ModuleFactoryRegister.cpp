// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/ModuleFactoryRegister.h"
#include "SimModule/SimulationModuleBase.h"

namespace Chaos
{
	void FModuleFactoryRegister::RegisterFactory(int32 TypeID, TWeakPtr<IFactoryModule> InFactory)
	{
		if (!ContainsFactory(TypeID))
		{
			RegisteredFactories.Add(TypeID, InFactory);
		}
	}

	void FModuleFactoryRegister::RemoveFactory(TWeakPtr<IFactoryModule> InFactory)
	{
		for (auto& It : RegisteredFactories)
		{
			if (It.Value == InFactory)
			{
				RegisteredFactories.Remove(It.Key);
				return;
			}
		}
	}

	void FModuleFactoryRegister::Reset()
	{
		RegisteredFactories.Reset();
	}

	bool FModuleFactoryRegister::ContainsFactory(int32 TypeID)
	{
		return RegisteredFactories.Contains(TypeID);
	}

	TSharedPtr<Chaos::FModuleNetData> FModuleFactoryRegister::GenerateNetData(int32 TypeID, int32 SimArrayIndex)
	{
		using namespace Chaos;

		if (RegisteredFactories.Contains(TypeID))
		{
			TSharedPtr<IFactoryModule> PinnedFactory = RegisteredFactories[TypeID].Pin();

			if (PinnedFactory.IsValid())
			{
				return PinnedFactory->GenerateNetData(SimArrayIndex);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("No factory registered for type '%d'"), TypeID);
		}

		return nullptr;
	}

}