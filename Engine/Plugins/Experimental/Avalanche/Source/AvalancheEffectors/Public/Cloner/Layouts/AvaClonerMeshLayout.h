// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaClonerEffectorShared.h"
#include "AvaClonerLayoutBase.h"
#include "AvaPropertyChangeDispatcher.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "AvaClonerMeshLayout.generated.h"

class AActor;
class USceneComponent;

UCLASS(BlueprintType)
class AVALANCHEEFFECTORS_API UAvaClonerMeshLayout : public UAvaClonerLayoutBase
{
	GENERATED_BODY()

	friend class FAvaClonerActorVisualizer;
	
public:
	UAvaClonerMeshLayout()
		: UAvaClonerLayoutBase(
			TEXT("Mesh")
			, TEXT("/Script/Niagara.NiagaraSystem'/Avalanche/ClonerResources/Systems/NS_ClonerSampleMesh.NS_ClonerSampleMesh'")
		)
	{}
	
	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Mesh")
	void SetCount(int32 InCount);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Mesh")
	int32 GetCount() const
	{
		return Count;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Mesh")
	void SetAsset(EAvaClonerMeshAsset InAsset);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Mesh")
	EAvaClonerMeshAsset GetAsset() const
	{
		return Asset;
	}
	
	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Mesh")
	void SetSampleData(EAvaClonerMeshSampleData InSampleData);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Mesh")
	EAvaClonerMeshSampleData GetSampleData() const
	{
		return SampleData;
	}

	UFUNCTION()
	void SetSampleActorWeak(const TWeakObjectPtr<AActor>& InSampleActor);
	
	TWeakObjectPtr<AActor> GetSampleActorWeak() const
	{
		return SampleActorWeak;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Mesh")
	void SetSampleActor(AActor* InActor);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Mesh")
	AActor* GetSampleActor() const
	{
		return SampleActorWeak.Get();
	}

	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

protected:
	virtual void OnLayoutInactive() override;
	virtual void OnLayoutParametersChanged(UAvaClonerComponent* InComponent) override;
	
	void OnSampleMeshTransformed(USceneComponent* InComponent, EUpdateTransformFlags InFlags, ETeleportType InType);
	
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetCount", Getter="GetCount", Category="Layout")
	int32 Count = 3 * 3 * 3;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetAsset", Getter="GetAsset", Category="Layout")
	EAvaClonerMeshAsset Asset = EAvaClonerMeshAsset::StaticMesh;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetSampleData", Getter="GetSampleData", Category="Layout")
	EAvaClonerMeshSampleData SampleData = EAvaClonerMeshSampleData::Vertices;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetSampleActorWeak", Getter="GetSampleActorWeak", DisplayName="SampleActor", Category="Layout")
	TWeakObjectPtr<AActor> SampleActorWeak;
	
	UPROPERTY(Transient)
	TWeakObjectPtr<USceneComponent> SceneComponentWeak;

private:
#if WITH_EDITOR
	/** Used for PECP */
	static const TAvaPropertyChangeDispatcher<UAvaClonerMeshLayout> PropertyChangeDispatcher;
#endif
};