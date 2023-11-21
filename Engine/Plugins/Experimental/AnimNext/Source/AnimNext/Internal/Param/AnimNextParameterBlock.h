// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyBag.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Param/IAnimNextParameterSourceInterface.h"
#include "RigVMHost.h"
#include "AnimNextParameterBlock.generated.h"

class UEdGraph;
struct FAnimNextScheduleGraphTask;
struct FAnimNextScheduleParamScopeTask;

namespace UE::AnimNext
{
	struct FContext;
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

// Library entry used to export to asset registry
USTRUCT()
struct FAnimNextParameterBlockAssetRegistryExportEntry
{
	GENERATED_BODY()

	FAnimNextParameterBlockAssetRegistryExportEntry() = default;

	FAnimNextParameterBlockAssetRegistryExportEntry(FName InName, const FSoftObjectPath& InLibrary)
		: Name(InName)
		, Library(InLibrary)
	{}
	
	UPROPERTY()
	FName Name;

	UPROPERTY()
	FSoftObjectPath Library;
};

// Library used to export to asset registry
USTRUCT()
struct FAnimNextParameterBlockAssetRegistryExports
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FAnimNextParameterBlockAssetRegistryExportEntry> Bindings;
};

/** An asset used to define AnimNext parameters and their bindings */
UCLASS(MinimalAPI, BlueprintType)
class UAnimNextParameterBlock : public URigVMHost, public IAnimNextParameterSourceInterface
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

	// IAnimNextParameterSourceInterface interface
	virtual void UpdateLayer(UE::AnimNext::FParamStackLayerHandle& InHandle) const override;
	virtual UE::AnimNext::FParamStackLayerHandle CacheLayer() const override;
	virtual bool ShouldCacheLayer(const UE::AnimNext::FParamStackLayerHandle& InHandle) const override;

	// UObject interface
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
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
