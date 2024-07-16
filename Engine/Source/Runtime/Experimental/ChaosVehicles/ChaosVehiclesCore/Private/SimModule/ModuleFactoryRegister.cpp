// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/ModuleFactoryRegister.h"
#include "SimModule/SimulationModuleBase.h"

namespace Chaos
{
	void FModuleFactoryRegister::RegisterFactory(const FName TypeName, TWeakPtr<IFactoryModule> InFactory)
	{
		if (!ContainsFactory(TypeName))
		{
			RegisteredFactoriesByName.Add(TypeName, InFactory);
		}
	}

	void FModuleFactoryRegister::RemoveFactory(TWeakPtr<IFactoryModule> InFactory)
	{
		for (TPair<FName, TWeakPtr<IFactoryModule>> Pair : RegisteredFactoriesByName)
		{
			if (Pair.Value == InFactory)
			{RegisteredFactoriesByName.Remove(Pair.Key);
				
				return;
			}
		}
	}

	void FModuleFactoryRegister::Reset()
	{
		RegisteredFactoriesByName.Reset();
	}

	bool FModuleFactoryRegister::ContainsFactory(const FName TypeName) const
	{
		return RegisteredFactoriesByName.Contains(TypeName);
	}

	TSharedPtr<Chaos::FModuleNetData> FModuleFactoryRegister::GenerateNetData(const FName TypeName, const int32 SimArrayIndex)
	{
		using namespace Chaos;

		if (RegisteredFactoriesByName.Contains(TypeName))
		{
			TSharedPtr<IFactoryModule> PinnedFactory = RegisteredFactoriesByName[TypeName].Pin();

			if (PinnedFactory.IsValid())
			{
				return PinnedFactory->GenerateNetData(SimArrayIndex);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("No factory registered for name '%s'"), *TypeName.ToString());
		}

		return nullptr;
	}
}