// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGWorldQueryHelpers.h"

#include "PCGComponent.h"
#include "Data/PCGPointData.h"
#include "Data/PCGWorldData.h"
#include "Helpers/PCGHelpers.h"

#include "LandscapeProxy.h"
#include "Components/BrushComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "Engine/OverlapResult.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/SoftObjectPath.h"

namespace PCGWorldQueryHelpers
{
	FTransform GetOrthonormalImpactTransform(const FHitResult& Hit)
	{
		// Implementation note: this uses the same orthonormalization process as the landscape cache
		ensure(Hit.ImpactNormal.IsNormalized());
		const FVector ArbitraryVector = (FMath::Abs(Hit.ImpactNormal.Y) < (1.f - UE_KINDA_SMALL_NUMBER) ? FVector::YAxisVector : FVector::ZAxisVector);
		const FVector XAxis = (ArbitraryVector ^ Hit.ImpactNormal).GetSafeNormal();
		const FVector YAxis = (Hit.ImpactNormal ^ XAxis);

		return FTransform(XAxis, YAxis, Hit.ImpactNormal, Hit.ImpactPoint);
	}

	bool FilterCommonQueryResults(
		const FPCGWorldCommonQueryParams* QueryParams,
		const UPrimitiveComponent* TriggeredComponent,
		const TWeakObjectPtr<UPCGComponent> OriginatingComponent)
	{
		check(QueryParams && TriggeredComponent);

		// Skip invisible walls / triggers / volumes
		if (TriggeredComponent->IsA<UBrushComponent>())
		{
			return false;
		}

		// Skip "No collision" type actors
		if (!TriggeredComponent->IsQueryCollisionEnabled() || TriggeredComponent->GetCollisionResponseToChannel(QueryParams->CollisionChannel) != ECR_Block)
		{
			return false;
		}

		// Skip to-be-cleaned-up PCG-created objects
		if (TriggeredComponent->ComponentHasTag(PCGHelpers::MarkedForCleanupPCGTag) || (TriggeredComponent->GetOwner() && TriggeredComponent->GetOwner()->ActorHasTag(PCGHelpers::MarkedForCleanupPCGTag)))
		{
			return false;
		}

		// Optionally skip all PCG created objects
		if (QueryParams->bIgnorePCGHits && (TriggeredComponent->ComponentHasTag(PCGHelpers::DefaultPCGTag) || (TriggeredComponent->GetOwner() && TriggeredComponent->GetOwner()->ActorHasTag(PCGHelpers::DefaultPCGActorTag))))
		{
			return false;
		}

		// Skip self-generated PCG objects optionally
		if (QueryParams->bIgnoreSelfHits && OriginatingComponent.IsValid() && TriggeredComponent->ComponentTags.Contains(OriginatingComponent->GetFName()))
		{
			return false;
		}

		// Additional filter as provided in the QueryParams base class
		if (QueryParams->ActorTagFilter != EPCGWorldQueryFilterByTag::NoTagFilter)
		{
			if (AActor* Actor = TriggeredComponent->GetOwner())
			{
				bool bFoundMatch = false;
				for (const FName& Tag : Actor->Tags)
				{
					if (QueryParams->ParsedActorTagsList.Contains(Tag))
					{
						bFoundMatch = true;
						break;
					}
				}

				if (bFoundMatch != (QueryParams->ActorTagFilter == EPCGWorldQueryFilterByTag::IncludeTagged))
				{
					return false;
				}
			}
			else if (QueryParams->ActorTagFilter == EPCGWorldQueryFilterByTag::IncludeTagged)
			{
				return false;
			}
		}

		// Landscape or not, include it if this is true
		if (QueryParams->SelectLandscapeHits == EPCGWorldQuerySelectLandscapeHits::Include)
		{
			return true;
		}

		bool bTriggeredOnLandscape = TriggeredComponent->GetOwner() && TriggeredComponent->GetOwner()->IsA<ALandscapeProxy>();

		// If excluding, skip. If requiring, and it's not a landscape, skip.
		if ((bTriggeredOnLandscape && QueryParams->SelectLandscapeHits == EPCGWorldQuerySelectLandscapeHits::Exclude) ||
			(!bTriggeredOnLandscape && QueryParams->SelectLandscapeHits == EPCGWorldQuerySelectLandscapeHits::Require))
		{
			return false;
		}

		return true;
	}

	TOptional<FHitResult> FilterRayHitResults(
		const FPCGWorldRaycastQueryParams* QueryParams,
		const TWeakObjectPtr<UPCGComponent> OriginatingComponent,
		const TArray<FHitResult>& HitResults)
	{
		check(QueryParams);

		for (const FHitResult& Hit : HitResults)
		{
			const UPrimitiveComponent* HitComponent = Hit.GetComponent();

			if (!FilterCommonQueryResults(QueryParams, HitComponent, OriginatingComponent))
			{
				continue;
			}

			// Optionally skip backface hits
			if (QueryParams->bIgnoreBackfaceHits)
			{
				// If it's a landscape, we cull if the normal is negative in Z direction (landscape normal is always the +Z axis). If not, then we cull if the impact normal and the ray are headed in the same direction
				if (Hit.bStartPenetrating || (Hit.GetActor() && Hit.GetActor()->IsA<ALandscapeProxy>() && Hit.ImpactNormal.Z < 0) || (Hit.TraceEnd - Hit.TraceStart).Dot(Hit.ImpactNormal) > 0)
				{
					continue;
				}
			}

			return Hit;
		}

		return {};
	}

	TOptional<FOverlapResult> FilterOverlapResults(
		const FPCGWorldVolumetricQueryParams* QueryParams,
		const TWeakObjectPtr<UPCGComponent> OriginatingComponent,
		const TArray<FOverlapResult>& OverlapResults)
	{
		check(QueryParams);

		for (const FOverlapResult& Overlap : OverlapResults)
		{
			const UPrimitiveComponent* OverlappedComponent = Overlap.GetComponent();

			if (!FilterCommonQueryResults(QueryParams, OverlappedComponent, OriginatingComponent))
			{
				continue;
			}

			return Overlap;
		}

		return {};
	}

	// TODO: Add an option to create and apply ranges
	bool ApplyRayHitMetadata(
		const TOptional<FHitResult>& HitResult,
		const FPCGWorldRaycastQueryParams& QueryParams,
		FPCGPoint& OutPoint,
		UPCGMetadata* OutMetadata,
		bool bShouldCreateAttributes)
	{
		if (!OutMetadata)
		{
			return false;
		}

		// Check for the attributes are added, if not, then add them all in one go
		auto CreateAttribute = [OutMetadata]<typename Type>(FName AttributeName, bool bShouldCreate, const Type& DefaultValue)
		{
			if (bShouldCreate && !OutMetadata->HasAttribute(AttributeName))
			{
				OutMetadata->CreateAttribute<Type>(AttributeName, DefaultValue, /*bAllowsInterpolation=*/true, /*bOverrideParent=*/false);
			}
		};

		// TODO: Create a new helper to create attributes outside the point loops
		if (bShouldCreateAttributes)
		{
			CreateAttribute(PCGWorldQueryConstants::ImpactAttribute, QueryParams.bGetImpact, false);
			CreateAttribute(PCGWorldQueryConstants::DistanceAttribute, QueryParams.bGetDistance, 0.0);
			CreateAttribute(PCGWorldQueryConstants::ImpactNormalAttribute, QueryParams.bGetImpactNormal, FVector());
			CreateAttribute(PCGPointDataConstants::ActorReferenceAttribute, QueryParams.bGetReferenceToActorHit, FSoftObjectPath());
			CreateAttribute(PCGWorldQueryConstants::PhysicalMaterialReferenceAttribute, QueryParams.bGetReferenceToPhysicalMaterial, FSoftObjectPath());
		}

		auto ApplyAttribute = [&OutPoint, OutMetadata]<typename Type>(FName AttributeName, const Type& Value, bool bShouldApply = true)
		{
			if (!bShouldApply)
			{
				return true;
			}

			if (FPCGMetadataAttribute<Type>* Attribute = OutMetadata->GetMutableTypedAttribute<Type>(AttributeName))
			{
				OutMetadata->InitializeOnSet(OutPoint.MetadataEntry);
				Attribute->SetValue(OutPoint.MetadataEntry, Value);

				return true;
			}

			return false;
		};

		if (!HitResult.IsSet())
		{
			ApplyAttribute(PCGWorldQueryConstants::ImpactAttribute, /*Value=*/false, QueryParams.bGetImpact);
			return true;
		}

		const FHitResult& Hit = HitResult.GetValue();

		ApplyAttribute(PCGWorldQueryConstants::ImpactAttribute, /*Value=*/true, QueryParams.bGetImpact);
		ApplyAttribute(PCGWorldQueryConstants::DistanceAttribute, Hit.Distance, QueryParams.bGetDistance);
		ApplyAttribute(PCGWorldQueryConstants::ImpactNormalAttribute, FVector(Hit.ImpactNormal), QueryParams.bGetImpactNormal);
		ApplyAttribute(PCGPointDataConstants::ActorReferenceAttribute, FSoftObjectPath(Hit.GetActor()), QueryParams.bGetReferenceToActorHit);
		ApplyAttribute(PCGWorldQueryConstants::PhysicalMaterialReferenceAttribute, FSoftObjectPath(Hit.PhysMaterial.Get()), QueryParams.bGetReferenceToPhysicalMaterial);

		return true;
	}
}
