// Copyright Epic Games, Inc. All Rights Reserved.
#include "T_TextureHistogram.h"
#include "MixerEngine.h"
#include "Job/JobArgs.h"
#include "MixerEngine.h"
#include "Job/HistogramService.h"
#include "Job/JobBatch.h"
#include "Job/Scheduler.h"
#include "Helper/MathUtils.h"
#include "Helper/GraphicsUtil.h"
#include "Device/FX/DeviceBuffer_FX.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Device/DeviceManager.h"
#include "Device/Mem/Device_Mem.h"
#include "Data/Blobber.h"
#include "2D/Tex.h"
#include "FxMat/MaterialManager.h"
#include "T_MinMax.h"

IMPLEMENT_GLOBAL_SHADER(CSH_Histogram, "/Plugin/TextureGraph/Utils/Histogram_comp.usf", "CSH_Histogram", SF_Compute);

static constexpr int NumBins = 256;

T_TextureHistogram::T_TextureHistogram()
{
}

T_TextureHistogram::~T_TextureHistogram()
{
}

RenderMaterial_FXPtr T_TextureHistogram::CreateMaterial_Histogram(FString Name, FString OutputId, const CSH_Histogram::FPermutationDomain& cmpshPermutationDomain,
	int NumThreadsX, int NumThreadsY, int NumThreadsZ)
{
	std::shared_ptr<FxMaterial_Histogram<CSH_Histogram>> Mat = std::make_shared<FxMaterial_Histogram<CSH_Histogram>>(OutputId, &cmpshPermutationDomain, NumThreadsX, NumThreadsY, NumThreadsZ);
	return std::make_shared<RenderMaterial_FX>(Name, std::static_pointer_cast<FxMaterial>(Mat));
}

TiledBlobPtr T_TextureHistogram::Create(UMixInterface* InMix, TiledBlobPtr SourceTex, int32 TargetId)
{
	if (SourceTex->HasHistogram())
	{
		TiledBlobPtr SourceHistogram = std::static_pointer_cast<TiledBlob>(SourceTex->GetHistogram());
		return SourceHistogram;
	}

	check(SourceTex);
	HistogramServicePtr Service = MixerEngine::GetScheduler()->GetHistogramService().lock();
	check(Service);

	JobBatchPtr Batch = Service->GetOrCreateNewBatch(InMix);

	CSH_Histogram::FPermutationDomain PermutationVector;

	FIntVector4 SrcDimensions(SourceTex->GetWidth(), SourceTex->GetHeight(), 1, 1);

	auto SrcTiles = std::make_shared<JobArg_Blob>(SourceTex, "SourceTiles");

	FString Name = FString::Printf(TEXT("[%s].[%d] Histogram"), *SourceTex->DisplayName(), TargetId);
	RenderMaterial_FXPtr Transform = T_TextureHistogram::CreateMaterial_Histogram(
		TEXT("T_Histogram"), TEXT("Result"), PermutationVector, SrcDimensions.X, SrcDimensions.Y, 1);

	JobUPtr JobObj = std::make_unique<Job>(InMix, TargetId, std::static_pointer_cast<BlobTransform>(Transform));
	JobObj
		->AddArg(SrcTiles);

	BufferDescriptor Desc;
	Desc.Width = NumBins;
	Desc.Height = 2;
	Desc.Format = BufferFormat::Float;// BufferFormat::Int;
	Desc.ItemsPerPoint = 4;
	Desc.Name = FString::Printf(TEXT("Histogram - %s"), *SourceTex->Name());
	Desc.AllowUAV();

	JobObj->SetTiled(false);

	TiledBlobPtr Result = JobObj->InitResult(Name, &Desc,1,1);

	Result->MakeSingleBlob();

	//Add the job using histogram idle service
	AddHistogramJobToCycle(Batch->GetCycle(),std::move(JobObj), TargetId, InMix);


	if (!SourceTex->HasHistogram())
	{
		//setting it as the histogram of source so it is retained untill the life cycle of source blob.
		SourceTex->SetHistogram(Result);
	}

	return Result;
}

void T_TextureHistogram::AddHistogramJobToCycle(MixUpdateCyclePtr Cycle, JobUPtr Job, int32 TargetId,UMixInterface* Mix)
{
	HistogramServicePtr Service = MixerEngine::GetScheduler()->GetHistogramService().lock();

	if (!Service)
		return;

	//TODO get rid of mix here and use Null Mix instead
	UMixInterface* mix = Job->GetMix();
	check(mix);

	Service->AddHistogramJob(Cycle,std::move(Job), TargetId,Mix);
}
