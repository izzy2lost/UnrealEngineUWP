// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "NNEOnnxruntime.h"
#include "NNERuntimeFormat.h"
#include "NNETypes.h"

namespace UE::NNERuntimeORT::Private
{
	namespace OrtHelper
	{
		TArray<uint32> GetShape(const Ort::Value& OrtTensor);

		bool OptimizeModel(FNNEModelRaw& Model, ENNEInferenceFormat OutFormat);
	}

	struct TypeInfoORT
	{
		ENNETensorDataType DataType = ENNETensorDataType::None;
		uint64 ElementSize = 0;
	};

	TypeInfoORT TranslateTensorTypeORTToNNE(ONNXTensorElementDataType OrtDataType);

} // UE::NNERuntimeORT::Private