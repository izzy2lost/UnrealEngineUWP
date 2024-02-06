// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/TraitPtr.h"
#include "TraitCore/TraitInterfaceUID.h"
#include "TraitCore/TraitTemplate.h"

#include <type_traits>

struct FAnimNextTraitSharedData;

namespace UE::AnimNext
{
	struct FTraitInstanceData;
	struct ITraitInterface;
	struct FNodeDescription;

	/**
	 * FTraitBinding
	 * 
	 * Base class for all trait bindings.
	 * A trait binding contains untyped data about a specific trait instance.
	 */
	struct ANIMNEXT_API FTraitBinding
	{
		// Creates an empty binding.
		FTraitBinding() = default;

		// Returns whether or not this binding is valid.
		bool IsValid() const { return TraitTemplate != nullptr; }

		// Queries a node for a pointer to its trait shared data.
		// If the trait handle is invalid, a null pointer is returned.
		template<class SharedDataType>
		const SharedDataType* GetSharedData() const
		{
			static_assert(std::is_base_of<FAnimNextTraitSharedData, SharedDataType>::value, "Trait shared data must derive from FAnimNextTraitSharedData");

			if (!IsValid())
			{
				return nullptr;
			}

			return static_cast<const SharedDataType*>(TraitTemplate->GetTraitDescription(*NodeDescription));
		}

		// Queries a node for a pointer to its trait instance data.
		// If the trait handle is invalid, a null pointer is returned.
		template<class InstanceDataType>
		InstanceDataType* GetInstanceData() const
		{
			static_assert(std::is_base_of<FTraitInstanceData, InstanceDataType>::value, "Trait instance data must derive from FTraitInstanceData");

			if (!IsValid())
			{
				return nullptr;
			}

			return static_cast<InstanceDataType*>(TraitTemplate->GetTraitInstance(*TraitPtr.GetNodeInstance()));
		}

		// Queries a node for a pointer to its trait latent properties.
		// If the trait handle is invalid or if we have no latent properties, a null pointer is returned.
		const FLatentPropertyHandle* GetLatentPropertyHandles() const
		{
			if (!IsValid() || !TraitTemplate->HasLatentProperties())
			{
				return nullptr;
			}

			return TraitTemplate->GetTraitLatentPropertyHandles(*NodeDescription);
		}

		// Returns a pointer to the latent property specified by the provided handle or nullptr if the binding/handle are invalid
		template<typename PropertyType>
		const PropertyType* GetLatentProperty(FLatentPropertyHandle Handle) const
		{
			if (!IsValid())
			{
				return nullptr;
			}

			if (!Handle.IsOffsetValid())
			{
				return nullptr;
			}

			const uint8* NodeInstance = (const uint8*)TraitPtr.GetNodeInstance();
			return (const PropertyType*)(NodeInstance + Handle.GetLatentPropertyOffset());
		}

		// Returns the trait pointer we are bound to.
		FWeakTraitPtr GetTraitPtr() const { return TraitPtr; }

		// Returns the trait interface UID when bound, an invalid UID otherwise.
		FTraitInterfaceUID GetInterfaceUID() const;

		// Equality and inequality tests
		bool operator==(const FTraitBinding& RHS) const { return TraitPtr == RHS.TraitPtr && Interface == RHS.Interface; }
		bool operator!=(const FTraitBinding& RHS) const { return TraitPtr != RHS.TraitPtr || Interface != RHS.Interface; }

	protected:
		// Creates a valid binding
		FTraitBinding(const ITraitInterface* InInterface, const FTraitTemplate* InTraitTemplate, const FNodeDescription* InNodeDescription, FWeakTraitPtr InTraitPtr)
			: Interface(InInterface)
			, TraitTemplate(InTraitTemplate)
			, NodeDescription(InNodeDescription)
			, TraitPtr(InTraitPtr)
		{}

		// Performs a naked cast to the desired interface type
		template<class TraitInterfaceType>
		const TraitInterfaceType* GetInterfaceTyped() const
		{
			static_assert(std::is_base_of<ITraitInterface, TraitInterfaceType>::value, "Trait interface data must derive from ITraitInterface");
			return static_cast<const TraitInterfaceType*>(Interface);
		}

		// A pointer to the bound interface or nullptr if we are bound to a trait but none of its interfaces
		const ITraitInterface*				Interface = nullptr;

		// A pointer to the trait template that implements the interface we are bound to or nullptr if we are invalid
		const FTraitTemplate*				TraitTemplate = nullptr;

		// A pointer to the node shared data we are bound to
		const FNodeDescription*				NodeDescription = nullptr;

		// A weak handle to the trait instance data we are bound to
		FWeakTraitPtr						TraitPtr;

		friend struct FExecutionContext;
	};

	/**
	 * TTraitBinding
	 * 
	 * A templated proxy for trait interfaces. It is meant to be specialized per interface
	 * in order to allow a clean API and avoid human error. It wraps the necessary information
	 * to bind a trait to a specific interface. See existing interfaces for examples.
	 * 
	 * Here, we forward declare the template which every interface must specialize. Because we
	 * rely on specialization, it must be defined within the UE::AnimNext namespace where the
	 * declaration exists.
	 * 
	 * Specializations must derive from FTraitBinding to provide the necessary machinery.
	 * 
	 * @see IUpdate, IEvaluate, IHierarchy
	 */
	template<class TraitInterfaceType>
	struct TTraitBinding;
}
