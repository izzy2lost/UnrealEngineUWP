// Copyright Epic Games, Inc. All Rights Reserved.

#include "View/MVVMBindingSubsystem.h"

#include "Framework/Application/SlateApplication.h"
#include "Misc/MemStack.h"
#include "SlateGlobals.h"
#include "Stats/Stats2.h"
#include "View/MVVMView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMBindingSubsystem)


DECLARE_CYCLE_STAT(TEXT("MVVM Bindings"), STAT_MVVMBindingTick, STATGROUP_Slate);

void UMVVMBindingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnPreTick().AddUObject(this, &UMVVMBindingSubsystem::HandlePreTick);
	}
}

void UMVVMBindingSubsystem::Deinitialize()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnPreTick().RemoveAll(this);
	}
	Super::Deinitialize();
}

namespace UE::MVVM::Private
{
struct FViewAndBinding
{
	FViewAndBinding(const TObjectKey<const UMVVMView>& InView, FMVVMViewDelayedBinding InBinding)
		: View(InView)
		, Binding(InBinding)
	{
	}
	TObjectKey<const UMVVMView> View;
	FMVVMViewDelayedBinding Binding;

	bool operator== (const FViewAndBinding& Other) const
	{
		return View == Other.View && Binding == Other.Binding;
	}

	friend uint32 GetTypeHash(const FViewAndBinding& Key)
	{
		uint32 Value1 = GetTypeHash(Key.View);
		uint32 Value2 = GetTypeHash(Key.Binding.GetCompiledBindingIndex());
		return HashCombine(Value1, Value2);
	}
};
}

void UMVVMBindingSubsystem::HandlePreTick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_MVVMBindingTick);

	if (EveryTickBindings.Num() > 0)
	{
		FMemMark Mark(FMemStack::Get());
		TArray<const UMVVMView*, TMemStackAllocator<>> ToTick;
		ToTick.Reserve(EveryTickBindings.Num());
		for (TWeakObjectPtr<const UMVVMView> View : EveryTickBindings)
		{
			if (const UMVVMView* ViewPtr = View.Get())
			{
				ToTick.Add(ViewPtr);
			}
		}

		for (const UMVVMView* ViewPtr : ToTick)
		{
			ViewPtr->ExecuteEveryTickBindings();
		}
	}

	if (DelayedBindings.Num() > 0)
	{
		TSet<UE::MVVM::Private::FViewAndBinding> AllDelayedBindingsExecutedThisFrame;
		AllDelayedBindingsExecutedThisFrame.Reserve(DelayedBindings.Num());

		FDelayedMap DelayedBindingsWhileTicking = MoveTemp(DelayedBindings);
		DelayedBindings = FDelayedMap();

		do 
		{
			for (const auto& DelayedBindingsPair : DelayedBindingsWhileTicking)
			{
				if (const UMVVMView* View = DelayedBindingsPair.Key.ResolveObjectPtr())
				{
					ensure(DelayedBindingsPair.Value.Num() > 0);
					for (const FMVVMViewDelayedBinding& DelayedBinding : DelayedBindingsPair.Value)
					{
						View->ExecuteDelayedBinding(DelayedBinding);
						UE::MVVM::Private::FViewAndBinding ViewAndBinding = UE::MVVM::Private::FViewAndBinding(DelayedBindingsPair.Key, DelayedBinding);
						AllDelayedBindingsExecutedThisFrame.Add(ViewAndBinding);
					}
				}
			}

			DelayedBindingsWhileTicking.Reset();

			// Test new bindings added while executing the latest binding list.
			//If it's a new  binding (not already executed this frame), execute it this frame. Else, execute it next frame.
			for (auto DelayedBindingItt = DelayedBindings.CreateIterator(); DelayedBindingItt; ++DelayedBindingItt)
			{
				FDelayedBindingList* FoundDelayedBindingListPtr = nullptr;
				for (int32 DelayIndex = DelayedBindingItt.Value().Num() - 1; DelayIndex >= 0; --DelayIndex)
				{
					// Was it executed this frame
					const FMVVMViewDelayedBinding& DelayedBinding = DelayedBindingItt.Value()[DelayIndex];
					UE::MVVM::Private::FViewAndBinding ViewAndBinding = UE::MVVM::Private::FViewAndBinding(DelayedBindingItt.Key(), DelayedBinding);
					if (!AllDelayedBindingsExecutedThisFrame.Find(ViewAndBinding))
					{
						if (!FoundDelayedBindingListPtr)
						{
							FoundDelayedBindingListPtr = &DelayedBindingsWhileTicking.FindOrAdd(DelayedBindingItt.Key());
						}
						FoundDelayedBindingListPtr->AddUnique(DelayedBinding);
						DelayedBindingItt.Value().RemoveAtSwap(DelayIndex);
					}

					// If they were all executed, remove the key from the next frame list.
					if (DelayedBindingItt.Value().Num() == 0)
					{
						DelayedBindingItt.RemoveCurrent();
					}
				}
			}

			//DelayedBindings = FDelayedMap(); // do not reset the array. Bindings could be executed this frame and needs to be re executed next frame

		} while (DelayedBindingsWhileTicking.Num() > 0);
	}
}

void UMVVMBindingSubsystem::AddViewWithEveryTickBinding(const UMVVMView* InView)
{
	check(!EveryTickBindings.Contains(InView));
	ensureMsgf(FSlateApplication::IsInitialized(), TEXT("The Slate Application is not initialized. This is probably because you are running a server. The Delayed and Tick binding will not execute."));

	EveryTickBindings.Add(InView);
}

void UMVVMBindingSubsystem::RemoveViewWithEveryTickBinding(const UMVVMView* InView)
{
	EveryTickBindings.RemoveSingleSwap(InView);
}

void UMVVMBindingSubsystem::AddDelayedBinding(const UMVVMView* View, FMVVMViewDelayedBinding InCompiledBinding)
{
	ensureMsgf(FSlateApplication::IsInitialized(), TEXT("The Slate Application is not initialized. This is probably because you are running a server. The Delayed and Tick binding will not execute."));
	DelayedBindings.FindOrAdd(View).AddUnique(InCompiledBinding);
}
