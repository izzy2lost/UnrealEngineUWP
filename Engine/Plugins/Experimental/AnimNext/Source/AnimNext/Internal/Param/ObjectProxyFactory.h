// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/IParameterSourceFactory.h"
#include "UObject/ObjectKey.h"

class AActor;
class UActorComponent;
class UAnimNextConfig;
class UAnimNextParameterSchema;
struct FAnimNextParamInstanceIdentifier;

namespace UE::AnimNext
{
	struct FClassProxy;
	class FModule;
}

namespace UE::AnimNext::UncookedOnly
{
	class FObjectProxyType;
}

namespace UE::AnimNext
{

// Factory for object proxies that supply 'external' parameters
class FObjectProxyFactory : public IParameterSourceFactory
{
	friend class ::UAnimNextConfig;
	friend class FModule;
	friend class UE::AnimNext::UncookedOnly::FObjectProxyType;
	friend struct FScheduleInitializationContext;

public:
	FObjectProxyFactory();
	virtual ~FObjectProxyFactory() override;

private:
	// Refresh built-in accessors
	void Refresh();

	// IParameterSourceFactory interface
	virtual TUniquePtr<IParameterSource> CreateParameterSource(const FParameterSourceContext& InContext, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId, TConstArrayView<FName> InRequiredParameters) const override;

	// Finds a class proxy for the supplied class
	ANIMNEXT_API TSharedRef<FClassProxy> FindOrCreateClassProxy(const UClass* InClass) const;

	// Map of classes -> proxy
	mutable TMap<TObjectKey<UClass>, TSharedPtr<FClassProxy>> ClassMap;

	// Detect concurrent access for ObjectAccessors
	UE_MT_DECLARE_RW_RECURSIVE_ACCESS_DETECTOR(ObjectAccessorsAccessDetector);

#if WITH_EDITOR
	FDelegateHandle OnObjectsReinstancedHandle;
#endif
};

}