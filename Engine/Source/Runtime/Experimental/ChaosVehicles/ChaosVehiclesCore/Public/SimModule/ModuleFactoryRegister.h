// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "SimulationModuleBase.h"
#include "Templates/SharedPointer.h"

namespace Chaos
{

	struct FModuleNetData;
	class IFactoryModule;

	class CHAOSVEHICLESCORE_API FModuleFactoryRegister
	{
	public:
		static FModuleFactoryRegister& Get()
		{
			static FModuleFactoryRegister Instance;
			return Instance;
		}
		
		void RegisterFactory(const FName TypeName, TWeakPtr<IFactoryModule> InFactory);
		void RemoveFactory(TWeakPtr<IFactoryModule> InFactory);
		void Reset();
		bool ContainsFactory(const FName TypeName) const;
		TSharedPtr<Chaos::FModuleNetData> GenerateNetData(const FName TypeName, const int32 SimArrayIndex);

	protected:

		FModuleFactoryRegister() = default;
		TMap<FName, TWeakPtr<IFactoryModule>> RegisteredFactoriesByName;
	};
	
	template<typename _To, typename ..._Rest>
	class TSimulationModuleTypeable;
	//Static helper function to create and register a factory of the correct type. The returned Factory MUST be stored somewhere by the caller.
	template<typename T, typename... Args>
	static bool RegisterFactoryHelper(Args... args)
	{
		FName SimTypeName = T::StaticSimType();
		if(SimTypeName.IsValid() == false)
		{
			return false;
		}
		if(FModuleFactoryRegister::Get().ContainsFactory(SimTypeName))
		{
			return true;
		}
		static TSharedPtr<T> SharedFactory = MakeShared<T>(args...);
		if (SharedFactory.IsValid())
		{
			FModuleFactoryRegister::Get().RegisterFactory(SimTypeName, SharedFactory);
			return true;
		}
		return false;
	}
} // namespace Chaos
