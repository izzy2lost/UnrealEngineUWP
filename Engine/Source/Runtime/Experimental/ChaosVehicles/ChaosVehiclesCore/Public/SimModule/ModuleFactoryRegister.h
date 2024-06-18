// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
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

		void RegisterFactory(int32 TypeID, TWeakPtr<IFactoryModule> InFactory);
		void RegisterFactory(const FName TypeName, TWeakPtr<IFactoryModule> InFactory);
		void RemoveFactory(TWeakPtr<IFactoryModule> InFactory);
		void Reset();
		bool ContainsFactory(int32 TypeID);
		bool ContainsFactory(const FName TypeName);
		TSharedPtr<Chaos::FModuleNetData> GenerateNetData(int32 TypeID, int32 SimArrayIndex);
		TSharedPtr<Chaos::FModuleNetData> GenerateNetData(const FName TypeName, const int32 SimArrayIndex);

	protected:

		FModuleFactoryRegister() = default;
		TMap<int32, TWeakPtr<IFactoryModule>> RegisteredFactories;
		TMap<FName, TWeakPtr<IFactoryModule>> RegisteredFactoriesByName;
	};

} // namespace Chaos
