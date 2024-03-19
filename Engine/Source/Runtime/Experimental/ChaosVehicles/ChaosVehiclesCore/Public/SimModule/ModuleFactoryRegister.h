// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


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
	void RemoveFactory(TWeakPtr<IFactoryModule> InFactory);
	bool ContainsFactory(int32 TypeID);
	TSharedPtr<Chaos::FModuleNetData> GenerateNetData(int32 TypeID, int32 SimArrayIndex);

protected:

	FModuleFactoryRegister() = default;
	TMap<int32, TWeakPtr<IFactoryModule>> RegisteredFactories;
};

} // namespace Chaos
