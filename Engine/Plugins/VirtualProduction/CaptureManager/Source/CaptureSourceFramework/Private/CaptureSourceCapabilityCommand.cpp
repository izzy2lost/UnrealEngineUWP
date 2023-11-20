// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureSourceCapabilityCommand.h"

FCommandDesc::FCommandDesc(FString InName, TArray<FPropertyDesc> InParams)
	: Name(MoveTemp(InName))
	, Params(MoveTemp(InParams))
{
}

void FCommandList::AddCommand(FCommandDesc InCommandDesc)
{
	Commands.Add(MoveTemp(InCommandDesc));
}

TArray<FCommandDesc> FCommandList::GetCommands() const
{
	return Commands;
}

void FCommandList::CheckCommandExists(const FString& InName)
{
	check(Commands.ContainsByPredicate([&InName](const FCommandDesc& InCommand)
	{
		return InCommand.Name == InName;
	}));
}

void FCommandList::CheckCommandParamValue(const FString& InName, const TSharedPtr<FCommandBase>& InCommand)
{
	FCommandDesc* Command = Commands.FindByPredicate([&InName](const FCommandDesc& InCommand)
	{
		return InCommand.Name == InName;
	});

	check(Command);

	for (const FPropertyDesc& Param : Command->Params)
	{
		check(InCommand->ContainsParamValue(Param.Name));
	}

	for (const TPair<FString, FPropertyValue>& ParamValue : InCommand->GetParamValues())
	{
		check(Command->Params.ContainsByPredicate([Name = ParamValue.Key](const FPropertyDesc& InElem)
		{
			return InElem.Name == Name;
		}));
	}

	for (const FPropertyDesc& Param : Command->Params)
	{
		check(FPropertyList::CheckPropertyValueType(Param.Type, InCommand->GetParamValue(Param.Name)));
	}
}

FCommandBase::FCommandBase(FString InName)
	: Name(MoveTemp(InName))
{
}

void FCommandBase::AddParamValue(const FString& InName, FPropertyValue InValue)
{
	Values.Emplace(InName, MoveTemp(InValue));
}

FPropertyValue FCommandBase::GetParamValue(const FString& InName) const
{
	return Values[InName];
}

void FCommandBase::SetParamValues(FValues InValues)
{
	Values = MoveTemp(InValues);
}

const FCommandBase::FValues& FCommandBase::GetParamValues() const
{
	return Values;
}

bool FCommandBase::ContainsParamValue(const FString& InName) const
{
	return Values.Contains(InName);
}

const FString& FCommandBase::GetName() const
{
	return Name;
}