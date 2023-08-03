// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"

#include "WorldPartition/DataLayer/DataLayerType.h" 

#include "DataLayerAsset.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EDataLayerLoadFilter : uint8
{
	None,
	ClientOnly,
	ServerOnly
};

UCLASS(BlueprintType, editinlinenew, MinimalAPI)
class UDataLayerAsset : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class UDataLayerConversionInfo;

#if WITH_EDITOR
	//~ Begin UObject Interface
	ENGINE_API virtual void PostLoad() override;
	ENGINE_API virtual bool CanEditChange(const FProperty* InProperty) const override;
	//~ End UObject Interface

public:
	void SetType(EDataLayerType Type) 
	{ 
		check(Type == EDataLayerType::Editor || !IsPrivate());
		DataLayerType = Type; 
	}
	void SetDebugColor(FColor InDebugColor) { DebugColor = InDebugColor; }
	ENGINE_API bool CanBeReferencedByActor(AActor* InActor) const;
	static ENGINE_API bool CanBeReferencedByActor(const TSoftObjectPtr<UDataLayerAsset>& InDataLayerAsset, AActor* InActor);
#endif

	ENGINE_API bool IsPrivate() const;

	UFUNCTION(Category = "Data Layer", BlueprintCallable)
	EDataLayerType GetType() const { return DataLayerType; }

	UFUNCTION(Category = "Data Layer|Runtime", BlueprintCallable)
	bool IsRuntime() const { return !IsPrivate() && DataLayerType == EDataLayerType::Runtime; }

	UFUNCTION(Category = "Data Layer|Runtime", BlueprintCallable)
	FColor GetDebugColor() const { return DebugColor; }

	bool SupportsActorFilters() const { return bSupportsActorFilters; }

	UFUNCTION(Category = "Data Layer|Runtime", BlueprintCallable)
	bool IsClientOnly() const { return IsRuntime() && LoadFilter == EDataLayerLoadFilter::ClientOnly; }

	UFUNCTION(Category = "Data Layer|Runtime", BlueprintCallable)
	bool IsServerOnly() const { return IsRuntime() && LoadFilter == EDataLayerLoadFilter::ServerOnly; }

private:
	/** Whether the Data Layer affects actor runtime loading */
	UPROPERTY(Category = "Data Layer", EditAnywhere)
	EDataLayerType DataLayerType;

	UPROPERTY(Category = "Actor Filter", EditAnywhere)
	bool bSupportsActorFilters;
		
	UPROPERTY(Category = "Runtime", EditAnywhere)
	FColor DebugColor;

	UPROPERTY(Category = "Runtime", EditAnywhere)
	EDataLayerLoadFilter LoadFilter;
};
