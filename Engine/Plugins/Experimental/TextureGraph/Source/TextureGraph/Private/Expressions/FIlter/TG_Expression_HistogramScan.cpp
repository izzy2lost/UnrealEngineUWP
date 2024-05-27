// Copyright Epic Games, Inc. All Rights Reserved.

#include "Expressions/Filter/TG_Expression_HistogramScan.h"
#include "Expressions/Filter/TG_Expression_Levels.h"
#include "FxMat/MaterialManager.h"
#include "Job/JobBatch.h"
#include "Transform/Expressions/T_Color.h"

void UTG_Expression_HistogramScan::Evaluate(FTG_EvaluationContext* InContext)
{
	Super::Evaluate(InContext);

	if (!Input)
	{
		Output = FTG_Texture::GetBlack();
		return;
	}

	BufferDescriptor SourceDesc = Input.GetBufferDescriptor();
	BufferDescriptor DesiredDesc = BufferDescriptor::Combine(Output.GetBufferDescriptor(), SourceDesc);
	DesiredDesc.ItemsPerPoint = 4;

	const RenderMaterial_FXPtr RenderMaterial = TextureGraphEngine::GetMaterialManager()->CreateMaterial_FX<VSH_Simple, FSH_Levels>(TEXT("T_Levels"));

	check(RenderMaterial);

	FTG_LevelsSettings Levels;

	float C = Contrast * 0.5f;
	float P = 1.0f - FMath::Clamp(Position, 0, 1);
	float P1 = (FMath::Max(P, 0.5f) - 0.5f) * 2.0f;
	float P2 = FMath::Min(P * 2.0f, 1.0f);

	Levels.Low = FMath::Lerp(P1, P2, C);
	Levels.High = FMath::Lerp(P2, P1, C);
	Levels.Mid = Levels.Low + (Levels.High - Levels.Low) * 0.5f;

	JobUPtr RenderJob = std::make_unique<Job>(InContext->Cycle->GetMix(), InContext->TargetId, std::static_pointer_cast<BlobTransform>(RenderMaterial));

	RenderJob
		->AddArg(ARG_BLOB(Input, "Input"))
		->AddArg(ARG_FLOAT(Levels.Low, "MinValue"))
		->AddArg(ARG_FLOAT(Levels.High, "MaxValue"))
		->AddArg(ARG_FLOAT(Levels.EvalMidExponent(), "Gamma"))
		;

	const FString Name = TEXT("HistogramScan"); 
	BufferDescriptor Desc = Output.GetBufferDescriptor();

	Output = RenderJob->InitResult(Name, &Desc);
	InContext->Cycle->AddJob(InContext->TargetId, std::move(RenderJob));
}
