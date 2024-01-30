// Copyright Epic Games, Inc. All Rights Reserved.

#include "SVGData.h"
#include "SVGImporterUtils.h"
#include "SVGTypes.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "EditorFramework/AssetImportData.h"
#endif

void USVGData::Initialize(const FSVGDataInitializer& InInitializer)
{
	SVGFileContent = InInitializer.SVGTextBuffer;
	CreateShapes(InInitializer.Elements);

#if WITH_EDITOR
	SourceFilePath = UAssetImportData::SanitizeImportFilename(InInitializer.SourceFilename, GetOutermost());
	GenerateSVGTexture();
#endif
}

#if WITH_EDITOR
void USVGData::GenerateSVGTexture()
{
	SVGTexture = FSVGImporterUtils::CreateSVGTexture(SVGFileContent, this);
}
#endif

void USVGData::CreateShapes(const TArray<TSharedRef<FSVGBaseElement>>& InSVGElements)
{
	constexpr float MaxDistanceFromSvgSpline = 1.e-3f;
	for (TSharedPtr<FSVGBaseElement> Element : InSVGElements)
	{
		if (Element->Type != ESVGElementType::Group && Element->Type != ESVGElementType::Other)
		{
			if (!Element->IsGraphicElement())
			{
				continue;
			}

			if (const TSharedPtr<FSVGGraphicsElement> SVGGraphicElem = StaticCastSharedPtr<FSVGGraphicsElement>(Element))
			{
				if (!SVGGraphicElem->IsVisible())
				{
					continue;
				}

				// Casting to path. Paths can have multiple subpaths
				if (const TSharedPtr<FSVGPath> SVGPath = StaticCastSharedPtr<FSVGPath>(Element))
				{
					FSVGShape NewShape(SVGGraphicElem->GetStyle(), SVGPath->IsClosed()
						, SVGGraphicElem->GetFillGradient()
						, SVGGraphicElem->GetStrokeGradient());

					NewShape.SetId(SVGGraphicElem->Name);

					int32 Count = 1;

					for (const FSVGSubPath& SubPath : SVGPath->SubPaths)
					{
						if (SubPath.Elements.Num() > 1)
						{
							FString CurrName = Element->Name + FString::FromInt(Count++);

							USplineComponent* SplineComponent = NewObject<USplineComponent>(GetTransientPackage(), *CurrName);
#if WITH_EDITOR
							// We need to be able to set different arrive and leave tangents per point
							SplineComponent->bAllowDiscontinuousSpline = true;
#endif
							int Key = 0;
							SplineComponent->ClearSplinePoints(true);

							for (const FSVGPathElement& PathElement : SubPath.Elements)
							{
								FSplinePoint Point = PathElement.AsSplinePoint(Key);

								// Apply  transform
								SVGGraphicElem->GetTransform().ApplyTransformToSplinePoint(Point);

								// Apply transform from parents, if any
								TSharedPtr<FSVGGroupElement> CurrParentGroup = SVGPath->GetParentGroup();
								while (CurrParentGroup)
								{
									CurrParentGroup->GetTransform().ApplyTransformToSplinePoint(Point);
									CurrParentGroup = CurrParentGroup->GetParentGroup();
								}

								SplineComponent->AddSplineLocalPoint(Point.Position);
								SplineComponent->SetTangentsAtSplinePoint(Key, Point.ArriveTangent, Point.LeaveTangent, ESplineCoordinateSpace::Local);

								Key++;
							}

							bool bIsClosed = SubPath.bIsClosed;
							NewShape.SetIsClosed(bIsClosed);

							SplineComponent->SetClosedLoop(bIsClosed);
							SplineComponent->UpdateSpline();

							TArray<FVector> Points;
							FSVGImporterUtils::ConvertSVGSplineToPolyLine(SplineComponent, ESplineCoordinateSpace::Local, MaxDistanceFromSvgSpline, Points);

							SplineComponent->DestroyComponent();

							NewShape.AddPolygon(Points);
						}
					}

					NewShape.ApplyFillRule();
					Shapes.Add(NewShape);
				}
			}
		}
	}
}
