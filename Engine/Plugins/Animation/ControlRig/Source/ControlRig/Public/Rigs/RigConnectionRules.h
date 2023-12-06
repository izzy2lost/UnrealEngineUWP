// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigHierarchyDefines.h"
#include "UObject/StructOnScope.h"
#include "RigConnectionRules.generated.h"

struct FRigBaseElement;
struct FRigConnectionRule;
class FRigElementKeyRedirector;
class URigHierarchy;

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigConnectionRuleStash
{
	GENERATED_BODY()

	FRigConnectionRuleStash();
	FRigConnectionRuleStash(const FRigConnectionRule* InRule);

	void Save(FArchive& Ar);
	void Load(FArchive& Ar);
	
	friend uint32 GetTypeHash(const FRigConnectionRuleStash& InRuleStash);

	bool IsValid() const;
	UScriptStruct* GetScriptStruct() const;
	TSharedPtr<FStructOnScope> Get() const;
	const FRigConnectionRule* Get(TSharedPtr<FStructOnScope>& InOutStorage) const;

	bool operator == (const FRigConnectionRuleStash& InOther) const;

	bool operator != (const FRigConnectionRuleStash& InOther) const
	{
		return !(*this == InOther);
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Rule)
	FString ScriptStructPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Rule)
	FString ExportedText;
};

USTRUCT(meta=(Hidden))
struct CONTROLRIG_API FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigConnectionRule() {}
	virtual ~FRigConnectionRule() {}

	virtual UScriptStruct* GetScriptStruct() const { return FRigConnectionRule::StaticStruct(); }
	virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const URigHierarchy* InHierarchy, const FRigElementKeyRedirector* InRedirector) const;
};

USTRUCT(BlueprintType, DisplayName="And Rule")
struct CONTROLRIG_API FRigAndConnectionRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigAndConnectionRule()
	{}

	template<typename TypeA, typename TypeB>
	FRigAndConnectionRule(const TypeA& InA, const TypeB& InB)
	{
		ChildRules.Emplace(&InA);
		ChildRules.Emplace(&InB);
	}

	virtual ~FRigAndConnectionRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigAndConnectionRule::StaticStruct(); }
	virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const URigHierarchy* InHierarchy, const FRigElementKeyRedirector* InRedirector) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	TArray<FRigConnectionRuleStash> ChildRules;
};

USTRUCT(BlueprintType, DisplayName="Or Rule")
struct CONTROLRIG_API FRigOrConnectionRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigOrConnectionRule()
	{}

	template<typename TypeA, typename TypeB>
	FRigOrConnectionRule(const TypeA& InA, const TypeB& InB)
	{
		ChildRules.Emplace(&InA);
		ChildRules.Emplace(&InB);
	}

	virtual ~FRigOrConnectionRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigOrConnectionRule::StaticStruct(); }
	virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const URigHierarchy* InHierarchy, const FRigElementKeyRedirector* InRedirector) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	TArray<FRigConnectionRuleStash> ChildRules;
};

USTRUCT(BlueprintType, DisplayName="Type Rule")
struct CONTROLRIG_API FRigTypeConnectionRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigTypeConnectionRule()
		: ElementType(ERigElementType::Connector)
	{}

	FRigTypeConnectionRule(ERigElementType InElementType)
	: ElementType(InElementType)
	{}

	virtual ~FRigTypeConnectionRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigTypeConnectionRule::StaticStruct(); }
	virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const URigHierarchy* InHierarchy, const FRigElementKeyRedirector* InRedirector) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	ERigElementType ElementType;
};

USTRUCT(BlueprintType, DisplayName="Tag Rule")
struct CONTROLRIG_API FRigTagConnectionRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigTagConnectionRule()
		: Tag(NAME_None)
	{}

	FRigTagConnectionRule(const FName& InTag)
	: Tag(InTag)
	{}

	virtual ~FRigTagConnectionRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigTagConnectionRule::StaticStruct(); }
	virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const URigHierarchy* InHierarchy, const FRigElementKeyRedirector* InRedirector) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	FName Tag;
};

/*
USTRUCT(BlueprintType, DisplayName="On Chain Rule")
struct CONTROLRIG_API FRigOnChainRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigOnChainRule()
	: MinNumBones(2)
	, MaxNumBones(0)
	{}

	FRigOnChainRule(int32 InMinNumBones = 2, int32 InMaxNumBones = 0)
	: MinNumBones(InMinNumBones)
	, MaxNumBones(InMaxNumBones)
	{}

	virtual ~FRigOnChainRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigOnChainRule::StaticStruct(); }
	virtual FRigElementResolveResult Resolve(const FRigBaseElement* InTarget, const URigHierarchy* InHierarchy, const FRigElementKeyRedirector* InRedirector) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	int32 MinNumBones;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	int32 MaxNumBones;
};
*/