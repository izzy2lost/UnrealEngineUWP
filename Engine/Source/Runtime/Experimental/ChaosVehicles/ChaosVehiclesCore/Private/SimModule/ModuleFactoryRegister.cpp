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

	void FModuleFactoryRegister::RegisterFactory(const FName TypeName, TWeakPtr<IFactoryModule> InFactory)
	{
		if (!ContainsFactory(TypeName))
		{
			RegisteredFactoriesByName.Add(TypeName, InFactory);
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

		for (TPair<FName, TWeakPtr<IFactoryModule>> Pair : RegisteredFactoriesByName)
		{
			if (Pair.Value == InFactory)
			{
				RegisteredFactoriesByName.Remove(Pair.Key);
				return;
			}
		}
	}

	void FModuleFactoryRegister::Reset()
	{
		RegisteredFactories.Reset();
		RegisteredFactoriesByName.Reset();
	}

	bool FModuleFactoryRegister::ContainsFactory(int32 TypeID)
	{
		return RegisteredFactories.Contains(TypeID);
	}

	bool FModuleFactoryRegister::ContainsFactory(const FName TypeName)
	{
		return RegisteredFactoriesByName.Contains(TypeName);
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