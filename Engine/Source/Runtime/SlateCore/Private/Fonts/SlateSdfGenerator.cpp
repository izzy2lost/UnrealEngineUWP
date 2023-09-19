// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateSdfGenerator.h"
#include "Async/AsyncWork.h"
#include "Math/Vector2D.h"
#include "Math/IntPoint.h"
#include "Containers/MpscQueue.h"
#include "Containers/StaticArray.h"
#include "HAL/IConsoleManager.h"
#include "SlateFontRenderer.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Stats/StatsMisc.h"
#include "ThirdParty/msdfgen/msdfgen.h"

// Disable msdfgen FreeType features if FreeType is not available
#if defined(MSDFGEN_USE_FREETYPE) && !WITH_FREETYPE
#undef MSDFGEN_USE_FREETYPE
#endif

// Including source file directly, as seen in dlmalloc third-party dependency
#include "ThirdParty/msdfgen/msdfgen.cpp"

CSV_DECLARE_CATEGORY_MODULE_EXTERN(SLATECORE_API, Slate);

#define MAX_GLYPH_SDF_SIDE 4096

namespace
{

// Corners with an angle greater than 3 radians (~171 degrees) won't be treated as corners.
constexpr double SDF_CORNER_ANGLE_THRESHOLD = 3.0;

// When a corner's angle tends towards zero, the size of its miter tends toward infinity. The miter limit filters extreme cases from being included in bounds.
constexpr double SDF_BOUNDS_MITER_LIMIT = 1.0;

constexpr uint32 SDF_TASK_POOL_SIZE = 64;

} // namespace

namespace SdfUtils
{

/** Converts FreeType's units-per-em to MSDFgen's units-per-em, due to the issue msdfgen#147 */
float GetMsdfgenUnitsPerEm(const uint16 InFreetypeUnitsPerEm)
{
	return 1.f/64.f*static_cast<float>(InFreetypeUnitsPerEm);
}

/**
 * Specifies how the glyph's shape geometry will be mapped into the distance field's pixel coordinate system
 * and how the distance values are mapped to the 0 - 255 range.
 */
class FGlyphSdfMapping
{
public:
	/** Computes planar mapping for the minimum pixel-aligned bounding box for the glyph and the outside portion of its spread (non-mitered), origin stays at pixel boundaries */
	void WrapNonMitered(
		const msdfgen::Shape::Bounds& InMsdfgenBounds,
		const uint16 InUnitsPerEm,
		const int32 InPpem,
		const float InEmOuterSpread)
	{
		Wrap(nullptr, InMsdfgenBounds, InUnitsPerEm, InPpem, InEmOuterSpread, 0.0);
	}

	/** Computes planar mapping for the minimum pixel-aligned bounding box for the glyph and the outside portion of its spread (mitered), origin stays at pixel boundaries */
	void WrapMitered(const msdfgen::Shape& MsdfgenShape,
		const msdfgen::Shape::Bounds& InMsdfgenBounds,
		const uint16 InUnitsPerEm,
		const int32 InPpem,
		const float InEmOuterSpread,
		const double InMiterLimit)
	{
		Wrap(&MsdfgenShape, InMsdfgenBounds, InUnitsPerEm, InPpem, InEmOuterSpread, InMiterLimit);
	}

	void SetSpread(
		const uint16 InUnitsPerEm,
		const float InEmOuterSpread,
		const float InEmInnerSpread);

	const msdfgen::Projection& GetMsdfgenProjection() const
	{
		return MsdfgenProjection;
	}

	int32 GetSdfWidth() const
	{
		return SdfBounds.Width();
	}
	int32 GetSdfHeight() const
	{
		return SdfBounds.Height();
	}

	int32 GetBearingX() const
	{
		return SdfBounds.Min.X;
	}
	int32 GetBearingY() const
	{
		return SdfBounds.Max.Y;
	}

	FORCEINLINE uint8 EncodeDistance(const float InMsdfgenUnitDistance) const
	{
		return msdfgen::pixelFloatToByte(DistanceFactor*InMsdfgenUnitDistance+DistanceBias);
	}

private:
	msdfgen::Projection MsdfgenProjection;
	FIntRect SdfBounds;
	float DistanceFactor;
	float DistanceBias;

	void Wrap(const msdfgen::Shape* MsdfgenShape,
		msdfgen::Shape::Bounds MsdfgenBounds,
		const uint16 InUnitsPerEm,
		const int32 InPpem,
		const float InEmOuterSpread,
		const double InMiterLimit);

};

void FGlyphSdfMapping::Wrap(const msdfgen::Shape* MsdfgenShape,
	msdfgen::Shape::Bounds MsdfgenBounds,
	const uint16 InUnitsPerEm,
	const int32 InPpem,
	const float InEmOuterSpread,
	const double InMiterLimit)
{
	const float MsdfgenUnitsPerEm = GetMsdfgenUnitsPerEm(InUnitsPerEm);
	const float MsdfgenScale = static_cast<float>(InPpem)/MsdfgenUnitsPerEm;

	// Add outer portion of spread to bounds
	const float MsdfgenOuterSpread = MsdfgenUnitsPerEm*InEmOuterSpread;
	MsdfgenBounds.l -= MsdfgenOuterSpread;
	MsdfgenBounds.b -= MsdfgenOuterSpread;
	MsdfgenBounds.r += MsdfgenOuterSpread;
	MsdfgenBounds.t += MsdfgenOuterSpread;

	// Find mitered bounds including outer portion of spread
	if (MsdfgenShape && InMiterLimit > 0)
	{
		MsdfgenShape->boundMiters(MsdfgenBounds.l, MsdfgenBounds.b, MsdfgenBounds.r, MsdfgenBounds.t, MsdfgenOuterSpread, InMiterLimit, 1);
	}

	// Convert to pixel bounds
	MsdfgenBounds.l *= MsdfgenScale;
	MsdfgenBounds.b *= MsdfgenScale;
	MsdfgenBounds.r *= MsdfgenScale;
	MsdfgenBounds.t *= MsdfgenScale;

	// Add 0.5 px to pixel bounds to make sure that spread does not extend beyond edge pixel centers
	MsdfgenBounds.l -= 0.5;
	MsdfgenBounds.b -= 0.5;
	MsdfgenBounds.r += 0.5;
	MsdfgenBounds.t += 0.5;

	// Normalize bounds if they are empty
	if (MsdfgenBounds.l > MsdfgenBounds.r)
	{
		MsdfgenBounds.l = 0;
		MsdfgenBounds.r = 0;
	}
	if (MsdfgenBounds.b > MsdfgenBounds.t)
	{
		MsdfgenBounds.b = 0;
		MsdfgenBounds.t = 0;
	}

	// Enlarge pixel bounds to nearest whole number
	SdfBounds.Min.X = FMath::FloorToInt32(MsdfgenBounds.l);
	SdfBounds.Min.Y = FMath::FloorToInt32(MsdfgenBounds.b);
	SdfBounds.Max.X = FMath::CeilToInt32(MsdfgenBounds.r);
	SdfBounds.Max.Y = FMath::CeilToInt32(MsdfgenBounds.t);

	MsdfgenProjection = msdfgen::Projection(
		msdfgen::Vector2(MsdfgenScale),
		msdfgen::Vector2(-SdfBounds.Min.X/MsdfgenScale, -SdfBounds.Min.Y/MsdfgenScale));
}

void FGlyphSdfMapping::SetSpread(
	const uint16 InUnitsPerEm,
	const float InEmOuterSpread,
	const float InEmInnerSpread)
{
	DistanceFactor = 1.f/(GetMsdfgenUnitsPerEm(InUnitsPerEm)*(InEmOuterSpread+InEmInnerSpread));
	DistanceBias = (
		InEmOuterSpread/(InEmOuterSpread+InEmInnerSpread)
		- 0.5f*DistanceFactor); // this part makes sure subtracts 0.5 at the very beginning, which is implicitly added by MSDFgen
}

/*
*
*/

#if WITH_FREETYPE

class FFreeTypeShapeBuilder
{
public:
	FFreeTypeShapeBuilder();

	bool Build(TSharedPtr<FFreeTypeFace> InFace,
		uint32 InGlyphIndex,
		float InEmOuterSpread,
		float InEmInnerSpread,
		int32 InPpem,
		msdfgen::Shape& OutMsdfgenShape,
		FGlyphSdfMapping& OutGlyphSdfMapping);

};

FFreeTypeShapeBuilder::FFreeTypeShapeBuilder()
{

}

bool FFreeTypeShapeBuilder::Build(TSharedPtr<FFreeTypeFace> InFace,
	uint32 InGlyphIndex, 
	float InEmOuterSpread,
	float InEmInnerSpread,
	int32 InPpem,
	msdfgen::Shape& OutMsdfgenShape,
	FGlyphSdfMapping& OutGlyphSdfMapping)
{
	if (!FreeTypeUtils::IsFaceEligibleForSdf(InFace->GetFace())) {
		return false;
	}

	FT_Error Error = ::FT_Load_Glyph(InFace->GetFace(), 
									 InGlyphIndex, 
									 FT_LOAD_NO_SCALE
									 | FT_LOAD_IGNORE_TRANSFORM
									 | FT_LOAD_NO_HINTING
									 | FT_LOAD_NO_AUTOHINT
									 | FT_LOAD_NO_BITMAP);
	if (Error != 0)
	{
		return false;
	}
	if (!FreeTypeUtils::IsGlyphEligibleForSdf(InFace->GetFace()->glyph)
		|| InFace->GetFace()->glyph->metrics.width <= 0
		|| InFace->GetFace()->glyph->metrics.height <= 0
		|| InFace->GetFace()->glyph->outline.n_points <= 0)
	{
		return false;
	}

	Error = msdfgen::readFreetypeOutline(OutMsdfgenShape, &InFace->GetFace()->glyph->outline);
	if (Error != 0 || OutMsdfgenShape.contours.empty())
	{
		return false;
	}

	msdfgen::edgeColoringInkTrap(OutMsdfgenShape, SDF_CORNER_ANGLE_THRESHOLD);

	// Find shape's geometry tight bounding box
	msdfgen::Shape::Bounds Bounds = OutMsdfgenShape.getBounds();

	// Check if shape is inverted and fix it
	msdfgen::Point2 OuterPoint(Bounds.l-(Bounds.r-Bounds.l)-1, Bounds.b-(Bounds.t-Bounds.b)-1);
	if (msdfgen::SimpleTrueShapeDistanceFinder::oneShotDistance(OutMsdfgenShape, OuterPoint) > 0)
	{
		for (msdfgen::Contour& Contour : OutMsdfgenShape.contours)
			Contour.reverse();
	}

	const uint16 UnitsPerEm = InFace->GetFace()->units_per_EM;
	// Compute glyph placement within the distance field bitmap
	OutGlyphSdfMapping.WrapMitered(OutMsdfgenShape, Bounds, UnitsPerEm, InPpem, InEmOuterSpread, SDF_BOUNDS_MITER_LIMIT);
	// Compute distance conversion to 8-bit color channel value
	OutGlyphSdfMapping.SetSpread(UnitsPerEm, InEmOuterSpread, InEmInnerSpread);

	if (!(OutGlyphSdfMapping.GetSdfWidth() > 0 && OutGlyphSdfMapping.GetSdfHeight() > 0 && OutGlyphSdfMapping.GetSdfWidth() < MAX_GLYPH_SDF_SIDE && OutGlyphSdfMapping.GetSdfHeight() < MAX_GLYPH_SDF_SIDE))
	{
		return false;
	}

	return true;
}

#endif // WITH_FREETYPE

/**
 * Performs the construction of multi-channel signed distance field for a single glyph.
 */
class alignas(PLATFORM_CACHE_LINE_SIZE) FSdfGeneratorTask
{
	FSlateSdfGenerator::FRequestDescriptor Descriptor;
	TArray<uint8> OutputPixels;
	msdfgen::Shape MsdfgenShape;
	FGlyphSdfMapping GlyphSdfMapping;
	#if SLATE_CSV_TRACKER
	FString DebugName;
	#endif

public:
	FSdfGeneratorTask()
	{
	}

	TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FSdfGeneratorTask, STATGROUP_ThreadPoolAsyncTasks);
	}

	void DoWork();
	bool CanAbandon()
	{
		return true;
	}
	void Abandon()
	{
	}

	void DoOutlineDecomposition();

	void BeginCsvTrace(TSharedPtr<FFreeTypeFace> InFace, uint32 InGlyphIndex);
	void EndCsvTrace();

	bool Begin(const FSlateSdfGenerator::FRequestDescriptor& InDescriptor, FSlateSdfGenerator::FRequestOutputInfo& OutOutputInfo);
	void End(const FSlateSdfGenerator::FForEachRequestDoneCallback& Callback);
	void Reset();

};

bool FSdfGeneratorTask::Begin(const FSlateSdfGenerator::FRequestDescriptor& InDescriptor, 
	FSlateSdfGenerator::FRequestOutputInfo& OutOutputInfo)
{
	OutOutputInfo.bGlyphUnavailable = true;
	#if WITH_FREETYPE
	TSharedPtr<FFreeTypeFace> FontFace = InDescriptor.FontFace.Pin();
	if (FontFace && FontFace->IsFaceLoading())
	{
		return false; // try again later
	}
	if (FontFace && FontFace->IsFaceValid())
	{
		if (SdfUtils::FFreeTypeShapeBuilder().Build(FontFace,
			InDescriptor.GlyphIndex,
			InDescriptor.EmOuterSpread,
			InDescriptor.EmInnerSpread,
			InDescriptor.Ppem,
			MsdfgenShape,
			GlyphSdfMapping))
		{
			Descriptor = InDescriptor;

			OutOutputInfo.ImageWidth = GlyphSdfMapping.GetSdfWidth();
			OutOutputInfo.ImageHeight = GlyphSdfMapping.GetSdfHeight();
			OutOutputInfo.BearingX = GlyphSdfMapping.GetBearingX();
			OutOutputInfo.BearingY = GlyphSdfMapping.GetBearingY();
			OutOutputInfo.bGlyphUnavailable = false;

			BeginCsvTrace(FontFace, InDescriptor.GlyphIndex);
		}
	}
	#endif
	return true;
}

void FSdfGeneratorTask::End(const FSlateSdfGenerator::FForEachRequestDoneCallback& Callback)
{
	EndCsvTrace();
	Callback(Descriptor, MoveTemp(OutputPixels));
	Descriptor = {};
	MsdfgenShape.contours.clear();
}

void FSdfGeneratorTask::Reset()
{
	EndCsvTrace();
	Descriptor = {};
	MsdfgenShape.contours.clear();
	OutputPixels.Empty();
}

void FSdfGeneratorTask::BeginCsvTrace(TSharedPtr<FFreeTypeFace> InFace, uint32 InGlyphIndex)
{
	#if SLATE_CSV_TRACKER
	if (DebugName.IsEmpty())
	{
		#if WITH_FREETYPE
		DebugName = FString("Sdf/");
		DebugName += FString(InFace->GetFace()->family_name ? InFace->GetFace()->family_name : "FontNotDef");
		DebugName += FString("/");
		char GlyphDebugName[16];
		GlyphDebugName[0] = GlyphDebugName[15] = '\0';
		if (FT_HAS_GLYPH_NAMES(InFace->GetFace())
			&& 0 == ::FT_Get_Glyph_Name(InFace->GetFace(), InGlyphIndex, GlyphDebugName, sizeof(GlyphDebugName))
			&& FCStringAnsi::Strnlen(GlyphDebugName, sizeof(GlyphDebugName)) > 0)
		{
			DebugName += FString(GlyphDebugName);
		}
		else
		{
			DebugName += FString::FromInt(InGlyphIndex);
		}
		#else
		DebugName = FString("Sdf/FontNotDef/");
		DebugName += FString::FromInt(InGlyphIndex);
		#endif
		FCsvProfiler::BeginStat(TCHAR_TO_ANSI(*DebugName), CSV_CATEGORY_INDEX(Slate));
	}
	#endif // SLATE_CSV_TRACKER
}

void FSdfGeneratorTask::EndCsvTrace()
{
	#if SLATE_CSV_TRACKER
	if (!DebugName.IsEmpty())
	{
		FCsvProfiler::EndStat(TCHAR_TO_ANSI(*DebugName), CSV_CATEGORY_INDEX(Slate));
		DebugName.Empty();
	}
	#endif
}

void FSdfGeneratorTask::DoWork()
{
	SCOPE_LOG_TIME("FSdfGeneratorTask", NULL);
	DoOutlineDecomposition();
}

void FSdfGeneratorTask::DoOutlineDecomposition()
{
	const bool OverlappedContourSupport = true;
	const int32 TargetWidth = GlyphSdfMapping.GetSdfWidth();
	const int32 TargetHeight = GlyphSdfMapping.GetSdfHeight();
	TArray<float> FloatPixels;
	FloatPixels.SetNumUninitialized(4 * TargetWidth * TargetHeight);
	OutputPixels.SetNumUninitialized(4 * TargetWidth * TargetHeight);

	TArray<uint8> ECBuffer;
	ECBuffer.SetNumUninitialized (TargetWidth * TargetHeight);

	msdfgen::BitmapRef<float, 4> FloatBitmap(FloatPixels.GetData(), TargetWidth, TargetHeight);
	msdfgen::generateMTSDF(
		FloatBitmap,
		MsdfgenShape,
		GlyphSdfMapping.GetMsdfgenProjection(),
		1.0,
		msdfgen::MSDFGeneratorConfig(
			OverlappedContourSupport,
			msdfgen::ErrorCorrectionConfig(
				msdfgen::ErrorCorrectionConfig::EDGE_PRIORITY,
				msdfgen::ErrorCorrectionConfig::CHECK_DISTANCE_AT_EDGE,
				msdfgen::ErrorCorrectionConfig::defaultMinDeviationRatio,
				msdfgen::ErrorCorrectionConfig::defaultMinImproveRatio,
				ECBuffer.GetData()
			)
		)
	);

	for (int32 y = 0; y < TargetHeight; ++y)
	{
		for (int32 x = 0; x < TargetWidth; ++x)
		{
			uint8 *const OutputPixel = OutputPixels.GetData() + (4 * ((TargetHeight - y - 1) * TargetWidth + x));
			OutputPixel[0] = GlyphSdfMapping.EncodeDistance(FloatBitmap(x, y)[0]);
			OutputPixel[1] = GlyphSdfMapping.EncodeDistance(FloatBitmap(x, y)[1]);
			OutputPixel[2] = GlyphSdfMapping.EncodeDistance(FloatBitmap(x, y)[2]);
			OutputPixel[3] = GlyphSdfMapping.EncodeDistance(FloatBitmap(x, y)[3]);
		}
	}
}

} // namespace SdfUtils

  /*
  *
  */

class FSlateSdfGeneratorImpl : public FSlateSdfGenerator
{
public:
	FSlateSdfGeneratorImpl();
	~FSlateSdfGeneratorImpl();

	virtual bool Spawn(const FRequestDescriptor& InRequest, FRequestOutputInfo& OutCharInfo) override;
	virtual void Update(const FForEachRequestDoneCallback& InEnumerator) override;
	virtual void Flush() override;
private:
	TArray<FAsyncTask<SdfUtils::FSdfGeneratorTask>*, TFixedAllocator<SDF_TASK_POOL_SIZE>> FreeTasks;
	TArray<FAsyncTask<SdfUtils::FSdfGeneratorTask>*, TFixedAllocator<SDF_TASK_POOL_SIZE>> StartedTasks;
	TArray<FAsyncTask<SdfUtils::FSdfGeneratorTask>, TAlignedHeapAllocator<PLATFORM_CACHE_LINE_SIZE>> TasksPool;
};


FSlateSdfGeneratorImpl::FSlateSdfGeneratorImpl()
{
	FreeTasks.Reserve(SDF_TASK_POOL_SIZE);
	StartedTasks.Reserve(SDF_TASK_POOL_SIZE);
	TasksPool.Reserve(SDF_TASK_POOL_SIZE);
	TasksPool.AddDefaulted(SDF_TASK_POOL_SIZE);
	int32 Count = 0;
	while (Count < TasksPool.Num())
	{
		FreeTasks.Push(&TasksPool[Count]);
		++Count;
	}
}

FSlateSdfGeneratorImpl::~FSlateSdfGeneratorImpl()
{
	Flush();
	FreeTasks.Empty();
	TasksPool.Empty();
}

bool FSlateSdfGeneratorImpl::Spawn(const FRequestDescriptor& InRequest, FRequestOutputInfo& OutCharInfo)
{
	if (FreeTasks.IsEmpty())
	{
		return false;
	}
	FAsyncTask<SdfUtils::FSdfGeneratorTask>* Task = FreeTasks.Pop(false);
	const bool bOutputInfoValid = Task->GetTask().Begin(InRequest, OutCharInfo);
	if (bOutputInfoValid && !OutCharInfo.bGlyphUnavailable)
	{
		StartedTasks.Push(Task);
		Task->StartBackgroundTask();
	}
	else
	{
		FreeTasks.Push(Task);
	}
	return bOutputInfoValid;
}

void FSlateSdfGeneratorImpl::Update(const FForEachRequestDoneCallback& InEnumerator)
{
	for (auto It = StartedTasks.CreateIterator(); It; ++It)
	{
		if ((*It)->IsDone())
		{
			(*It)->GetTask().End(InEnumerator);
			FreeTasks.Push(*It);
			It.RemoveCurrentSwap();
		}
	}
}

void FSlateSdfGeneratorImpl::Flush()
{	
	for (auto It = StartedTasks.CreateIterator(); It; ++It)
	{
		if ((*It)->Cancel() || (*It)->IsDone())
		{
			(*It)->GetTask().Reset();
			FreeTasks.Push(*It);
			It.RemoveCurrentSwap();
		}
	}
	for (auto It = StartedTasks.CreateIterator(); It; ++It)
	{
		(*It)->EnsureCompletion(false);
		(*It)->GetTask().Reset();
		FreeTasks.Push(*It);
	}
	StartedTasks.Empty(SDF_TASK_POOL_SIZE);
	check(FreeTasks.Num() == SDF_TASK_POOL_SIZE);
}

FSlateSdfGenerator::FSlateSdfGenerator()
{
	// no inline
}

FSlateSdfGenerator::~FSlateSdfGenerator()
{
	// no inline
}

FSlateSdfGenerator::Ptr FSlateSdfGenerator::create()
{
	return MakeUnique<FSlateSdfGeneratorImpl>();
}
