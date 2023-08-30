// Copyright Epic Games, Inc. All Rights Reserved.

export void NNERuntimeBasicCPUNormalizeLayer(
	uniform float Output[],
	uniform const float Input[],
	uniform const float Mean[],
	uniform const float Std[],
	uniform const int BatchNum,
	uniform const int InputOutputNum,
	uniform const int OutputBufferSize,
	uniform const int InputBufferSize)
{
	foreach(BatchIdx = 0 ... BatchNum, Idx = 0 ... InputOutputNum)
	{
		Output[BatchIdx * OutputBufferSize + Idx] = (Input[BatchIdx * InputBufferSize + Idx] - Mean[Idx]) / Std[Idx];
	}
}

export void NNERuntimeBasicCPUDenormalizeLayer(
	uniform float Output[],
	uniform const float Input[],
	uniform const float Mean[],
	uniform const float Std[],
	uniform const int BatchNum,
	uniform const int InputOutputNum,
	uniform const int OutputBufferSize,
	uniform const int InputBufferSize)
{
	foreach(BatchIdx = 0 ... BatchNum, Idx = 0 ... InputOutputNum)
	{
		Output[BatchIdx * OutputBufferSize + Idx] = (Input[BatchIdx * InputBufferSize + Idx] * Std[Idx]) + Mean[Idx];
	}
}

export void NNERuntimeBasicCPULinearLayer(
	uniform float Output[],
	uniform const float Input[],
	uniform const float Weights[],
	uniform const float Biases[],
	uniform const int BatchNum,
	uniform const int RowNum,
	uniform const int ColNum,
	uniform const int OutputBufferSize,
	uniform const int InputBufferSize)
{
	foreach(BatchIdx = 0 ... BatchNum, ColIdx = 0 ... ColNum)
	{
		Output[BatchIdx * OutputBufferSize + ColIdx] = Biases[ColIdx];
	}

	for (uniform int BatchIdx = 0; BatchIdx < BatchNum; BatchIdx++)
	{
		for (uniform int RowIdx = 0; RowIdx < RowNum; RowIdx++)
		{
			uniform const float Value = Input[BatchIdx * InputBufferSize + RowIdx];

			if (Value != 0.0)
			{
				foreach(ColIdx = 0 ... ColNum)
				{
					Output[BatchIdx * OutputBufferSize + ColIdx] += Value * Weights[RowIdx * ColNum + ColIdx];
				}
			}
		}
	}
}

export void NNERuntimeBasicCPUCompressedLinearLayer(
	uniform float Output[],
	uniform const float Input[],
	uniform const uint16 Weights[],
	uniform const float WeightOffsets[],
	uniform const float WeightScales[],
	uniform const float Biases[],
	uniform const int BatchNum,
	uniform const int RowNum,
	uniform const int ColNum,
	uniform const int OutputBufferSize,
	uniform const int InputBufferSize)
{
	foreach(BatchIdx = 0 ... BatchNum, ColIdx = 0 ... ColNum)
	{
		Output[BatchIdx * OutputBufferSize + ColIdx] = Biases[ColIdx];
	}

	for (uniform int BatchIdx = 0; BatchIdx < BatchNum; BatchIdx++)
	{
		for (uniform int RowIdx = 0; RowIdx < RowNum; RowIdx++)
		{
			uniform const float Value = Input[BatchIdx * InputBufferSize + RowIdx];

			if (Value != 0.0)
			{
				uniform const float Offset = WeightOffsets[RowIdx];
				uniform const float Scale = WeightScales[RowIdx];

				foreach(ColIdx = 0 ... ColNum)
				{
					Output[BatchIdx * OutputBufferSize + ColIdx] += Value * ((Scale * ((float)Weights[RowIdx * ColNum + ColIdx])) + Offset);
				}
			}
		}
	}
}

export void NNERuntimeBasicCPUMultiLinearLayer(
	uniform float Output[],
	uniform const float Input[],
	uniform const float Weights[],
	uniform const float Biases[],
	uniform const int BatchNum,
	uniform const int BlockNum,
	uniform const int RowNum,
	uniform const int ColNum,
	uniform const int OutputBufferSize,
	uniform const int InputBufferSize)
{
	foreach(BatchIdx = 0 ... BatchNum, BlockIdx = 0 ... BlockNum, ColIdx = 0 ... ColNum)
	{
		Output[BatchIdx * OutputBufferSize + BlockIdx * ColNum + ColIdx] = Biases[BlockIdx * ColNum + ColIdx];
	}

	for (uniform int BatchIdx = 0; BatchIdx < BatchNum; BatchIdx++)
	{
		for (uniform int BlockIdx = 0; BlockIdx < BlockNum; BlockIdx++)
		{
			for (uniform int RowIdx = 0; RowIdx < RowNum; RowIdx++)
			{
				uniform const float Value = Input[BatchIdx * InputBufferSize + BlockIdx * RowNum + RowIdx];

				if (Value != 0.0)
				{
					foreach (ColIdx = 0 ... ColNum)
					{
						Output[BatchIdx * OutputBufferSize + BlockIdx * ColNum + ColIdx] += Value * Weights[BlockIdx * RowNum * ColNum + RowIdx * ColNum + ColIdx];
					}
				}
			}
		}
	}
}

export void NNERuntimeBasicCPUActivationLinear(
	uniform float Output[],
	uniform const float Input[],
	uniform const int BatchNum,
	uniform const int Num,
	uniform const int OutputBufferSize,
	uniform const int InputBufferSize)
{
	foreach(BatchIdx = 0 ... BatchNum, Idx = 0 ... Num)
	{
		Output[BatchIdx * OutputBufferSize + Idx] = Input[BatchIdx * InputBufferSize + Idx];
	}
}

export void NNERuntimeBasicCPUActivationReLU(
	uniform float Output[],
	uniform const float Input[],
	uniform const int BatchNum,
	uniform const int Num,
	uniform const int OutputBufferSize,
	uniform const int InputBufferSize)
{
	foreach(BatchIdx = 0 ... BatchNum, Idx = 0 ... Num)
	{
		Output[BatchIdx * OutputBufferSize + Idx] = max(Input[BatchIdx * InputBufferSize + Idx], 0.0f);
	}
}

export void NNERuntimeBasicCPUActivationELU(
	uniform float Output[],
	uniform const float Input[],
	uniform const int BatchNum,
	uniform const int Num,
	uniform const int OutputBufferSize,
	uniform const int InputBufferSize)
{
	foreach(BatchIdx = 0 ... BatchNum, Idx = 0 ... Num)
	{
		const float Value = Input[BatchIdx * InputBufferSize + Idx];
		Output[BatchIdx * OutputBufferSize + Idx] = select(Value > 0.0f, Value, exp(Value) - 1.0f);
	}
}

inline float NNERuntimeBasicCPUTanH(const float X, const float Min = -10.0f, const float Max = 10.0f)
{
	const float Exp2X = exp(2.0f * clamp(X, Min, Max));
	return (Exp2X - 1) / (Exp2X + 1);
}

export void NNERuntimeBasicCPUActivationTanH(
	uniform float Output[],
	uniform const float Input[],
	uniform const int BatchNum,
	uniform const int Num,
	uniform const int OutputBufferSize,
	uniform const int InputBufferSize)
{
	foreach(BatchIdx = 0 ... BatchNum, Idx = 0 ... Num)
	{
		Output[BatchIdx * OutputBufferSize + Idx] = NNERuntimeBasicCPUTanH(Input[BatchIdx * InputBufferSize + Idx]);
	}
}

export void NNERuntimeBasicCPUActivationPReLU(
	uniform float Output[],
	uniform const float Input[],
	uniform const float Alpha[],
	uniform const int BatchNum,
	uniform const int Num,
	uniform const int OutputBufferSize,
	uniform const int InputBufferSize)
{
	foreach(BatchIdx = 0 ... BatchNum, Idx = 0 ... Num)
	{
		const float Value = Input[BatchIdx * InputBufferSize + Idx];
		Output[BatchIdx * OutputBufferSize + Idx] = select(Value > 0.0f, Value, Alpha[Idx] * Value);
	}
}
