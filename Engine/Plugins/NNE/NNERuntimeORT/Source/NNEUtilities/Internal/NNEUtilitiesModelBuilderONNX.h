// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNETensor.h"
#include "NNETypes.h"
#include "NNEUtilitiesModelBuilder.h"
#include "Templates/UniquePtr.h"

namespace UE::NNE { class FAttributeMap; }
struct FNNEAttributeValue;
struct FNNEModelRaw;

namespace UE::NNEUtilities::Internal
{

static constexpr int64 DefaultOnnxIrVersion = 7;
static constexpr int64 DefaultOnnxOpsetVersion = 15;

NNEUTILITIES_API TUniquePtr<IModelBuilder> CreateONNXModelBuilder(int64 IrVersion = DefaultOnnxIrVersion, int64 OpsetVersion = DefaultOnnxOpsetVersion);

NNEUTILITIES_API bool CreateONNXModelForOperator(const FString& OperatorName, bool bUseVariadicShapeForModel,
	TConstArrayView<NNE::Internal::FTensor> InInputTensors, TConstArrayView<NNE::Internal::FTensor> InOutputTensors,
	TConstArrayView<NNE::Internal::FTensor> InWeightTensors, TConstArrayView<TConstArrayView<uint8>> InWeightTensorsData,
	const UE::NNE::FAttributeMap& Attributes, FNNEModelRaw& ModelData);

} // UE::NNEUtilities::Internal

