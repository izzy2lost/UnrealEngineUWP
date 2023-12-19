// Copyright Epic Games, Inc. All Rights Reserved.

#include "Serialization/JsonSerializable.h"
#include "Serialization/JsonSerializerReader.h"
#include "Serialization/JsonSerializerWriter.h"

FJsonSerializable::~FJsonSerializable() 
{
}

const FString FJsonSerializable::ToJson(bool bPrettyPrint/*=true*/) const
{
	// Strip away const, because we use a single method that can read/write which requires non-const semantics
	// Otherwise, we'd have to have 2 separate macros for declaring const to json and non-const from json
	return ((FJsonSerializable*)this)->ToJson(bPrettyPrint);
}

const FString FJsonSerializable::ToJson(bool bPrettyPrint/*=true*/)
{
	FString JsonStr;
	if (bPrettyPrint)
	{
		TSharedRef<TJsonWriter<> > JsonWriter = TJsonWriterFactory<>::Create(&JsonStr);
		FJsonSerializerWriter<> Serializer(JsonWriter);
		Serialize(Serializer, false);
		JsonWriter->Close();
	}
	else
	{
		TSharedRef< TJsonWriter< TCHAR, TCondensedJsonPrintPolicy< TCHAR > > > JsonWriter = TJsonWriterFactory< TCHAR, TCondensedJsonPrintPolicy< TCHAR > >::Create( &JsonStr );
		FJsonSerializerWriter<TCHAR, TCondensedJsonPrintPolicy< TCHAR >> Serializer(JsonWriter);
		Serialize(Serializer, false);
		JsonWriter->Close();
	}
	return JsonStr;
}

void FJsonSerializable::ToJson(TSharedRef<TJsonWriter<> >& JsonWriter, bool bFlatObject) const
{
	FJsonSerializerWriter<> Serializer(JsonWriter);
	((FJsonSerializable*)this)->Serialize(Serializer, bFlatObject);
}

void FJsonSerializable::ToJson(TSharedRef< TJsonWriter< TCHAR, TCondensedJsonPrintPolicy< TCHAR > > >& JsonWriter, bool bFlatObject) const
{
	FJsonSerializerWriter<TCHAR, TCondensedJsonPrintPolicy< TCHAR >> Serializer(JsonWriter);
	((FJsonSerializable*)this)->Serialize(Serializer, bFlatObject);
}

bool FJsonSerializable::FromJson(const FString& Json)
{
	return FromJson(CopyTemp(Json));
}

bool FJsonSerializable::FromJson(FString&& Json)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(MoveTemp(Json));
	if (FJsonSerializer::Deserialize(JsonReader,JsonObject) &&
		JsonObject.IsValid())
	{
		FJsonSerializerReader Serializer(JsonObject);
		Serialize(Serializer, false);
		return true;
	}
	UE_LOG(LogJson, Warning, TEXT("Failed to parse Json from a string: %s"), *JsonReader->GetErrorMessage());
	return false;
}

bool FJsonSerializable::FromJson(TSharedPtr<FJsonObject> JsonObject)
{
	if (JsonObject.IsValid())
	{
		FJsonSerializerReader Serializer(JsonObject);
		Serialize(Serializer, false);
		return true;
	}
	return false;
}
