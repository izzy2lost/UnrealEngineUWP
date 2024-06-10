// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Map.h"
#include "Interfaces/MetasoundFrontendInterfaceRegistry.h"
#include "MetasoundFrontendDocumentCacheInterface.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentModifyDelegates.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundVertex.h"
#include "Templates/Function.h"
#include "UObject/ScriptInterface.h"

#include "MetasoundFrontendDocumentBuilder.generated.h"


// Forward Declarations
class FMetasoundAssetBase;


namespace Metasound::Frontend
{
	// Forward Declarations
	class INodeTemplate;

	using FFinalizeNodeFunctionRef = TFunctionRef<void(FMetasoundFrontendNode&, const Metasound::Frontend::FNodeRegistryKey&)>;

	enum class EInvalidEdgeReason : uint8
	{
		None = 0,
		MismatchedAccessType,
		MismatchedDataType,
		MissingInput,
		MissingOutput,
		COUNT
	};

	METASOUNDFRONTEND_API FString LexToString(const EInvalidEdgeReason& InReason);

	struct METASOUNDFRONTEND_API FNamedEdge
	{
		const FGuid OutputNodeID;
		const FName OutputName;
		const FGuid InputNodeID;
		const FName InputName;

		friend bool operator==(const FNamedEdge& InLHS, const FNamedEdge& InRHS)
		{
			return InLHS.OutputNodeID == InRHS.OutputNodeID
				&& InLHS.OutputName == InRHS.OutputName
				&& InLHS.InputNodeID == InRHS.InputNodeID
				&& InLHS.InputName == InRHS.InputName;
		}

		friend bool operator!=(const FNamedEdge& InLHS, const FNamedEdge& InRHS)
		{
			return !(InLHS == InRHS);
		}

		friend FORCEINLINE uint32 GetTypeHash(const FNamedEdge& InBinding)
		{
			const int32 NameHash = HashCombineFast(GetTypeHash(InBinding.OutputName), GetTypeHash(InBinding.InputName));
			const int32 GuidHash = HashCombineFast(GetTypeHash(InBinding.OutputNodeID), GetTypeHash(InBinding.InputNodeID));
			return HashCombineFast(NameHash, GuidHash);
		}
	};

	struct METASOUNDFRONTEND_API FModifyInterfaceOptions
	{
		FModifyInterfaceOptions(const TArray<FMetasoundFrontendInterface>& InInterfacesToRemove, const TArray<FMetasoundFrontendInterface>& InInterfacesToAdd);
		FModifyInterfaceOptions(TArray<FMetasoundFrontendInterface>&& InInterfacesToRemove, TArray<FMetasoundFrontendInterface>&& InInterfacesToAdd);
		FModifyInterfaceOptions(const TArray<FMetasoundFrontendVersion>& InInterfaceVersionsToRemove, const TArray<FMetasoundFrontendVersion>& InInterfaceVersionsToAdd);

		TArray<FMetasoundFrontendInterface> InterfacesToRemove;
		TArray<FMetasoundFrontendInterface> InterfacesToAdd;

		// Function used to determine if an old of a removed interface
		// and new member of an added interface are considered equal and
		// to be swapped, retaining preexisting connections (and locations
		// if in editor and 'SetDefaultNodeLocations' option is set)
		TFunction<bool(FName, FName)> NamePairingFunction;

#if WITH_EDITORONLY_DATA
		bool bSetDefaultNodeLocations = true;
#endif // WITH_EDITORONLY_DATA
	};
} // namespace Metasound::Frontend


// Builder Document UObject, which is only used for registration purposes when attempting
// async registration whereby the original document is serialized and must not be mutated.
UCLASS()
class METASOUNDFRONTEND_API UMetaSoundBuilderDocument : public UObject, public IMetaSoundDocumentInterface
{
	GENERATED_BODY()

public:
	UE_DEPRECATED(5.5, "Use overload supplying MetaSound to copy (builder documents no longer supported for cases outside of cloned document registration.")
	static UMetaSoundBuilderDocument& Create(const UClass& InBuilderClass);

	// Create and return a valid builder document which copies the provided interface's document & class
	static UMetaSoundBuilderDocument& Create(const IMetaSoundDocumentInterface& InDocToCopy);

	virtual bool ConformObjectToDocument() override;

	// Returns the document
	virtual const FMetasoundFrontendDocument& GetConstDocument() const override;

	// Returns temp path of builder document
	virtual FTopLevelAssetPath GetAssetPathChecked() const override;

	// Returns the base class registered with the MetaSound UObject registry.
	virtual const UClass& GetBaseMetaSoundUClass() const final override;

	// Returns the builder class used to modify the given document.
	virtual const UClass& GetBuilderUClass() const final override;

	// Returns if the document is being actively built (always true as builder documents are always being actively built)
	virtual bool IsActivelyBuilding() const final override;

private:
	virtual FMetasoundFrontendDocument& GetDocument() override;

	virtual void OnBeginActiveBuilder() override;
	virtual void OnFinishActiveBuilder() override;

	UPROPERTY(Transient)
	FMetasoundFrontendDocument Document;

	UPROPERTY(Transient)
	TObjectPtr<const UClass> MetaSoundUClass = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<const UClass> BuilderUClass = nullptr;
};

// Builder used to support dynamically generating MetaSound documents at runtime. Builder contains caches that speed up
// common search and modification operations on a given document, which may result in slower performance on construction,
// but faster manipulation of its managed document.  The builder's managed copy of a document is expected to not be modified
// by any external system to avoid cache becoming stale.
USTRUCT()
struct METASOUNDFRONTEND_API FMetaSoundFrontendDocumentBuilder
{
	GENERATED_BODY()

public:
	// Default ctor should typically never be used directly as builder interface (and optionally delegates) should be specified on construction (Default exists only to make UObject reflection happy).
	FMetaSoundFrontendDocumentBuilder(TScriptInterface<IMetaSoundDocumentInterface> InDocumentInterface = { }, TSharedPtr<Metasound::Frontend::FDocumentModifyDelegates> InDocumentDelegates = { }, bool bPrimeCache = false);
	virtual ~FMetaSoundFrontendDocumentBuilder();

	// Call when the builder will no longer modify the IMetaSoundDocumentInterface
	void FinishBuilding();

	const FMetasoundFrontendClass* AddDependency(const FMetasoundFrontendClass& InClass);
	void AddEdge(FMetasoundFrontendEdge&& InNewEdge);
	bool AddNamedEdges(const TSet<Metasound::Frontend::FNamedEdge>& ConnectionsToMake, TArray<const FMetasoundFrontendEdge*>* OutEdgesCreated = nullptr, bool bReplaceExistingConnections = true);
	bool AddEdgesByNodeClassInterfaceBindings(const FGuid& InFromNodeID, const FGuid& InToNodeID, bool bReplaceExistingConnections = true);
	bool AddEdgesFromMatchingInterfaceNodeOutputsToGraphOutputs(const FGuid& InNodeID, TArray<const FMetasoundFrontendEdge*>& OutEdgesCreated, bool bReplaceExistingConnections = true);
	bool AddEdgesFromMatchingInterfaceNodeInputsToGraphInputs(const FGuid& InNodeID, TArray<const FMetasoundFrontendEdge*>& OutEdgesCreated, bool bReplaceExistingConnections = true);
	const FMetasoundFrontendNode* AddGraphInput(const FMetasoundFrontendClassInput& InClassInput);
	const FMetasoundFrontendNode* DuplicateGraphInput(const FMetasoundFrontendClassInput& InClassInput, FMetasoundFrontendLiteral DefaultValue, const FName InName);
	const FMetasoundFrontendNode* AddGraphNode(const FMetasoundFrontendGraphClass& InClass, FGuid InNodeID = FGuid::NewGuid());
	const FMetasoundFrontendNode* AddGraphOutput(const FMetasoundFrontendClassOutput& InClassOutput);
	const FMetasoundFrontendNode* DuplicateGraphOutput(const FMetasoundFrontendClassOutput& InClassOutput, const FName InName);

	bool AddInterface(FName InterfaceName);

	const FMetasoundFrontendNode* AddNodeByClassName(const FMetasoundFrontendClassName& InClassName, int32 InMajorVersion = 1, FGuid InNodeID = FGuid::NewGuid());
	const FMetasoundFrontendNode* AddNodeByTemplate(const Metasound::Frontend::INodeTemplate& InTemplate, Metasound::Frontend::FNodeTemplateGenerateInterfaceParams Params, FGuid InNodeID = FGuid::NewGuid());

	// Returns whether or not the given edge can be added, which requires that its input
	// is not already connected and the edge is valid (see function 'IsValidEdge').
	bool CanAddEdge(const FMetasoundFrontendEdge& InEdge) const;

	// Clears document completely of all graph page data (nodes, edges, & member metadata), dependencies,
	// interfaces, member metadata, preset state, etc. Leaves ClassMetadata intact. Reloads the builder state,
	// so external delegates must be relinked if desired.
	void ClearDocument(TSharedRef<Metasound::Frontend::FDocumentModifyDelegates> ModifyDelegates = { });

	UE_DEPRECATED(5.5, "Use ClearDocument instead")
	void ClearGraph() {  }

#if WITH_EDITORONLY_DATA
	bool ClearMemberMetadata(const FGuid& InMemberID);
#endif // WITH_EDITORONLY_DATA

	bool ContainsDependencyOfType(EMetasoundFrontendClassType ClassType) const;
	bool ContainsEdge(const FMetasoundFrontendEdge& InEdge) const;
	bool ContainsNode(const FGuid& InNodeID) const;

	bool ConvertFromPreset();
	bool ConvertToPreset(const FMetasoundFrontendDocument& InReferencedDocument, TSharedRef<Metasound::Frontend::FDocumentModifyDelegates> ModifyDelegates = { });

#if WITH_EDITORONLY_DATA
	const FMetaSoundFrontendGraphComment* FindGraphComment(const FGuid& InCommentID) const;
	FMetaSoundFrontendGraphComment* FindGraphComment(const FGuid& InCommentID);
	FMetaSoundFrontendGraphComment& FindOrAddGraphComment(const FGuid& InCommentID);
	UMetaSoundFrontendMemberMetadata* FindMemberMetadata(const FGuid& InMemberID);
#endif // WITH_EDITORONLY_DATA

	static bool FindDeclaredInterfaces(const FMetasoundFrontendDocument& InDocument, TArray<const Metasound::Frontend::IInterfaceRegistryEntry*>& OutInterfaces);
	bool FindDeclaredInterfaces(TArray<const Metasound::Frontend::IInterfaceRegistryEntry*>& OutInterfaces) const;

	const FMetasoundFrontendClass* FindDependency(const FGuid& InClassID) const;
	const FMetasoundFrontendClass* FindDependency(const FMetasoundFrontendClassMetadata& InMetadata) const;
	TArray<const FMetasoundFrontendEdge*> FindEdges(const FGuid& InNodeID, const FGuid& InVertexID) const;

	const FMetasoundFrontendClassInput* FindGraphInput(FName InputName) const;
	const FMetasoundFrontendNode* FindGraphInputNode(FName InputName) const;
	const FMetasoundFrontendClassOutput* FindGraphOutput(FName OutputName) const;
	const FMetasoundFrontendNode* FindGraphOutputNode(FName OutputName) const;

	bool FindInterfaceInputNodes(FName InterfaceName, TArray<const FMetasoundFrontendNode*>& OutInputs) const;
	bool FindInterfaceOutputNodes(FName InterfaceName, TArray<const FMetasoundFrontendNode*>& OutOutputs) const;

	FMetasoundFrontendGraph& FindBuildGraphChecked();
	const FMetasoundFrontendGraph& FindConstBuildGraphChecked() const;

	const FMetasoundFrontendNode* FindNode(const FGuid& InNodeID) const;

	const FMetasoundFrontendVertex* FindNodeInput(const FGuid& InNodeID, const FGuid& InVertexID) const;
	const FMetasoundFrontendVertex* FindNodeInput(const FGuid& InNodeID, FName InVertexName) const;
	TArray<const FMetasoundFrontendVertex*> FindNodeInputs(const FGuid& InNodeID, FName TypeName = FName()) const;
	TArray<const FMetasoundFrontendVertex*> FindNodeInputsConnectedToNodeOutput(const FGuid& InOutputNodeID, const FGuid& InOutputVertexID, TArray<const FMetasoundFrontendNode*>* ConnectedInputNodes = nullptr) const;

	const FMetasoundFrontendVertex* FindNodeOutput(const FGuid& InNodeID, const FGuid& InVertexID) const;
	const FMetasoundFrontendVertex* FindNodeOutput(const FGuid& InNodeID, FName InVertexName) const;
	TArray<const FMetasoundFrontendVertex*> FindNodeOutputs(const FGuid& InNodeID, FName TypeName = FName()) const;
	const FMetasoundFrontendVertex* FindNodeOutputConnectedToNodeInput(const FGuid& InInputNodeID, const FGuid& InInputVertexID, const FMetasoundFrontendNode** ConnectedOutputNode = nullptr) const;

	const FMetasoundFrontendDocument& GetConstDocumentChecked() const;
	const IMetaSoundDocumentInterface& GetConstDocumentInterfaceChecked() const;
	const FString GetDebugName() const;

	UE_DEPRECATED(5.5, "Use GetConstDocumentChecked() instead")
	const FMetasoundFrontendDocument& GetDocument() const;

	// The graph ID used when requests are made to mutate specific paged graph topology (ex. adding or removing nodes or edges)
	const FGuid& GetBuildPageID() const;

	template<typename TObjectType>
	TObjectType& CastDocumentObjectChecked() const
	{
		UObject* Owner = DocumentInterface.GetObject();
		return *CastChecked<TObjectType>(Owner);
	}

	// Generates and returns new class name for the given builder's document. Should ONLY be called on new assets
	// using transient frontend builder, as using a persistent builder registered with the DocumentBuilderRegistry
	// may result in corrupt records keyed off of undefined class name being potentially generated over.  In addition,
	// this can potentially leave existing node references in an abandoned state to this class causing MetaSound generator
	// build errors.
	FMetasoundFrontendClassName GenerateNewClassName();

	const Metasound::Frontend::FDocumentModifyDelegates& GetDocumentDelegates() const;

	UE_DEPRECATED(5.5, "Use GetConstDocumentInterfaceChecked instead")
	const IMetaSoundDocumentInterface& GetDocumentInterface() const;
	FMetasoundAssetBase& GetMetasoundAsset() const;

	int32 GetTransactionCount() const;

	TArray<const FMetasoundFrontendNode*> GetGraphInputTemplateNodes(FName InInputName);
	EMetasoundFrontendVertexAccessType GetNodeInputAccessType(const FGuid& InNodeID, const FGuid& InVertexID) const;
	const FMetasoundFrontendLiteral* GetNodeInputClassDefault(const FGuid& InNodeID, const FGuid& InVertexID) const;
	const FMetasoundFrontendLiteral* GetNodeInputDefault(const FGuid& InNodeID, const FGuid& InVertexID) const;
	EMetasoundFrontendVertexAccessType GetNodeOutputAccessType(const FGuid& InNodeID, const FGuid& InVertexID) const;

	// Initializes the builder's document, using the (optional) provided document template, (optional) class name, and (optionally) whether or not to reset the existing class version.
	void InitDocument(const FMetasoundFrontendDocument* InDocumentTemplate = nullptr, const FMetasoundFrontendClassName* InNewClassName = nullptr, bool bResetVersion = true);

	// Initializes GraphClass Metadata, optionally resetting the version back to 1.0 and/or creating a unique class name if a name is not provided.
	static void InitGraphClassMetadata(FMetasoundFrontendClassMetadata& InOutMetadata, bool bResetVersion = false, const FMetasoundFrontendClassName* NewClassName = nullptr);
	void InitGraphClassMetadata(bool bResetVersion, const FMetasoundFrontendClassName* NewClassName);

	void InitNodeLocations();

	UE_DEPRECATED(5.5, "Use invalidate overload that is provided new version of modify delegates")
	void InvalidateCache() { }

	bool IsDependencyReferenced(const FGuid& InClassID) const;
	bool IsNodeInputConnected(const FGuid& InNodeID, const FGuid& InVertexID) const;
	bool IsNodeOutputConnected(const FGuid& InNodeID, const FGuid& InVertexID) const;

	bool IsInterfaceDeclared(FName InInterfaceName) const;
	bool IsInterfaceDeclared(const FMetasoundFrontendVersion& InInterfaceVersion) const;
	bool IsPreset() const;

	// Returns whether or not builder is attached to a DocumentInterface and is valid to build or act on a document.
	bool IsValid() const;

	// Returns whether or not the given edge is valid (i.e. represents an input and output that equate in data and access types) or malformed.
	// Note that this does not return whether or not the given edge exists, but rather if it could be legally applied to the given edge vertices.
	Metasound::Frontend::EInvalidEdgeReason IsValidEdge(const FMetasoundFrontendEdge& InEdge) const;

	bool ModifyInterfaces(Metasound::Frontend::FModifyInterfaceOptions&& InOptions);

	UE_DEPRECATED(5.5,
		"Cache invalidation may require new copy of delegates. In addition, re-priming is discouraged. "
		"To enforce this, new recommended pattern is to construct a new builder instead")
	void ReloadCache();

	bool RemoveDependency(const FGuid& InClassID);
	bool RemoveDependency(EMetasoundFrontendClassType ClassType, const FMetasoundFrontendClassName& InClassName, const FMetasoundFrontendVersionNumber& InClassVersionNumber);
	bool RemoveEdge(const FMetasoundFrontendEdge& EdgeToRemove);

	// Removes all edges connected to an input or output vertex associated with the node of the given ID.
	bool RemoveEdges(const FGuid& InNodeID);

	bool RemoveEdgesByNodeClassInterfaceBindings(const FGuid& InOutputNodeID, const FGuid& InInputNodeID);
	bool RemoveEdgesFromNodeOutput(const FGuid& InNodeID, const FGuid& InVertexID);
	bool RemoveEdgeToNodeInput(const FGuid& InNodeID, const FGuid& InVertexID);

#if WITH_EDITOR
	bool RemoveGraphComment(const FGuid& InCommentID);
#endif // WITH_EDITOR

	bool RemoveGraphInput(FName InInputName);
	bool RemoveGraphOutput(FName InOutputName);
	bool RemoveInterface(FName InName);
	bool RemoveNamedEdges(const TSet<Metasound::Frontend::FNamedEdge>& InNamedEdgesToRemove, TArray<FMetasoundFrontendEdge>* OutRemovedEdges = nullptr);
	bool RemoveNode(const FGuid& InNodeID);

#if WITH_EDITOR
	int32 RemoveNodeLocation(const FGuid& InNodeID, const FGuid* InLocationGuid = nullptr);
#endif // WITH_EDITOR

	void Reload(TSharedPtr<Metasound::Frontend::FDocumentModifyDelegates> Delegates = {}, bool bPrimeCache = false);

	bool RemoveNodeInputDefault(const FGuid& InNodeID, const FGuid& InVertexID);
	bool RemoveUnusedDependencies();

	UE_DEPRECATED(5.5, "Use GenerateNewClassName instead")
	bool RenameRootGraphClass(const FMetasoundFrontendClassName& InName);

#if WITH_EDITOR
	void SetAuthor(const FString& InAuthor);

#endif // WITH_EDITOR

	// Sets the builder's targeted paged graph ID to the given ID if it exists.
	// Returns true if the builder is already targeting the given ID or if it successfully
	// found a page implementation with the given ID and was able to switch to it, false if not.
	// Swapping the targeted build graph ID clears the local cache, so swapping frequently can
	// induce cash thrashing.
	bool SetBuildPageID(const FGuid& InBuildPageID);

	bool SetGraphInputAccessType(FName InputName, EMetasoundFrontendVertexAccessType AccessType);
	bool SetGraphInputDataType(FName InputName, FName DataType);
	bool SetGraphInputDefault(FName InputName, const FMetasoundFrontendLiteral& InDefaultLiteral);

	bool SetGraphOutputAccessType(FName OutputName, EMetasoundFrontendVertexAccessType AccessType);
	bool SetGraphOutputDataType(FName OutputName, FName DataType);

#if WITH_EDITOR
	void SetDisplayName(const FText& InDisplayName);

	void SetMemberMetadata(UMetaSoundFrontendMemberMetadata& NewMetadata);

	// Sets the editor-only comment to the provided value.
	// Returns true if the node was found and the comment was updated, false if not.
	bool SetNodeComment(const FGuid& InNodeID, FString&& InNewComment);

	// Sets the editor-only comment visibility.
	// Returns true if the node was found and the visibility was set, false if not.
	bool SetNodeCommentVisible(const FGuid& InNodeID, bool bIsVisible);

	// Sets the editor-only node location of a node with the given ID to the provided location.
	// Returns true if the node was found and the location was updated, false if not.
	bool SetNodeLocation(const FGuid& InNodeID, const FVector2D& InLocation, const FGuid* InLocationGuid = nullptr);
#endif // WITH_EDITOR

	bool SetNodeInputDefault(const FGuid& InNodeID, const FGuid& InVertexID, const FMetasoundFrontendLiteral& InLiteral);

	// Sets the document's version number.  Should only be called by document versioning.
	void SetVersionNumber(const FMetasoundFrontendVersionNumber& InDocumentVersionNumber);

	bool SwapGraphInput(const FMetasoundFrontendClassVertex& InExistingInputVertex, const FMetasoundFrontendClassVertex& NewInputVertex);
	bool SwapGraphOutput(const FMetasoundFrontendClassVertex& InExistingOutputVertex, const FMetasoundFrontendClassVertex& NewOutputVertex);
	bool UpdateDependencyClassNames(const TMap<FMetasoundFrontendClassName, FMetasoundFrontendClassName>& OldToNewReferencedClassNames);

#if WITH_EDITORONLY_DATA
	// Transforms template nodes within the given builder's document, which can include swapping associated edges and/or
	// replacing nodes with other, registry-defined concrete node class instances. Returns true if any template nodes were processed.
	bool TransformTemplateNodes();

	// Versions legacy document members that contained interface information
	bool VersionInterfaces();
#endif // WITH_EDITORONLY_DATA

private:
	using FFinalizeNodeFunctionRef = TFunctionRef<void(FMetasoundFrontendNode&, const Metasound::Frontend::FNodeRegistryKey&)>;

	FMetasoundFrontendNode* AddNodeInternal(const FMetasoundFrontendClassMetadata& InClassMetadata, Metasound::Frontend::FFinalizeNodeFunctionRef FinalizeNode, FGuid InNodeID = FGuid::NewGuid(), int32* NewNodeIndex = nullptr, const FGuid* InGraphPageID = nullptr);
	void BeginBuilding(TSharedPtr<Metasound::Frontend::FDocumentModifyDelegates> Delegates = {}, bool bPrimeCache = false);

	// Conforms GraphOutput node's ClassID, Access & Data Type with the GraphOutput.
	// creating and removing dependencies as necessary within the document dependency array. Does *NOT*
	// modify edge data (i.e. if the DataType is changed on the given node and it has corresponding
	// edges, edges may then be invalid due to access type/DataType incompatibility).
	bool ConformGraphInputNodeToClass(const FMetasoundFrontendClassInput& GraphInput);

	// Conforms GraphOutput node's ClassID, Access & Data Type with the GraphOutput.
	// Creates and removes dependencies as necessary within the document dependency array. Does *NOT*
	// modify edge data (i.e. if the DataType is changed on the given node and it has corresponding
	// edges, edges may then be invalid due to access type/DataType incompatibility).
	bool ConformGraphOutputNodeToClass(const FMetasoundFrontendClassOutput& GraphOutput);

	bool FindNodeClassInterfaces(const FGuid& InNodeID, TSet<FMetasoundFrontendVersion>& OutInterfaces) const;
	FMetasoundFrontendNode* FindNodeInternal(const FGuid& InNodeID);

	void IterateNodesConnectedWithVertex(const FMetasoundFrontendVertexHandle& Vertex, TFunctionRef<void(const FMetasoundFrontendEdge&, FMetasoundFrontendNode&)> NodeIndexIterFunc);

	const FTopLevelAssetPath GetBuilderClassPath() const;
	FMetasoundFrontendDocument& GetDocumentChecked() const;
	IMetaSoundDocumentInterface& GetDocumentInterfaceChecked() const;

	bool SetGraphInputInheritsDefault(FName InName, bool bInputInheritsDefault);

	bool SpliceVariableNodeFromStack(const FGuid& InNodeID);
	bool UnlinkVariableNode(const FGuid& InNodeID);

	UPROPERTY(Transient)
	TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface;

	FGuid BuildPageID;

	TSharedPtr<Metasound::Frontend::IDocumentCache> DocumentCache;
	TSharedPtr<Metasound::Frontend::FDocumentModifyDelegates> DocumentDelegates;
};
