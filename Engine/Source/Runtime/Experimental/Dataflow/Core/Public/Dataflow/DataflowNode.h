// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosLog.h"
#include "CoreMinimal.h"
#include "Dataflow/DataflowConnection.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowAnyType.h"
#include "UObject/StructOnScope.h"
#include "Dataflow/DataflowSettings.h"

#include "DataflowNode.generated.h"

class UScriptStruct;

namespace Dataflow {
	struct FNodeParameters {
		FName Name;
		UObject* OwningObject = nullptr;
	};
	class FGraph;
}



/**
* FNode
*		Base class for node based evaluation within the Dataflow graph. 
* 
* Note : Do NOT create mutable variables in the classes derived from FDataflowNode. The state
*        is stored on the FContext. The Evaluate is const to allow support for multithreaded
*        evaluation. 
*/
PRAGMA_DISABLE_DEPRECATION_WARNINGS
USTRUCT()
struct FDataflowNode
{
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	GENERATED_USTRUCT_BODY()

	friend class Dataflow::FGraph;
	friend struct FDataflowConnection;

	FGuid Guid;
	FName Name;
	Dataflow::FTimestamp LastModifiedTimestamp;

	UE_DEPRECATED(5.5, "Inputs type has changed and has been made private")
	TMap< int, FDataflowInput* > Inputs;
	TMap< int, FDataflowOutput* > Outputs;

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	bool bActive = true;

	FDataflowNode()
		: Guid(FGuid())
		, Name("Invalid")
		, LastModifiedTimestamp(Dataflow::FTimestamp::Invalid)
	{
	}

	FDataflowNode(const Dataflow::FNodeParameters& Param, FGuid InGuid = FGuid::NewGuid())
		: Guid(InGuid)
		, Name(Param.Name)
		, LastModifiedTimestamp(Dataflow::FTimestamp::Invalid)
	{
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	virtual ~FDataflowNode() { ClearInputs(); ClearOutputs(); }
	// Warning: FDataflowNodes aren't actually safe to copy/move yet. These are here to disable deprecation warnings from the implicit operators that were getting created anyways.
	// (Deleting the operators would require tagging all derived classes since this is a USTRUCT, so also not doing that here).
	FDataflowNode& operator=(const FDataflowNode&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	FGuid GetGuid() const { return Guid; }
	FName GetName() const { return Name; }
	void SetName(FName InName) { Name = InName; }
	Dataflow::FTimestamp GetTimestamp() const { return LastModifiedTimestamp;  }
	DATAFLOWCORE_API uint32 GetValueHash();

	static FName StaticType() { return FName("FDataflowNode"); }
	virtual FName GetType() const { return StaticType(); }
	virtual FName GetDisplayName() const { return ""; }
	virtual FName GetCategory() const { return ""; }
	virtual FString GetTags() const { return ""; }
	DATAFLOWCORE_API virtual FString GetToolTip();
	DATAFLOWCORE_API FString GetPinToolTip(const FName& PropertyName, const Dataflow::FPin::EDirection Direction = Dataflow::FPin::EDirection::NONE);
	DATAFLOWCORE_API FText GetPinDisplayName(const FName& PropertyName, const Dataflow::FPin::EDirection Direction = Dataflow::FPin::EDirection::NONE);
	DATAFLOWCORE_API TArray<FString> GetPinMetaData(const FName& PropertyName, const Dataflow::FPin::EDirection Direction = Dataflow::FPin::EDirection::NONE);
	virtual TArray<Dataflow::FRenderingParameter> GetRenderParameters() const { return GetRenderParametersImpl(); }
	// Copy node property values from another node
	UE_DEPRECATED(5.4, "FDataflowNode::CopyNodeProperties is deprecated.")
	DATAFLOWCORE_API void CopyNodeProperties(const TSharedPtr<FDataflowNode> CopyFromDataflowNode);

	UE_DEPRECATED(5.4, "FDataflowNode::IsDeprecated is deprecated.")
	virtual bool IsDeprecated() { return false; }
	UE_DEPRECATED(5.4, "FDataflowNode::IsExperimental is deprecated.")
	virtual bool IsExperimental() { return false; }

	//
	// Connections
	//

	DATAFLOWCORE_API TArray<Dataflow::FPin> GetPins() const;

	UE_DEPRECATED(5.5, "Use AddPins method instead")
	virtual Dataflow::FPin AddPin() { return Dataflow::FPin::InvalidPin; }
	/** Override this function to add the AddOptionPin functionality to the node's context menu. */
	virtual TArray<Dataflow::FPin> AddPins()
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		const Dataflow::FPin DeprecatedAddPin = AddPin();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		if(DeprecatedAddPin == Dataflow::FPin::InvalidPin)
		{
			return TArray<Dataflow::FPin>();
		}
		return { DeprecatedAddPin };
	}
	/** Override this function to add the AddOptionPin functionality to the node's context menu. */
	virtual bool CanAddPin() const { return false; }
	UE_DEPRECATED(5.5, "Use GetPinsToRemove method instead")
	virtual Dataflow::FPin GetPinToRemove() const { return Dataflow::FPin::InvalidPin; }
	UE_DEPRECATED(5.4, "Use GetPinsToRemove and OnPinRemoved instead.")
	virtual Dataflow::FPin RemovePin()
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return GetPinToRemove();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	/** Override this function to add the RemoveOptionPin functionality to the node's context menu. OnPinRemoved will be called in this order.*/
	virtual TArray<Dataflow::FPin> GetPinsToRemove() const
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		const Dataflow::FPin DeprecatedRemovePin = GetPinToRemove();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		if (DeprecatedRemovePin == Dataflow::FPin::InvalidPin)
		{
			return TArray<Dataflow::FPin>();
		}
		return { DeprecatedRemovePin };
	}
	/** 
	 * Override this to update any bookkeeping when a pin is being removed.
	 * This will be called before the pin is unregistered as an input.
	 */
	virtual void OnPinRemoved(const Dataflow::FPin& Pin) {}
	/** Override this function to add the RemoveOptionPin functionality to the node's context menu. */
	virtual bool CanRemovePin() const { return false; }


	DATAFLOWCORE_API bool InputSupportsType(FName Name, FName Type) const;
	DATAFLOWCORE_API bool OutputSupportsType(FName Name, FName Type) const;

	DATAFLOWCORE_API virtual void AddInput(FDataflowInput* InPtr);
	DATAFLOWCORE_API int32 GetNumInputs() const;
	DATAFLOWCORE_API TArray< FDataflowInput* > GetInputs() const;
	DATAFLOWCORE_API void ClearInputs();
	DATAFLOWCORE_API bool HasHideableInputs() const;
	DATAFLOWCORE_API bool HasHiddenInputs() const;

	DATAFLOWCORE_API FDataflowInput* FindInput(FName Name);
	DATAFLOWCORE_API FDataflowInput* FindInput(const Dataflow::FConnectionKey& Key);
	/**This version can find array inputs if only the Reference is supplied by searching through all inputs */
	DATAFLOWCORE_API FDataflowInput* FindInput(const Dataflow::FConnectionReference& Reference);
	DATAFLOWCORE_API const FDataflowInput* FindInput(FName Name) const;
	DATAFLOWCORE_API const FDataflowInput* FindInput(const Dataflow::FConnectionKey& Key) const;
	/**This version can find array inputs if only the Reference is supplied by searching through all inputs */
	DATAFLOWCORE_API const FDataflowInput* FindInput(const Dataflow::FConnectionReference& Reference) const;
	DATAFLOWCORE_API const FDataflowInput* FindInput(const FGuid& InGuid) const;

	DATAFLOWCORE_API virtual void AddOutput(FDataflowOutput* InPtr);
	DATAFLOWCORE_API int NumOutputs() const;
	DATAFLOWCORE_API TArray< FDataflowOutput* > GetOutputs() const;
	DATAFLOWCORE_API void ClearOutputs();
	DATAFLOWCORE_API bool HasHideableOutputs() const;
	DATAFLOWCORE_API bool HasHiddenOutputs() const;

	DATAFLOWCORE_API FDataflowOutput* FindOutput(FName Name);
	DATAFLOWCORE_API FDataflowOutput* FindOutput(uint32 GuidHash);
	DATAFLOWCORE_API FDataflowOutput* FindOutput(const Dataflow::FConnectionKey& Key);
	DATAFLOWCORE_API FDataflowOutput* FindOutput(const Dataflow::FConnectionReference& Reference);
	DATAFLOWCORE_API const FDataflowOutput* FindOutput(FName Name) const;
	DATAFLOWCORE_API const FDataflowOutput* FindOutput(uint32 GuidHash) const;
	DATAFLOWCORE_API const FDataflowOutput* FindOutput(const Dataflow::FConnectionKey& Key) const;
	DATAFLOWCORE_API const FDataflowOutput* FindOutput(const Dataflow::FConnectionReference& Reference) const;
	DATAFLOWCORE_API const FDataflowOutput* FindOutput(const FGuid& InGuid) const;

	/** Return a property's byte offset from the dataflow base node address using the full property name (must includes its parent struct property names). Does not work with array properties.*/
	uint32 GetPropertyOffset(const FName& PropertyFullName) const;

	static DATAFLOWCORE_API const FName DataflowInput;
	static DATAFLOWCORE_API const FName DataflowOutput;
	static DATAFLOWCORE_API const FName DataflowPassthrough;
	static DATAFLOWCORE_API const FName DataflowIntrinsic;

	static DATAFLOWCORE_API const FLinearColor DefaultNodeTitleColor;
	static DATAFLOWCORE_API const FLinearColor DefaultNodeBodyTintColor;

	/** Override this method to provide custom serialization for this node. */
	virtual void Serialize(FArchive& Ar) {}

	/** Override this method to provide custom reconnections when a node inputs has been deprecated and removed. */
	virtual FDataflowInput* RedirectSerializedInput(const FName& MissingInputName) { return nullptr; }
	/** Override this method to provide custom reconnections when a node outputs has been deprecated and removed. */
	virtual FDataflowOutput* RedirectSerializedOutput(const FName& MissingOutputName) { return nullptr; }

	/** Called by editor toolkits when the node is selected, or already selected and invalidated. */
	virtual void OnSelected(Dataflow::FContext& Context) {}
	/** Called by editor toolkits when the node is deselected. */
	virtual void OnDeselected() {}

	//
	//  Struct Support
	//

	virtual void SerializeInternal(FArchive& Ar) { check(false); }
	virtual FStructOnScope* NewStructOnScope() { return nullptr; }
	virtual const UScriptStruct* TypedScriptStruct() const { return nullptr; }

	/** Register the Input and Outputs after the creation in the factory. Use PropertyName to disambiguate a struct name from its first property. */
	template<typename T>
	FDataflowInput& RegisterInputConnection(const Dataflow::TConnectionReference<T>& Reference, const FName& PropertyName = NAME_None)
	{
		FDataflowInput& Input = RegisterInputConnectionInternal(Reference, PropertyName);
		if constexpr (std::is_base_of_v<FDataflowAnyType, T>)
		{
			Input.SetTypePolicy(T::FPolicyType::GetInterface());
		}
		return Input;
	}

	template <typename T>
	FDataflowInput& RegisterInputConnection(const T* Reference, const FName& PropertyName = NAME_None)
	{
		return RegisterInputConnection(Dataflow::TConnectionReference<T>(Reference), PropertyName);
	}

	template <typename T>
	FDataflowOutput& RegisterOutputConnection(const Dataflow::TConnectionReference<T>& Reference, const Dataflow::TConnectionReference<T>& Passthrough = Dataflow::TConnectionReference<T>(nullptr), const FName& PropertyName = NAME_None)
	{
		FDataflowOutput& Output = RegisterOutputConnectionInternal(Reference, PropertyName);
		if constexpr (std::is_base_of_v<FDataflowAnyType, T>)
		{
			Output.SetTypePolicy(T::FPolicyType::GetInterface());
		}

		if (Passthrough.Reference != nullptr)
		{
			Output.SetPassthroughInput(Passthrough);
		}
		return Output;
	}

	template <typename T>
	FDataflowOutput& RegisterOutputConnection(const T* Reference, const T* Passthrough = nullptr, const FName& PropertyName = NAME_None)
	{
		return RegisterOutputConnection(Dataflow::TConnectionReference<T>(Reference), Dataflow::TConnectionReference<T>(Passthrough), PropertyName);
	}

	template <typename T>
	UE_DEPRECATED(5.5, "PassthroughName is no longer needed to register output connections")
	FDataflowOutput& RegisterOutputConnection(const T* Reference, const T* Passthrough, const FName& PropertyName, const FName& PassthroughName)
	{
		return RegisterOutputConnection(Dataflow::TConnectionReference<T>(Reference), Dataflow::TConnectionReference<T>(Passthrough), PropertyName);
	}

	template<typename T>
	FDataflowInput& RegisterInputArrayConnection(const Dataflow::TConnectionReference<T>& Reference, const FName& ElementPropertyName = NAME_None,
		const FName& ArrayPropertyName = NAME_None)
	{
		FDataflowInput& Input = RegisterInputArrayConnectionInternal(Reference, ElementPropertyName, ArrayPropertyName);
		if constexpr (std::is_base_of_v<FDataflowAnyType, T>)
		{
			Input.SetTypePolicy(T::FPolicyType::GetInterface());
		}
		return Input;
	}

	template<typename T>
	FDataflowInput& FindOrRegisterInputArrayConnection(const Dataflow::TConnectionReference<T>& Reference, const FName& ElementPropertyName = NAME_None,
		const FName& ArrayPropertyName = NAME_None)
	{
		if (FDataflowInput* const FoundInput = FindInput(Reference))
		{
			return *FoundInput;
		}

		FDataflowInput& Input = RegisterInputArrayConnectionInternal(Reference, ElementPropertyName, ArrayPropertyName);
		if constexpr (std::is_base_of_v<FDataflowAnyType, T>)
		{
			Input.SetTypePolicy(T::FPolicyType::GetInterface());
		}
		return Input;
	}

	/** Unregister the input connection if one exists matching this property, and then invalidate the graph. */
	void UnregisterInputConnection(const Dataflow::FConnectionReference& Reference)
	{
		UnregisterInputConnection(GetKeyFromReference(Reference));
	}
	UE_DEPRECATED(5.5, "PropertyName is no longer needed to unregister connections")
	void UnregisterInputConnection(const void* Reference, const FName& PropertyName)
	{
		return UnregisterInputConnection(GetKeyFromReference(Reference));
	}
	DATAFLOWCORE_API void UnregisterInputConnection(const Dataflow::FConnectionKey& Key);
	/** Unregister the connection if one exists matching this pin, then invalidate the graph. */
	DATAFLOWCORE_API void UnregisterPinConnection(const Dataflow::FPin& Pin);

	//
	// Evaluation
	//
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput*) const { ensure(false); }

	/**
	*   GetValue(...)
	*
	*	Get the value of the Reference output, invoking up stream evaluations if not 
	*   cached in the contexts data store. 
	* 
	*   @param Context : The evaluation context that holds the data store.
	*   @param Reference : Pointer to a member of this node that corresponds with the output to set.
	*						*Reference will be used as the default if the input is not connected. 
	*/
	template<class T, typename = std::enable_if_t<!std::is_base_of_v<FDataflowAnyType, T>>>
	const T& GetValue(Dataflow::FContext& Context, const Dataflow::TConnectionReference<T>& Reference) const
	{
		checkSlow(FindInput(Reference));
		return FindInput(Reference)->template GetValue<T>(Context, *static_cast<const T*>(Reference.Reference));
	}

	template<typename TAnyType, typename = std::enable_if_t<std::is_base_of_v<FDataflowAnyType, TAnyType>>>
	typename TAnyType::FStorageType GetValue(Dataflow::FContext& Context, const Dataflow::TConnectionReference<TAnyType>& Reference) const
	{
		checkSlow(Reference.Reference && FindInput(Reference));
		return FindInput(Reference)->template GetValueFromAnyType<TAnyType>(Context, static_cast<const TAnyType*>(Reference.Reference)->Value);
	}

	template<class T, typename = std::enable_if_t<!std::is_base_of_v<FDataflowAnyType, T>>>
	const T& GetValue(Dataflow::FContext& Context, const T* Reference) const
	{
		return GetValue(Context, Dataflow::TConnectionReference<T>(Reference));
	}

	template<typename TAnyType, typename = std::enable_if_t<std::is_base_of_v<FDataflowAnyType, TAnyType>>>
	typename TAnyType::FStorageType GetValue(Dataflow::FContext& Context, const TAnyType* Reference) const
	{
		return GetValue(Context, Dataflow::TConnectionReference<TAnyType>(Reference));
	}
	
	template<class T>
	TFuture<const T&> GetValueParallel(Dataflow::FContext& Context, const Dataflow::TConnectionReference<T>& Reference) const
	{
		checkSlow(FindInput(Reference));
		return FindInput(Reference)->template GetValueParallel<T>(Context, *static_cast<const T*>(Reference.Reference));
	}

	template<class T>
	TFuture<const T&> GetValueParallel(Dataflow::FContext& Context, const T* Reference) const
	{
		return GetValueParallel(Context, Dataflow::TConnectionReference<T>(Reference));
	}

	/**
	*   GetValue(...)
	*
	*	Get the value of the Reference output, invoking up stream evaluations if not
	*   cached in the contexts data store.
	*
	*   @param Context : The evaluation context that holds the data store.
	*   @param Reference : Pointer to a member of this node that corresponds with the output to set.
	*   @param Default : Default value if the input is not connected.
	*/
	template<class T> const T& GetValue(Dataflow::FContext& Context, const Dataflow::TConnectionReference<T>& Reference, const T& Default) const
	{
		checkSlow(FindInput(Reference));
		return FindInput(Reference)->template GetValue<T>(Context, Default);
	}

	template<class T> const T& GetValue(Dataflow::FContext& Context, const T* Reference, const T& Default) const
	{
		return GetValue(Context, Dataflow::TConnectionReference<T>(Reference), Default);
	}

	/**
	*   SetValue(...)
	*
	*   Set the value of the Reference output.
	* 
	*   Note: If the compiler errors out with "You cannot bind an lvalue to an rvalue reference", then simply remove
	*         the explicit template parameter from the function call to allow for a const reference type to be deducted.
	*         const int32 Value = 0; SetValue<int32>(Context, Value, &Reference);  // Error: You cannot bind an lvalue to an rvalue reference
	*         const int32 Value = 0; SetValue(Context, Value, &Reference);  // Fine
	*         const int32 Value = 0; SetValue<const int32&>(Context, Value, &Reference);  // Fine
	* 
	*   @param Context : The evaluation context that holds the data store.
	*   @param Value : The value to store in the contexts data store. 
	*   @param Reference : Pointer to a member of this node that corresponds with the output to set. 
	*/
	template<class T, typename = std::enable_if_t<!std::is_base_of_v<FDataflowAnyType, T>>>
	void SetValue(Dataflow::FContext& Context, T&& Value, const typename TDecay<T>::Type* Reference) const
	{
		if (const FDataflowOutput* Output = FindOutput(Reference))
		{
			Output->template SetValue<T>(Forward<T>(Value), Context);
		}
		else
		{
			checkfSlow(false, TEXT("This output could not be found within this node, check this has been properly registered in the node constructor"));
		}
	}

	template<typename TAnyType, typename = std::enable_if_t<std::is_base_of_v<FDataflowAnyType, TAnyType>>>
	void SetValue(Dataflow::FContext& Context, const typename TAnyType::FStorageType& Value, const TAnyType* Reference) const
	{
		if (const FDataflowOutput* Output = FindOutput(Reference))
		{
			Output->template SetValueFromAnyType<TAnyType>(Value, Context);
		}
		else
		{
			checkfSlow(false, TEXT("This output could not be found within this node, check this has been properly registered in the node constructor"));
		}
	}

	/**
	*   ForwardInput(...)
	*
	*   Forward an input to this output.
	*   This will not cache the value itself but cache a reference to the input connection cache entry.
	*   This is memory efficient and do not require a runtime copy of the data.
	*   Input and output references must match in type.
	*   Note that forwarding an input never sets a default value when no input is connected, use SafeForwardInput instead.
	*
	*   @param Context : The evaluation context that holds the data store.
	*   @param InputReference : Pointer to a input member of this node that needs to be forwarded.
	*   @param Reference : Pointer to a member of this node that corresponds with the output to set.
	*/
	DATAFLOWCORE_API void ForwardInput(Dataflow::FContext& Context, const Dataflow::FConnectionReference& InputReference, const Dataflow::FConnectionReference& Reference) const;

	/**
	*   SafeForwardInput(...)
	*
	*   Forward an input to this output or set a default value if no input is connected.
	*   This is more memory efficient when an input is connected than setting the value.
	*   Input and output references must match in type.
	*
	*   @param Context : The evaluation context that holds the data store.
	*   @param InputReference : Pointer to a input member of this node that needs to be forwarded.
	*   @param Reference : Pointer to a member of this node that corresponds with the output to set.
	*/
	template<class T>
	void SafeForwardInput(Dataflow::FContext& Context, const Dataflow::FConnectionReference& InputReference, const T* Reference) const
	{
		if (IsConnected(InputReference))
		{
			ForwardInput(Context, InputReference, Dataflow::TConnectionReference<T>(Reference));
		}
		else if constexpr (std::is_base_of_v<FDataflowAnyType, T>)
		{
			SetValue(Context, static_cast<const T*>(InputReference.Reference)->Value, Reference);
		}
		else
		{
			SetValue(Context, *static_cast<const T*>(InputReference.Reference), Reference);
		}
	}

	/**
	*   IsConnected(...)
	*
	*	Checks if Reference input is connected.
	*
	*   @param Reference : Pointer to a member of this node that corresponds with the input.
	*/
	bool IsConnected(const Dataflow::FConnectionReference& Reference) const
	{
		checkSlow(FindInput(Reference));
		return (FindInput(Reference)->GetConnection() != nullptr);
	}

	template<typename T>
	bool IsConnected(const T* Reference) const
	{
		return IsConnected(Dataflow::FConnectionReference(Reference));
	}

	void PauseInvalidations()
	{
		if (!bPauseInvalidations)
		{
			bPauseInvalidations = true;
			PausedModifiedTimestamp = Dataflow::FTimestamp::Invalid;
		}
	}

	void ResumeInvalidations()
	{
		if (bPauseInvalidations)
		{
			bPauseInvalidations = false;
			Invalidate(PausedModifiedTimestamp);
		}
	}

	DATAFLOWCORE_API void Invalidate(const Dataflow::FTimestamp& ModifiedTimestamp = Dataflow::FTimestamp::Current());

	virtual void OnInvalidate() {}

	DATAFLOWCORE_API virtual bool ValidateConnections();

	DATAFLOWCORE_API virtual void ValidateProperties();

	bool HasValidConnections() const { return bHasValidConnections; }

	virtual bool IsA(FName InType) const 
	{ 
		return InType.ToString().Equals(StaticType().ToString());
	}

	template<class T>
	const T* AsType() const
	{
		FName TargetType = T::StaticType();
		if (IsA(TargetType))
		{
			return (T*)this;
		}
		return nullptr;
	}

	template<class T>
	T* AsType()
	{
		FName TargetType = T::StaticType();
		if (IsA(TargetType))
		{
			return (T*)this;
		}
		return nullptr;
	}

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnNodeInvalidated, FDataflowNode*);
	FOnNodeInvalidated& GetOnNodeInvalidatedDelegate() { return OnNodeInvalidatedDelegate; }

	// returns true if the type was changed successfully
	// only unset datatype connection will be set a new type 
	DATAFLOWCORE_API bool TrySetConnectionType(FDataflowConnection* Connection, FName NewType);

	// Only used when forcing types on connection in order to make sure the node properly refreshes the rest of its connection accordingly if there's any dependencies between their types
	DATAFLOWCORE_API void NotifyConnectionTypeChanged(FDataflowConnection* Connection);

protected:
	virtual bool OnInputTypeChanged(const FDataflowInput* Input) { return false; };
	virtual bool OnOutputTypeChanged(const FDataflowOutput* Output) { return false; }

	// returns true if the input type was changed successfully
	DATAFLOWCORE_API bool SetInputConcreteType(const Dataflow::FConnectionReference& InputReference, FName NewType);

	// returns true if the output type was changed successfully
	DATAFLOWCORE_API bool SetOutputConcreteType(const Dataflow::FConnectionReference& OutputReference, FName NewType);

	DATAFLOWCORE_API FDataflowInput& RegisterInputConnectionInternal(const Dataflow::FConnectionReference& Reference, const FName& PropertyName = NAME_None);
	DATAFLOWCORE_API FDataflowOutput& RegisterOutputConnectionInternal(const Dataflow::FConnectionReference& Reference, const FName& PropertyName = NAME_None);
	DATAFLOWCORE_API FDataflowInput& RegisterInputArrayConnectionInternal(const Dataflow::FConnectionReference& Reference, const FName& ElementPropertyName = NAME_None,
		const FName& ArrayPropertyName = NAME_None);

private:

	void InitConnectionParametersFromPropertyReference(const FStructOnScope& StructOnScope, const void* PropertyRef, const FName& PropertyName, Dataflow::FConnectionParameters& OutParams);
	// This will add [ContainerIndex] to any array it finds unless ContainerIndex == INDEX_NONE.
	static FString GetPropertyFullNameString(const TConstArrayView<const FProperty*>& PropertyChain, int32 ContainerIndex = INDEX_NONE);
	static FName GetPropertyFullName(const TArray<const FProperty*>& PropertyChain, int32 ContainerIndex = INDEX_NONE);
	static FText GetPropertyDisplayNameText(const TArray<const FProperty*>& PropertyChain, int32 ContainerIndex = INDEX_NONE);
	static FString StripContainerIndexFromPropertyFullName(const FString& PropertyFullName);
	static uint32 GetPropertyOffset(const TArray<const FProperty*>& PropertyChain);
	uint32 GetConnectionOffsetFromReference(const void* Reference) const;
	DATAFLOWCORE_API Dataflow::FConnectionKey GetKeyFromReference(const Dataflow::FConnectionReference& Reference) const;

	/**
	* Find a property using the property address and name (not including its parent struct property names).
	* If NAME_None is used as the name, and the same address is shared by a parent structure property and
	* its first child property, then the parent will be returned.
	*/
	const FProperty* FindProperty(const UStruct* Struct, const void* Property, const FName& PropertyName, TArray<const FProperty*>* OutPropertyChain = nullptr) const;
	const FProperty& FindPropertyChecked(const UStruct* Struct, const void* Property, const FName& PropertyName, TArray<const FProperty*>* OutPropertyChain = nullptr) const;

	/** Find a property using the property full name (must includes its parent struct property names). */
	const FProperty* FindProperty(const UStruct* Struct, const FName& PropertyFullName, TArray<const FProperty*>* OutPropertyChain = nullptr) const;

	virtual TArray<Dataflow::FRenderingParameter> GetRenderParametersImpl() const { return TArray<Dataflow::FRenderingParameter>(); }

	bool bHasValidConnections = true;
	TMap<Dataflow::FConnectionKey, FDataflowInput*> ExpandedInputs;
	TMap<uint32, const FArrayProperty*> InputArrayProperties; // Used to calculate ContainerElementOffsets

protected:
	bool bPauseInvalidations = false;
	Dataflow::FTimestamp PausedModifiedTimestamp = Dataflow::FTimestamp::Invalid; // When unpausing invalidations, Invalidate will be called with this timestamp.
	FOnNodeInvalidated OnNodeInvalidatedDelegate;

};

namespace Dataflow
{

class FDataflowNodePauseInvalidationScope
{
public:
	explicit FDataflowNodePauseInvalidationScope(FDataflowNode* InNode)
		:Node(InNode)
	{
		if (Node)
		{
			Node->PauseInvalidations();
		}
	}

	~FDataflowNodePauseInvalidationScope()
	{
		if (Node)
		{
			Node->ResumeInvalidations();
		}
	}

	FDataflowNodePauseInvalidationScope() = delete;
	FDataflowNodePauseInvalidationScope(const FDataflowNodePauseInvalidationScope&) = delete;
	FDataflowNodePauseInvalidationScope(FDataflowNodePauseInvalidationScope&&) = delete;
private:
	FDataflowNode* Node;
};

	//
	// Use these macros to register dataflow nodes. 
	//

#define DATAFLOW_NODE_REGISTER_CREATION_FACTORY(A)									\
	::Dataflow::FNodeFactory::RegisterNodeFromType<A>();

#define DATAFLOW_NODE_RENDER_TYPE(A, B, ...)												\
	virtual TArray<::Dataflow::FRenderingParameter> GetRenderParametersImpl() const {		\
		TArray<::Dataflow::FRenderingParameter> Array;								\
		Array.Add({ A, B, {__VA_ARGS__,} });														\
		return Array;}

#define DATAFLOW_NODE_DEFINE_INTERNAL(TYPE, DISPLAY_NAME, CATEGORY, TAGS)			\
public:																				\
	static FName StaticType() {return #TYPE;}										\
	static FName StaticDisplay() {return DISPLAY_NAME;}								\
	static FName StaticCategory() {return CATEGORY;}								\
	static FString StaticTags() {return TAGS;}										\
	static FString StaticToolTip() {return FString("Create a dataflow node.");}		\
	virtual FName GetType() const { return #TYPE; }									\
	virtual bool IsA(FName InType) const override									\
		{ return InType.ToString().Equals(StaticType().ToString()) || Super::IsA(InType); }	\
	virtual FStructOnScope* NewStructOnScope() override {							\
	   return new FStructOnScope(TYPE::StaticStruct(), (uint8*)this);}				\
	virtual void SerializeInternal(FArchive& Ar) override {							\
		UScriptStruct* const Struct = TYPE::StaticStruct();							\
		Struct->SerializeTaggedProperties(Ar, (uint8*)this,							\
		Struct, nullptr);															\
		Serialize(Ar);}																\
	virtual FName GetDisplayName() const override { return TYPE::StaticDisplay(); }	\
	virtual FName GetCategory() const override { return TYPE::StaticCategory(); }	\
	virtual FString GetTags() const override { return TYPE::StaticTags(); }			\
	virtual const UScriptStruct* TypedScriptStruct() const override					\
		{return TYPE::StaticStruct();}												\
	TYPE() {}																		\
private:

#define DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY(A, C1, C2)	\
{																					\
	::Dataflow::FNodeColorsRegistry::Get().RegisterNodeColors(A, {C1, C2});			\
}																					\

}


