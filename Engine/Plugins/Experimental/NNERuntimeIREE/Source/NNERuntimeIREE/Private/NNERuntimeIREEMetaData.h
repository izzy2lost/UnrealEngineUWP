// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Serialization/JsonSerializerMacros.h"

namespace UE::NNERuntimeIREE
{
	struct FArgumentMetaData : FJsonSerializable
	{
		FString Name;
		TArray<int32> Shape;
		FString Type;

		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE("n", Name);
			JSON_SERIALIZE_ARRAY("s", Shape);
			JSON_SERIALIZE("t", Type);
		END_JSON_SERIALIZER

		bool ParseFromString(const FString& ArgumentString);
	};

	struct FFunctionMetaData : FJsonSerializable
	{
		FString Name;
		TArray<FArgumentMetaData> ArgumentMetaData;
		TArray<FArgumentMetaData> ResultMetaData;

		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE("n", Name);
			JSON_SERIALIZE_ARRAY_SERIALIZABLE("a", ArgumentMetaData, FArgumentMetaData);
			JSON_SERIALIZE_ARRAY_SERIALIZABLE("r", ResultMetaData, FArgumentMetaData);
		END_JSON_SERIALIZER

		bool ParseFromString(const FString& FunctionString);
	};

	struct FModuleMetaData : FJsonSerializable
	{
		TMap<FString, FFunctionMetaData> FunctionMetaData;

		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE_MAP_SERIALIZABLE("f", FunctionMetaData, FFunctionMetaData);
		END_JSON_SERIALIZER

		bool ParseFromString(const FString& ModuleString);
	};
} // UE::NNERuntimeIREE