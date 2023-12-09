// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyBag.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Param/IParameterSource.h"
#include "RigVMHost.h"
#include "AnimNextParameterBlock.generated.h"

class UEdGraph;
struct FAnimNextScheduleGraphTask;
struct FAnimNextScheduleParamScopeTask;

namespace UE::AnimNext
{
	struct FContext;
	struct FParameterBlockProxy;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
	struct FUtilsPrivate;
}

namespace UE::AnimNext::Editor
{
	class FParametersEditor;
	struct FUtils;
	class SParameterBlockViewRow;
	class FParameterBlockParameterCustomization;
}

/** An asset used to define AnimNext parameters and their bindings */
UCLASS(MinimalAPI, BlueprintType)
class UAnimNextParameterBlock : public URigVMHost
{
	GENERATED_BODY()

	UAnimNextParameterBlock(const FObjectInitializer& ObjectInitializer);

	friend class UAnimNextParameterBlockFactory;
	friend class UAnimNextParameterBlock_EditorData;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend struct UE::AnimNext::UncookedOnly::FUtilsPrivate;
	friend class UE::AnimNext::Editor::FParametersEditor;
	friend struct UE::AnimNext::Editor::FUtils;
	friend struct FAnimNode_AnimNextParameters;
	friend struct FAnimNextScheduleGraphTask;
	friend struct FAnimNextScheduleParamScopeEntryTask;
	friend class UE::AnimNext::Editor::SParameterBlockViewRow;
	friend class UE::AnimNext::Editor::FParameterBlockParameterCustomization;
	friend struct UE::AnimNext::FParameterBlockProxy;

	void UpdateLayer(UE::AnimNext::FParamStackLayerHandle& InHandle, float InDeltaTime) const;

	// UObject interface
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	UE_DEPRECATED(5.4, "Implement the version that takes FAssetRegistryTagsContext instead.")
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

	FInstancedPropertyBag& GetPropertyBag() { return PropertyBag; }

	FRigVMExtendedExecuteContext BaseRigVMContext;

	UPROPERTY()
	TObjectPtr<URigVM> RigVM;

	UPROPERTY()
	FInstancedPropertyBag PropertyBag;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Parameters", meta = (ShowInnerProperties))
	TObjectPtr<UObject> EditorData;
#endif
};
