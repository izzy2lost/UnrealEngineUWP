// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Class.h"

#include "Containers/UnrealString.h"

#include "Misc/Guid.h"

#include "LiveLinkHubCaptureMessages.generated.h"

namespace UE::LiveLinkHubCaptureMessages
{
	constexpr uint16 Version = 1;
}

UENUM()
enum class EStatus
{
	Ok = 0,
	InvalidArgument = 1
};

USTRUCT()
struct FBaseMessage
{
	GENERATED_BODY()

	UPROPERTY()
	uint16 Version = UE::LiveLinkHubCaptureMessages::Version;

	UPROPERTY()
	FGuid Guid;
};

USTRUCT()
struct FBaseResponse : public FBaseMessage
{
	GENERATED_BODY()

	UPROPERTY()
	EStatus Status = EStatus::Ok;

	UPROPERTY()
	FString Message;

	UPROPERTY()
	FGuid RequestGuid;
};

USTRUCT()
struct FConnectRequest : public FBaseMessage
{
	GENERATED_BODY()
};

USTRUCT()
struct FConnectResponse : public FBaseResponse
{
	GENERATED_BODY()
};

USTRUCT()
struct FDiscoveryRequest
{
	GENERATED_BODY()
};

USTRUCT()
struct FDiscoveryResponse
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;
};

USTRUCT()
struct FPingRequest : public FBaseMessage
{
	GENERATED_BODY();
};

USTRUCT()
struct FPingResponse : public FBaseResponse
{
	GENERATED_BODY();
};

USTRUCT()
struct FGetCapabilitiesRequest : public FBaseMessage
{
	GENERATED_BODY()
};

USTRUCT()
struct FGetCapabilitiesResponse : public FBaseResponse
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FString> Capabilities;
};

UENUM()
enum class EValueType
{
	Unknown = -1,
	Bool = 0,
	Integer = 1,
	FloatingPoint = 2,
	String = 3
};

UENUM()
enum class EValueAccess
{
	ReadWrite = 0,
	ReadOnly = 1,
};

USTRUCT()
struct FValueDescriptor
{
	GENERATED_BODY()

	UPROPERTY()
	EValueType Type = EValueType::Bool;

	UPROPERTY()
	EValueAccess Access = EValueAccess::ReadOnly;
};

USTRUCT()
struct FMapValuesDescriptor
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FString, FValueDescriptor> Values;
};

USTRUCT()
struct FGetCapabilityRequest : public FBaseMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString Capability;
};

USTRUCT()
struct FGetCapabilityResponse : public FBaseResponse
{
	GENERATED_BODY()

	UPROPERTY()
	FMapValuesDescriptor Properties;

	UPROPERTY()
	TMap<FString, FMapValuesDescriptor> Commands;

	UPROPERTY()
	TMap<FString, FMapValuesDescriptor> Events;
};

USTRUCT()
struct FGetPropertyValueRequest : public FBaseMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString Capability;

	UPROPERTY()
	FString Property;
};

USTRUCT()
struct FValue
{
	GENERATED_BODY()

	UPROPERTY()
	EValueType Type = EValueType::Bool;

	UPROPERTY(meta = (EditCondition = "Type == EValueType::Bool"))
	bool BoolValue = false;

	UPROPERTY(meta = (EditCondition = "Type == EValueType::Integer"))
	int64 IntegerValue = 0;

	UPROPERTY(meta = (EditCondition = "Type == EValueType::FloatingPoint"))
	double FloatingPointValue = 0.f;

	UPROPERTY(meta = (EditCondition = "Type == EValueType::String"))
	FString StringValue;
};

USTRUCT()
struct FGetPropertyValueResponse : public FBaseResponse
{
	GENERATED_BODY()

	UPROPERTY()
	FValue Value;
};

USTRUCT()
struct FSetPropertyValueRequest : public FBaseMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString Capability;
	
	UPROPERTY()
	FString Property;

	UPROPERTY()
	FValue Value;
};

USTRUCT()
struct FSetPropertyValueResponse : public FBaseResponse
{
	GENERATED_BODY()
};

USTRUCT()
struct FExecuteCommandRequest : public FBaseMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString Capability;

	UPROPERTY()
	FString Command;

	UPROPERTY()
	TMap<FString, FValue> Params;
};

USTRUCT()
struct FExecuteCommandResponse : public FBaseResponse
{
	GENERATED_BODY()
};

USTRUCT()
struct FEventMessage : public FBaseMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString Capability;

	UPROPERTY()
	FString Name;

	UPROPERTY()
	TMap<FString, FValue> Params;
};
