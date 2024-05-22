// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDParticleActorCustomization.h"

#include "ChaosVDModule.h"
#include "ChaosVDParticleActor.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IStructureDetailsView.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "DetailsCustomizations/ChaosVDDetailsCustomizationUtils.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

FChaosVDParticleActorCustomization::FChaosVDParticleActorCustomization()
{
	AllowedCategories.Add(FChaosVDParticleActorCustomization::ParticleDataCategoryName);
	AllowedCategories.Add(FChaosVDParticleActorCustomization::GeometryCategoryName);
}

FChaosVDParticleActorCustomization::~FChaosVDParticleActorCustomization()
{
	if (CurrentObservedActor.Get())
	{
		CurrentObservedActor->OnParticleDataUpdated().Unbind();
	}
}

TSharedRef<IDetailCustomization> FChaosVDParticleActorCustomization::MakeInstance()
{
	return MakeShareable( new FChaosVDParticleActorCustomization );
}

void FChaosVDParticleActorCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FChaosVDDetailsCustomizationUtils::HideAllCategories(DetailBuilder, AllowedCategories);

	// We keep the particle data we need to visualize as a shared ptr because copying it each frame we advance/rewind to to an struct that lives in the particle actor it is not cheap.
	// Having a struct details view to which we set that pointer data each time the data in the particle is updated (meaning we assigned another ptr from the recording)
	// seems to be more expensive because it has to rebuild the entire layout from scratch.
	// So a middle ground I found is to have a Particle Data struct in this customization instance, which we add as external property. Then each time the particle data is updated we copy the data over.
	// This allow us to only perform the copy just for the particle that is being inspected and not every particle updated in that frame.

	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);
	if (SelectedObjects.Num() > 0)
	{
		//TODO: Add support for multi-selection.
		if (!ensure(SelectedObjects.Num() == 1))
		{
			UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] [%d] objects were selectioned but this customization panel only support single object selection."), ANSI_TO_TCHAR(__FUNCTION__), SelectedObjects.Num())
		}

		if (AChaosVDParticleActor* CurrentActor = CurrentObservedActor.Get())
		{
			CachedParticleData = FChaosVDParticleDataWrapper();
			CachedGeometryDataInstanceCopy = FChaosVDMeshDataInstanceState();
			CurrentActor->OnParticleDataUpdated().Unbind();
			CurrentActor = nullptr;
		}
		
		if (AChaosVDParticleActor* ParticleActor = Cast<AChaosVDParticleActor>(SelectedObjects[0]))
		{
			CurrentObservedActor = ParticleActor;
			CurrentObservedActor->OnParticleDataUpdated().BindRaw(this, &FChaosVDParticleActorCustomization::HandleParticleDataUpdated);

			HandleParticleDataUpdated();

			TSharedPtr<IPropertyHandle> InspectedDataPropertyHandlePtr;

			if (TSharedPtr<FChaosVDMeshDataInstanceHandle> SelectedGeometryInstance = ParticleActor->GetSelectedMeshInstance().Pin())
			{
				InspectedDataPropertyHandlePtr = AddExternalStructure(CachedGeometryDataInstanceCopy, DetailBuilder, GeometryCategoryName, LOCTEXT("GeometryShapeDataStructName", "Geometry Shape Data"));
			}
			else
			{
				InspectedDataPropertyHandlePtr = AddExternalStructure(CachedParticleData, DetailBuilder, ParticleDataCategoryName, LOCTEXT("ParticleDataStructName", "Particle Data"));
			}

			if (InspectedDataPropertyHandlePtr)
			{
				TSharedRef<IPropertyHandle> InspectedDataPropertyHandleRef = InspectedDataPropertyHandlePtr.ToSharedRef();
				FChaosVDDetailsCustomizationUtils::HideInvalidCVDDataWrapperProperties({&InspectedDataPropertyHandleRef, 1}, DetailBuilder);
			}
		}
	}
}

void FChaosVDParticleActorCustomization::HandleParticleDataUpdated()
{
	AChaosVDParticleActor* ParticleActor = CurrentObservedActor.Get();
	if (!ParticleActor)
	{
		CachedParticleData = FChaosVDParticleDataWrapper();
		CachedGeometryDataInstanceCopy = FChaosVDMeshDataInstanceState();
		return;
	}

	// If we have selected a mesh instance, the only data being added to the details panel is the Shape Instance data, so can just update that data here
	if (TSharedPtr<FChaosVDMeshDataInstanceHandle> SelectedGeometryInstance = ParticleActor->GetSelectedMeshInstance().Pin())
	{
		ParticleActor->VisitGeometryInstances([this, SelectedGeometryInstance](const TSharedRef<FChaosVDMeshDataInstanceHandle>& MeshDataHandle)
		{
			if (MeshDataHandle == SelectedGeometryInstance)
			{
				CachedGeometryDataInstanceCopy = MeshDataHandle->GetState();
			}
		});
	}
	else
	{
		TSharedPtr<const FChaosVDParticleDataWrapper> ParticleDataPtr = ParticleActor->GetParticleData();
		CachedParticleData = ParticleDataPtr ? *ParticleDataPtr : FChaosVDParticleDataWrapper();
	}
}

#undef LOCTEXT_NAMESPACE
