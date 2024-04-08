// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IDatasmithMeshElement;
class IDatasmithScene;

struct FDatasmithTessellationOptions;
struct FDatasmithMeshElementPayload;

typedef TFunction<TSharedPtr<class IWireInterface>()> FInterfaceMaker;

class IWireInterface
{
public:
	virtual ~IWireInterface() {}

	virtual bool Initialize(const TCHAR* Filename = nullptr) = 0;

	virtual bool Load(TSharedPtr<IDatasmithScene> Scene) = 0;

	virtual void SetTessellationOptions(const FDatasmithTessellationOptions& Options) = 0;
	virtual void SetOutputPath(const FString& Path) = 0;
	virtual bool LoadStaticMesh(const TSharedPtr<IDatasmithMeshElement> MeshElement, FDatasmithMeshElementPayload& OutMeshPayload, const FDatasmithTessellationOptions& InTessellationOptions) = 0;

	static DATASMITHWIRETRANSLATOR_API uint64 GetRequiredAliasVersion();
	static DATASMITHWIRETRANSLATOR_API void RegisterInterface(int16 MajorVersion, int16 MinorVersion, FInterfaceMaker&& MakeInterface);
};
