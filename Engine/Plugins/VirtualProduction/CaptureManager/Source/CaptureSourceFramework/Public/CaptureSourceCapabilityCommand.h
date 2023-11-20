// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureSourceCapabilityProperty.h"

#include "Templates/SharedPointer.h"

struct CAPTURESOURCEFRAMEWORK_API FCommandDesc
{
	FString Name;
	TArray<FPropertyDesc> Params;

	FCommandDesc(FString InName, TArray<FPropertyDesc> InParams);
};

class CAPTURESOURCEFRAMEWORK_API FCommandBase
{
public:
	using FValues = TMap<FString, FPropertyValue>;

	FCommandBase(FString InName);
	virtual ~FCommandBase() = default;

	void AddParamValue(const FString& InName, FPropertyValue InValue);
	FPropertyValue GetParamValue(const FString& InName) const;

	void SetParamValues(FValues InValues);

	const FValues& GetParamValues() const;
	bool ContainsParamValue(const FString& InName) const;

	const FString& GetName() const;

private:

	FString Name;
	FValues Values;
};

class FCommandList
{
public:

	FCommandList() = default;

	void AddCommand(FCommandDesc InCommandDesc);
	TArray<FCommandDesc> GetCommands() const;

	void CheckCommandExists(const FString& InName);
	void CheckCommandParamValue(const FString& InName, const TSharedPtr<FCommandBase>& InCommand);

private:

	TArray<FCommandDesc> Commands;
};