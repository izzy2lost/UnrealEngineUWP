// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Module/AnimNextModule.h"
#include "AssetDefinitionDefault.h"
#include "Module/AnimNextModule_EventGraph.h"
#include "Module/AnimNextModule_Parameter.h"
#include "Graph/AnimNextModule_AnimationGraph.h"
#include "AnimNextModuleAssetDefinition.generated.h"

#define LOCTEXT_NAMESPACE "AnimNextAssetDefinitions"

UCLASS()
class UAssetDefinition_AnimNextModule : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition interface
	virtual FText GetAssetDisplayName() const override { return LOCTEXT("AnimNextModule", "AnimNext Module"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(128,96,48)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UAnimNextModule::StaticClass(); }
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Animation, LOCTEXT("AnimNextSubMenu", "AnimNext")) };
		return Categories;
	}
	virtual bool ShouldSaveExternalPackages() const override { return true; }
};

UCLASS()
class UAssetDefinition_AnimNextParameter : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition interface
	virtual FText GetAssetDisplayName() const override { return LOCTEXT("AnimNextParameter", "Parameter"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(128,64,64)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UAnimNextModule_Parameter::StaticClass(); }
	virtual FText GetObjectDisplayNameText(UObject* Object) const override;
};

UCLASS()
class UAssetDefinition_AnimNextAnimationGraph : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition interface
	virtual FText GetAssetDisplayName() const override { return LOCTEXT("AnimNextAnimationGraph", "Animation Graph"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(128,64,64)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UAnimNextModule_AnimationGraph::StaticClass(); }
	virtual FText GetObjectDisplayNameText(UObject* Object) const override;
};

UCLASS()
class UAssetDefinition_AnimNextEventGraph : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition interface
	virtual FText GetAssetDisplayName() const override { return LOCTEXT("AnimNextEventGraph", "Event Graph"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(128,64,64)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UAnimNextModule_EventGraph::StaticClass(); }
	virtual FText GetObjectDisplayNameText(UObject* Object) const override;
};

#undef LOCTEXT_NAMESPACE