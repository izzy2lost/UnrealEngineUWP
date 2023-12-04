// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigHierarchyDefines.h"
#include "UObject/StructOnScope.h"
#include "RigConnectionRules.generated.h"

struct FRigConnectionRule;
struct FRigConnectionInfo;

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	FName Tag;
};

USTRUCT(BlueprintType, DisplayName="Chain Rule")
struct CONTROLRIG_API FRigChainConnectionRule : public FRigConnectionRule
{
	GENERATED_BODY()

public:

	FRigChainConnectionRule()
	: MinNumBones(2)
	, MaxNumBones(0)
	, bAllowBranches(false)
	{}

	FRigChainConnectionRule(const FRigElementKey& InRootConnector, int32 InMinNumBones = 2, int32 InMaxNumBones = 0, bool InAllowBranches = false)
	: RootConnector(InRootConnector)
	, MinNumBones(InMinNumBones)
	, MaxNumBones(InMaxNumBones)
	, bAllowBranches(InAllowBranches)
	{}

	virtual ~FRigChainConnectionRule() override {}

	virtual UScriptStruct* GetScriptStruct() const override { return FRigChainConnectionRule::StaticStruct(); }

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	FRigElementKey RootConnector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	int32 MinNumBones;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	int32 MaxNumBones;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	bool bAllowBranches;
};