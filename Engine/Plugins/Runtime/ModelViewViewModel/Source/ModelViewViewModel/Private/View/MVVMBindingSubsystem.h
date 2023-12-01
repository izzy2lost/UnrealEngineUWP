// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/EngineSubsystem.h"
#include "UObject/ObjectKey.h"
#include "View/MVVMViewClass.h"

#include "MVVMBindingSubsystem.generated.h"

class UMVVMView;

/** */
UCLASS(NotBlueprintable, Hidden)
class UMVVMBindingSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void AddViewWithTickBinding(const UMVVMView* View);
	void RemoveViewWithTickBinding(const UMVVMView* View);

	void AddDelayedBinding(const UMVVMView* View, FMVVMViewClass_BindingKey BindingKey);
	void RemoveDelayedBindings(const UMVVMView* View);
	void RemoveDelayedBindings(const UMVVMView* View, FMVVMViewClass_SourceKey SourceKey);

private:
	void HandlePreTick(float DeltaTIme);

	using FDelayedBindingList = TArray<FMVVMViewClass_BindingKey, TInlineAllocator<8>>;
	using FDelayedBindingMap = TMap<TObjectKey<const UMVVMView>, FDelayedBindingList>;
	FDelayedBindingMap DelayedBindings;
	TArray<TWeakObjectPtr<const UMVVMView>> ViewsWithTickBindings;
};
