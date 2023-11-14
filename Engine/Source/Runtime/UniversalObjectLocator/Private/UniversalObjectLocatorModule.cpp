// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubObjectLocator.h"
#include "Modules/ModuleManager.h"
#include "IUniversalObjectLocatorModule.h"
#include "UniversalObjectLocatorFragment.h"
//#include "Modules/VisualizerDebuggingState.h"
#include "Containers/Ticker.h"

#include "DirectPathObjectLocator.h"

namespace UE::UniversalObjectLocator
{

TArray<FFragmentType> GFragmentTypes;

class FUniversalObjectLocatorModule
	: public IUniversalObjectLocatorModule
{
public:
	
	void StartupModule() override
	{
		{
			FFragmentTypeParameters FragmentTypeParams("subobj", NSLOCTEXT("SubObjectLocator", "Object", "Object"));
			FragmentTypeParams.PrimaryEditorType = "SubObject";
			FSubObjectLocator::FragmentType = RegisterFragmentType<FSubObjectLocator>(FragmentTypeParams);
		}

		{
			FFragmentTypeParameters FragmentTypeParams("uobj", NSLOCTEXT("DirectPathObjectLocator", "Object", "Object"));
			FDirectPathObjectLocator::FragmentType = RegisterFragmentType<FDirectPathObjectLocator>(FragmentTypeParams);
		}

		TickerDelegate = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FUniversalObjectLocatorModule::PurgeVisualizers), 60.f);
	}

	void ShutdownModule() override
	{

	}

	FFragmentTypeHandle RegisterFragmentTypeImpl(const FFragmentType& FragmentType) override
	{
		const int32 Index = GFragmentTypes.Num();
		GFragmentTypes.Add(FragmentType);
		checkf(Index < static_cast<int32>(std::numeric_limits<uint8>::max()), TEXT("Maximum number of UOL FragmentTypes reached"));

		// @todo: enable this code once visualizer debugging state is enabled
		// Re-assign the debugging ptr in case it changed
		// UE::Core::FVisualizerDebuggingState::Assign("UOL", GFragmentTypes.GetData());

		return FFragmentTypeHandle(static_cast<uint8>(Index));
	}

	void UnregisterFragmentTypeImpl(FFragmentTypeHandle FragmentType) override
	{
		GFragmentTypes[FragmentType.GetIndex()] = FFragmentType{};
	}

	bool PurgeVisualizers(float) const
	{
		for (FFragmentType& FragmentType : GFragmentTypes)
		{
			if (FragmentType.DebuggingAssistant)
			{
				FragmentType.DebuggingAssistant->Purge();
			}
		}
		return true;
	}

	FTSTicker::FDelegateHandle TickerDelegate;
};

} // namespace UE::UniversalObjectLocator

IMPLEMENT_MODULE(UE::UniversalObjectLocator::FUniversalObjectLocatorModule, UniversalObjectLocator);

