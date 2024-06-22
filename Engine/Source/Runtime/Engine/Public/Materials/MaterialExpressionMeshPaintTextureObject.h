// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Materials/MaterialExpression.h"

#include "MaterialExpressionMeshPaintTextureObject.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionMeshPaintTextureObject : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

protected:
	/** Initialize the output pins. */
	ENGINE_API void InitOutputs();

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	ENGINE_API virtual uint32 GetOutputType(int32 OutputIndex) override;
	ENGINE_API virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
#endif
	void GetCaption(TArray<FString>& OutCaptions) const;
	//~ End UMaterialExpression Interface
};
