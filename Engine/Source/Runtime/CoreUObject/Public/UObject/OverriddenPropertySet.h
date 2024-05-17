// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Serialization/ArchiveSerializedPropertyChain.h"

#include "OverriddenPropertySet.generated.h"

COREUOBJECT_API DECLARE_LOG_CATEGORY_EXTERN(LogOverridableObject, Warning, All);

struct FPropertyChangedChainEvent;

/*
 *************************************************************************************
 * Overridable serialization is experimental, not supported and use at your own risk *
 *************************************************************************************
 * Scope responsible to control overridable serialization logic.
 */
struct FOverridableSerializationLogic
{
public:

	/**
	 * Call to enable overridable serialization and to set the overridden properties of the current serialized object
	 * Note this is not re-entrant and it stores information in a thread local storage
	 * @param InOverriddenProperties of the current serializing object */
	inline static void Enable(FOverriddenPropertySet* InOverriddenProperties)
	{
		checkf(!bUseOverridableSerialization, TEXT("Nobody should use this method if overridable serialization is already enabled"));
		bUseOverridableSerialization = true;
		OverriddenProperties = InOverriddenProperties;
	}

	/**
	 * Call to disable overridable serialization
	 * Note this is not re-entrant and it stores information in a thread local storage */
	inline static void Disable()
	{
		checkf(bUseOverridableSerialization, TEXT("Expecting overridable serialization to be already enabled"));
		OverriddenProperties = nullptr;
		bUseOverridableSerialization = false;
	}

	/**
	 * Called during the serialization of an object to know to know if it should do overridden serialization logic
	 * @return true if the overridable serialization is enabled on the current serializing object */
	inline static bool IsEnabled()
	{
		return bUseOverridableSerialization;
	}

	/**
	 * Call during the serialization of an object to get its overriden properties
	 * Note: Expects the current serialized object to use overridable serialization
	 * Note this is not re-entrant and it stores information in a thread local storage
	 * @return the overridden properties of the current object being serialized */
	inline static FOverriddenPropertySet* GetOverriddenProperties()
	{
		return OverriddenProperties;
	}

	/**
	 * Retrieve from the Archive and the current property the overridden property operation to know if it has to be serialized or not
	 * @param Ar currently being used to serialize the current object (will be used to retrieve the current property serialized path)
	 * @param Property the property about to be serialized, can be null
	 * @param DataPtr to the memory of that property
	 * @param DefaultValue memory pointer of that property
	 * @return the overridden property operation */
	COREUOBJECT_API static EOverriddenPropertyOperation GetOverriddenPropertyOperation(const FArchive& Ar, FProperty* Property = nullptr, uint8* DataPtr = nullptr, uint8* DefaultValue = nullptr);

	/**
	 * Use the port text path to retrieve the current overridden property operation to know if it has to be serialized or not
	 * @param DataPtr to the memory of that property
	 * @param DefaultValue memory pointer of that property
	 * @param PortFlags for the import/export text operation
	 * @return the overridden property operation */
	COREUOBJECT_API static EOverriddenPropertyOperation GetOverriddenPropertyOperationForPortText(const void* DataPtr, const void* DefaultValue, int32 PortFlags);

	/**
	 * Call during the import text
	 * @return the current text import property path */
	COREUOBJECT_API static FPropertyVisitorPath* GetOverriddenPortTextPropertyPath();

	/**
	 * Call during the import text to set the property path
	 * @param Path to start tracking the property path*/
	COREUOBJECT_API static void SetOverriddenPortTextPropertyPath(FPropertyVisitorPath& Path);

	/** Call during the import text to reset property path */
	COREUOBJECT_API static void ResetOverriddenPortTextPropertyPath();

private:

	static EOverriddenPropertyOperation GetOverriddenPropertyOperation(const int32 PortFlags, const FArchiveSerializedPropertyChain* CurrentPropertyChain, FProperty* Property, const void* DataPtr, const void* DefaultValue);

	static thread_local bool bUseOverridableSerialization;
	static thread_local FOverriddenPropertySet* OverriddenProperties;
	static thread_local FPropertyVisitorPath* OverriddenPortTextPropertyPath;
};

/*
 *************************************************************************************
 * Overridable serialization is experimental, not supported and use at your own risk *
 *************************************************************************************
 * Scope responsible for enabling/disabling the overridable serialization from the parameters
*/
struct FEnableOverridableSerializationScope
{
	COREUOBJECT_API FEnableOverridableSerializationScope(bool bEnableOverridableSerialization, FOverriddenPropertySet* OverriddenProperties);
	COREUOBJECT_API ~FEnableOverridableSerializationScope();

protected:
	bool bOverridableSerializationEnabled = false;
	bool bWasOverridableSerializationEnabled = false;
	FOverriddenPropertySet* SavedOverriddenProperties = nullptr;
};


/*
 *************************************************************************************
 * Overridable serialization is experimental, not supported and use at your own risk *
 *************************************************************************************
 * Scope responsible for tracking current property path for text importing
*/
struct FOverridableTextPortPropertyPathScope
{
	COREUOBJECT_API FOverridableTextPortPropertyPathScope(const FProperty* InProperty, int32 InIndex = INDEX_NONE, EPropertyVisitorInfoType InPropertyInfo = EPropertyVisitorInfoType::None);
	COREUOBJECT_API ~FOverridableTextPortPropertyPathScope();

protected:

	const FProperty* Property = nullptr;
	FPropertyVisitorPath DefaultPath;
};

/*
 * Override operation type for each property node
 */
UENUM()
enum class EOverriddenPropertyOperation : uint8
{
	None =	0,	/* no overridden operation was recorded on this property  */
	Modified,	/* some sub property has recorded overridden operation */
	Replace,	/* everything has been overridden from this property down to every sub property/sub object*/
	Add,		/* this element was added in the container */
	Remove,		/* this element was removed from the container */
};

inline TOptional<EOverriddenPropertyOperation> GetOverriddenOperationFromString(const FString& OverriddenOperationString)
{
	int64 EnumValue = StaticEnum<EOverriddenPropertyOperation>()->GetValueByNameString(StaticEnum<EOverriddenPropertyOperation>()->GetName() + FString(TEXT("::")) + OverriddenOperationString);
	if (EnumValue == INDEX_NONE)
	{
		TOptional<EOverriddenPropertyOperation>();
	}

	return TOptional((EOverriddenPropertyOperation)EnumValue);
}

inline TOptional<EOverriddenPropertyOperation> GetOverriddenOperationFromName(const FName OverriddenOperationName)
{
	return GetOverriddenOperationFromString(OverriddenOperationName.ToString());
}

inline FString GetOverriddenOperationString(EOverriddenPropertyOperation Operation)
{
	return UEnum::GetValueAsString(Operation).RightChop(StaticEnum<EOverriddenPropertyOperation>()->GetName().Len() + /*::*/2);
}

USTRUCT()
struct FOverriddenPropertyNodeID
{
	GENERATED_BODY()

	FOverriddenPropertyNodeID(FName InPath = NAME_None)
		: Path(InPath)
		, Object(nullptr)
	{}

	FOverriddenPropertyNodeID(const UObject& InObject)
		: Path(FString::Printf(TEXT("%d"), GUObjectArray.ObjectToIndex(&InObject)))
		, Object(&InObject)
	{
	}

	bool operator==(const FOverriddenPropertyNodeID& Other) const
	{
		return Path == Other.Path || (Object && Other.Object && Object == Other.Object);
	}

	FString ToString() const
	{
		return Path.ToString();
	}

	bool IsValid() const
	{
		return !Path.IsNone();
	}

	void HandleObjectsReInstantiated(const TMap<UObject*, UObject*>& Map);

	UPROPERTY()
	FName Path;

	UPROPERTY()
	TObjectPtr<const UObject> Object;

	friend uint32 GetTypeHash(const FOverriddenPropertyNodeID& NodeID)
	{
		return GetTypeHash(NodeID.Path);
	}
};

/*
 *************************************************************************************
 * Overridable serialization is experimental, not supported and use at your own risk *
 *************************************************************************************
 * Overridden property information node, there will be one per overriden property/subojects
 *
 */
USTRUCT()
struct FOverriddenPropertyNode
{
	GENERATED_BODY()

	FOverriddenPropertyNode(FOverriddenPropertyNodeID InNodeID = FOverriddenPropertyNodeID())
		: NodeID(InNodeID)
	{}

	UPROPERTY()
	FOverriddenPropertyNodeID NodeID;

	UPROPERTY()
	EOverriddenPropertyOperation Operation = EOverriddenPropertyOperation::None;

	UPROPERTY()
	TMap<FOverriddenPropertyNodeID, FOverriddenPropertyNodeID> SubPropertyNodeKeys;

	bool operator==(const FOverriddenPropertyNode& Other) const
	{
		return NodeID == Other.NodeID;
	}

	friend uint32 GetTypeHash(const FOverriddenPropertyNode& Node)
	{
		return GetTypeHash(Node.NodeID);
	}
};



/*
 * Property change notification type mapping the Pre/PostEditChange callbacks
 */
enum class EPropertyNotificationType : uint8
{
	PreEdit,
	PostEdit
};

/*
 *************************************************************************************
 * Overridable serialization is experimental, not supported and use at your own risk *
 *************************************************************************************
 * Structure holding and tracking overridden properties of an UObject
 */
USTRUCT()
struct COREUOBJECT_API FOverriddenPropertySet
{
	GENERATED_BODY()

public:

	FOverriddenPropertySet() = default;
	FOverriddenPropertySet(UObject& InOwner)
		: Owner(&InOwner)
	{}

	/**
	 * Retrieve the overridable operation from the specified the edit property chain node
	 * @param PropertyEvent only needed to know about the container item index in any
	 * @param PropertyNode leading to the property interested in, null will return the operation of the object itself
	 * @param bOutInheritedOperation optional parameter to know if the state returned was inherited from a parent property
	 * @return the current type of override operation on the property */
	EOverriddenPropertyOperation GetOverriddenPropertyOperation(const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode, bool* bOutInheritedOperation = nullptr) const;

	/**
	 * Clear any properties from the serialized property chain node
	 * @param PropertyEvent only needed to know about the container item index in any
	 * @param PropertyNode leading to the property to clear, null will clear the overrides on the object itself
	 * @return if the operation was successful */
	bool ClearOverriddenProperty(const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode);

	/**
	 * Utility methods that call NotifyPropertyChange(Pre/PostEdit)
	 * @param PropertyEvent information about the type of change
	 * @param PropertyNode leading to the property that is changing, null means it is the object itself that is changing
	 * @param Data memory of the current property */
	void OverrideProperty(const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode, const void* Data);

	/**
	 * Handling and storing modification on a property of an object
	 * @param Notification type either pre/post property overridden
	 * @param PropertyEvent information about the type of change
	 * @param PropertyNode leading to the property that is changing, null means it is the object itself that is changing
	 * @param Data memory of the current property */
	void NotifyPropertyChange(const EPropertyNotificationType Notification, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode, const void* Data);

	/**
	 * Retrieve the overridable operation from the specified the serialized property chain and the specified property
	 * @param CurrentPropertyChain leading to the property being serialized if any, null or empty it will return the node of the object itself
	 * @param Property being serialized if any, if null it will fallback on the last property of the chain
	 * @return the current type of override operation on the property */
	EOverriddenPropertyOperation GetOverriddenPropertyOperation(const FArchiveSerializedPropertyChain* CurrentPropertyChain, FProperty* Property) const;

	/**
	 * Setup the overridable operation of the current property from the serialized property chain and the specified property
	 * @param Operation to set for this property
	 * @param CurrentPropertyChain leading to the property being serialized if any, null or empty it will return the node of the object itself
	 * @param Property being serialized if any, if null it will fallback on the last property of the chain
	 * @return the node containing the information of the overridden property */
	FOverriddenPropertyNode* SetOverriddenPropertyOperation(EOverriddenPropertyOperation Operation, const FArchiveSerializedPropertyChain* CurrentPropertyChain, FProperty* Property);

	/**
	 * Retrieve the overridden property node from the serialized property chain
	 * @param CurrentPropertyChain leading to the property being serialized if any, null or empty it will return the node of the object itself
	 * @return the node containing the information of the overridden property */
	const FOverriddenPropertyNode* GetOverriddenPropertyNode(const FArchiveSerializedPropertyChain* CurrentPropertyChain) const;

	/**
	 * Retrieve the overridable operation given the property key
	 * @param NodeID that uniquely identify the property within the object 
	 * @return the current type of override operation on the property */
	EOverriddenPropertyOperation GetSubPropertyOperation(FOverriddenPropertyNodeID NodeID) const;

	/**
	 * Set the overridable operation of a sub property of the specified node.
	 * @param Operation to set for this property
	 * @param Node from where the sub property is owned by
	 * @param NodeID the ID of the sub property 
	 * @return the node to the sub property */
	FOverriddenPropertyNode* SetSubPropertyOperation(EOverriddenPropertyOperation Operation, FOverriddenPropertyNode& Node, FOverriddenPropertyNodeID NodeID);

	/**
	 * Check if this is an overridden property set of a CDO and that this property is owned by the class of this CDO
	 * NOTE: this is used to know if a property should be serialized to keep its default CDO value.
	 * @param Property 
	 * @return 
	 */
	bool IsCDOOwningProperty(const FProperty& Property) const;

	/**
	 * Resets all overrides of the object */
	void Reset();
	void HandleObjectsReInstantiated(const TMap<UObject*, UObject*>& Map);

protected:

	FOverriddenPropertyNode& FindOrAddNode(FOverriddenPropertyNode& ParentPropertyNode, FOverriddenPropertyNodeID NodeID);

	EOverriddenPropertyOperation GetOverriddenPropertyOperation(const FOverriddenPropertyNode& ParentPropertyNode, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode, bool* bOutInheritedOperation, const void* Data) const;
	bool ClearOverriddenProperty(FOverriddenPropertyNode& ParentPropertyNode, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode, const void* Data);
	void NotifyPropertyChange(FOverriddenPropertyNode* ParentPropertyNode, const EPropertyNotificationType Notification, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode, const void* Data);

	EOverriddenPropertyOperation GetOverriddenPropertyOperation(const FOverriddenPropertyNode& ParentPropertyNode, const FArchiveSerializedPropertyChain* CurrentPropertyChain, FProperty* Property) const;
	FOverriddenPropertyNode* SetOverriddenPropertyOperation(EOverriddenPropertyOperation Operation, FOverriddenPropertyNode& ParentPropertyNode, const FArchiveSerializedPropertyChain* CurrentPropertyChain, FProperty* Property);
	const FOverriddenPropertyNode* GetOverriddenPropertyNode(const FOverriddenPropertyNode& ParentPropertyNode, const FArchiveSerializedPropertyChain* CurrentPropertyChain) const;

	void RemoveOverriddenSubProperties(FOverriddenPropertyNode& PropertyNode);


private:
	UPROPERTY()
	TObjectPtr<UObject> Owner = nullptr;

	UPROPERTY()
	TSet<FOverriddenPropertyNode> OverriddenPropertyNodes;

	static inline FOverriddenPropertyNodeID RootNodeID = FOverriddenPropertyNodeID(FName(TEXT("root")));

public:
	bool bNeedsSubobjectTemplateInstantiation = false;
};

// Utility methods for maps
namespace UE::OverridableMapUtilities
{
	/**
	 * Find the index of the key id in the specified map
	 * @param KeyIDToFind in the map
	 * @param MapHelper the map to search in
	 * @return INDEX_NONE if it didn't find it
	 */
	COREUOBJECT_API int32 FindKeyInternalIndex(const FOverriddenPropertyNodeID& KeyIDToFind, FScriptMapHelper& MapHelper);

	/**
	 * Retrieve the key id from the key property and value
	 * NOTE: This method will check if unable to create the key
	 * @param KeyProp to use while converting to an key id
	 * @param KeyData to use while converting to an key id
	 * @return the key id of the specified key
	 */
	COREUOBJECT_API FOverriddenPropertyNodeID GetIDFromKey(const FProperty* KeyProp, uint8* KeyData);
}