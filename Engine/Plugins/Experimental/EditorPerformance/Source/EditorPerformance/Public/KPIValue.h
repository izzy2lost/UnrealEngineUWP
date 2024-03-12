// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "Containers/Array.h"
#include "Containers/Map.h"

class FKPIValue
{
public:

	enum ECompare : uint8
	{
		LessThan,
		LessThanOrEqual,
		GreaterThan,
		GreaterThanOrEqual,
	};

	enum EDisplayType : uint8
	{
		Number,
		Decimal,
		Seconds,
		Milliseconds,
		Minutes,
		Bytes,
		MegaBytes,
		GigaBytes,
		MegaBitsPerSecond,
		Percent
	};

	enum EState : uint8
	{
		NotSet,
		Good,
		Bad,
	};

	FKPIValue(FName NewCategory, FName NewName, float NewInitialValue, float NewThresholdValue, FKPIValue::ECompare NewCompare = ECompare::LessThan, FKPIValue::EDisplayType NewDisplayType=EDisplayType::Number, FKPIValue::EState NewState= NotSet):
		Id(FGuid::NewGuid()),
		Category(NewCategory),
		Name(NewName),
		CurrentValue(NewInitialValue),
		ThresholdValue(NewThresholdValue),
		State(NewState),
		Compare(NewCompare),
		DisplayType(NewDisplayType)	
	{}

	FKPIValue()
	{}

	EState				GetState() const;
	void				SetValue(float Value);
	static FString		GetValueAsString( float Value, FKPIValue::EDisplayType Type );
	static FString		GetComparisonAsString(FKPIValue::ECompare Compare);
	static FString		GetComparisonAsPrettyString(FKPIValue::ECompare Compare);
	static FString		GetDisplayTypeAsString(FKPIValue::EDisplayType Type);

	FGuid			Id;
	FName			Category;
	FName			Name;
	float			CurrentValue = 0;
	float			ThresholdValue = 0;
	EState			State = EState::NotSet;
	ECompare		Compare = ECompare::LessThan;
	EDisplayType	DisplayType = EDisplayType::Number;
	
};

typedef TMap<FName, FKPIValue> FKPIValues;
typedef TMap<FName, float> FKPIThesholds;

class FKPIProfile
{
public:
	FString				MapName=TEXT("");
	FKPIThesholds		Thresholds;
};

typedef TMap<FString, FKPIProfile> FKPIProfiles;

class FKPIHint
{
public:
	FName			Category;
	FName			Name;
	FText			Message;
	FText			URL;
};

typedef TMap<FName, FKPIHint> FKPIHints;

class FKPIRegistry
{
public:

	bool							DeclareKPIValue(const FName Category, const FName Name, float InitialValue, float ThresholdValue, FKPIValue::ECompare Compare, FKPIValue::EDisplayType Type);
	bool							DeclareKPIValue(const FKPIValue& Value);
	bool							DeclareKPIHint(const FName Category, const FName Name, const FText& HintMessage, const FText& HintURL);

	bool							SetKPIValue(const FName Name, float CurrentValue);
	bool							SetKPIThreshold(const FName Name, float ThresholdValue);
	bool							InvalidateKPIValue(const FName Name);
	bool 							GetKPIValue(const FName Name, FKPIValue& Result) const;
	bool 							GetKPIHint(const FName Name, FKPIHint& Result) const;
	const FKPIValues&				GetKPIValues() const;
	const FKPIProfiles&				GetKPIProfiles() const;
	

	void							LoadKPIHints(const FString& HintSectionName, const FString& FileName);
	void							LoadKPIProfiles(const FString& ProfileSectionName, const FString& FileName);
	bool							ApplyKPIProfile(const FKPIProfile& Profile);

private:

	FKPIValues						Values;
	FKPIProfiles					Profiles;
	FKPIHints						Hints;
};

