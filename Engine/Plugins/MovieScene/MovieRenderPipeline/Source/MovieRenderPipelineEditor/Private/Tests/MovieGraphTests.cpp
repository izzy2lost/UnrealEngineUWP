// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphEditorTestUtilities.h"

#include "Algo/RemoveIf.h"
#include "Graph/MovieGraphConfig.h"
#include "Graph/Nodes/MovieGraphBranchNode.h"
#include "Graph/Nodes/MovieGraphInputNode.h"
#include "Graph/Nodes/MovieGraphOutputNode.h"
#include "Graph/Nodes/MovieGraphSubgraphNode.h"
#include "Graph/Nodes/MovieGraphVariableNode.h"

#include "Misc/AutomationTest.h"

namespace UE::MovieGraph::Private::Tests
{
	void FilterOutUndesirableClasses(TArray<UClass*>& InClassArray)
	{
		static const TSet<UClass*> UndesirableClasses = {
			UMovieGraphVariableNode::StaticClass(),	// Will be tested separately in this test
			UMovieGraphInputNode::StaticClass(),	// Should not be able to be added via API
			UMovieGraphOutputNode::StaticClass(),	// Should not be able to be added via API
			UMovieGraphSubgraphNode::StaticClass()	// Will be tested separately in another test
		};

		Algo::StableRemoveIf(InClassArray,
			[&](const UClass* Class)
			{
				return UndesirableClasses.Contains(Class);
			});
	}

	/**
	* 1) Add a null node, verify that NumNodes == 0
	* 2) Add an unsupported node, verify that NumNodes == 0
	* 3) Add a supported node, Verify that NumNodes == NumSupportedNodes
	* 4) Add a variable and variable node, verify the variable node exists includes the variable member
	*/
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAddGraphNodeTest, "VirtualProduction.MovieGraph.AddGraphNodeTest", (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter));
	bool FAddGraphNodeTest::RunTest(const FString& Parameters)
	{
		// Create Config
		UMovieGraphConfig* GraphConfig = CreateNewMovieGraphConfig("AddGraphNodeTest");
		TestTrue(TEXT("MovieGraphConfig successfully created"), GraphConfig != nullptr);
		if (!GraphConfig)
		{
			return false;
		}

		GraphConfig->ConstructRuntimeNode<UMovieGraphNode>(nullptr);
		TestTrue(TEXT("Null Node not added"), GraphConfig->GetNodes().IsEmpty());

		// Ensure input and output nodes can't be added via the API
		const UMovieGraphInputNode* TestInputRuntimeNode = GraphConfig->ConstructRuntimeNode<UMovieGraphInputNode>(UMovieGraphInputNode::StaticClass());
		if(TestInputRuntimeNode && GraphConfig->GetNodes().Contains(TestInputRuntimeNode))
		{
			AddError(*FString::Printf(TEXT("Graph allowed user construction of %s"), *TestInputRuntimeNode->GetClass()->GetName()));
		}
		const UMovieGraphOutputNode* TestOutputRuntimeNode = GraphConfig->ConstructRuntimeNode<UMovieGraphOutputNode>(UMovieGraphOutputNode::StaticClass());
		if(TestOutputRuntimeNode && GraphConfig->GetNodes().Contains(TestOutputRuntimeNode))
		{
			AddError(*FString::Printf(TEXT("Graph allowed user construction of %s"), *TestOutputRuntimeNode->GetClass()->GetName()));
		}
		
		// Add one node of each type, excepting variables, inputs and outputs
		TArray<UClass*> AllNodeClasses = GetAllDerivedClasses(UMovieGraphNode::StaticClass(), true);
		FilterOutUndesirableClasses(AllNodeClasses);
		for (UClass* Class : AllNodeClasses)
		{
			GraphConfig->ConstructRuntimeNode<UMovieGraphNode>(Class);
		}
		TestTrue(TEXT("All supported nodes types added"), GraphConfig->GetNodes().Num() == AllNodeClasses.Num());

		// Test adding a variable node
		UMovieGraphVariable* Variable = GraphConfig->AddVariable(TEXT("A_Cool_Variable"));
		TestTrue(TEXT("Variable member added"), Variable != nullptr && GraphConfig->GetVariables().Contains(Variable));
		if (!Variable)
		{
			return false;
		}
		UMovieGraphVariableNode* VariableNode = GraphConfig->ConstructRuntimeNode<UMovieGraphVariableNode>(UMovieGraphVariableNode::StaticClass());
		TestTrue(TEXT("Variable node type added"), VariableNode != nullptr && GraphConfig->GetNodes().Contains(VariableNode));
		if (!VariableNode)
		{
			return false;
		}
		VariableNode->SetVariable(Variable);
		TestTrue(TEXT("Variable node variable member set"), VariableNode->GetVariable() == Variable);

		// Open graph config in editor if desired
		//OpenGraphConfigInEditor(GraphConfig);

		return true;
	}

	/**
	* 1) Add two nodes to the graph
	* 2) Remove one node, verify NumNodes == 1
	* 3) Try to remove a nullptr, verify NumNodes == 1
	* 4) Try to remove a node that doesn't exist in the graph, verify NumNodes == 1
	* 5) Remove a variable, variable node, and verify it's not in GetNodes()
	*/
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRemoveGraphNodeTest, "VirtualProduction.MovieGraph.RemoveGraphNodeTest", (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter));
	bool FRemoveGraphNodeTest::RunTest(const FString& Parameters)
	{
		// Create Config
		UMovieGraphConfig* GraphConfig = CreateNewMovieGraphConfig("RemoveGraphNodeTest");
		TestTrue(TEXT("MovieGraphConfig successfully created"), GraphConfig != nullptr);
		if (!GraphConfig)
		{
			return false;
		}
		
		// Add two nodes
		TArray<UClass*> AllNodeClasses = GetAllDerivedClasses(UMovieGraphNode::StaticClass(), true);
		FilterOutUndesirableClasses(AllNodeClasses);
		UMovieGraphNode* NodeOne = GraphConfig->ConstructRuntimeNode<UMovieGraphNode>(AllNodeClasses[0]);
		UMovieGraphNode* NodeTwo = GraphConfig->ConstructRuntimeNode<UMovieGraphNode>(AllNodeClasses[1]);
		if (!NodeOne || !NodeTwo)
		{
			AddError(*FString::Printf(TEXT("Unable to create two nodes.")));
			return false;
		}
		TestTrue(
			FString::Printf(TEXT("Node types added: %s and %s"), *AllNodeClasses[0]->GetName(), *AllNodeClasses[1]->GetName()),
			GraphConfig->GetNodes().Num() == 2);

		// Test removing one node
		GraphConfig->RemoveNode(NodeOne);
		TestTrue(TEXT("Node removed"), GraphConfig->GetNodes().Num() == 1);

		// Test removing a nullptr
		GraphConfig->RemoveNode(nullptr);
		TestTrue(TEXT("Calling RemoveNode with nullptr should do nothing"), GraphConfig->GetNodes().Num() == 1);

		// Test removing a node which doesn't exist in the graph
		GraphConfig->RemoveNode(NodeOne);
		TestTrue(TEXT("Attempting to remove node which does not exist in the graph should do nothing"), GraphConfig->GetNodes().Num() == 1);
		
		// Test adding and removing a variable member and variable node
		UMovieGraphVariable* Variable = GraphConfig->AddVariable(TEXT("A_Cool_Variable"));
		TestTrue(TEXT("Variable member added"), Variable != nullptr && GraphConfig->GetVariables().Contains(Variable));
		if (!Variable)
		{
			return false;
		}
		
		UMovieGraphVariableNode* VariableNode = GraphConfig->ConstructRuntimeNode<UMovieGraphVariableNode>(UMovieGraphVariableNode::StaticClass());
		TestTrue(TEXT("Variable node type added"), VariableNode != nullptr && GraphConfig->GetNodes().Contains(VariableNode));
		if (!VariableNode)
		{
			return false;
		}
		
		VariableNode->SetVariable(Variable);
		TestTrue(TEXT("Variable node variable member set"), VariableNode->GetVariable() == Variable);
		
		VariableNode->SetVariable(nullptr);
		TestTrue(TEXT("Calling SetVariable with nullptr should do nothing"), VariableNode->GetVariable() == Variable);
		
		GraphConfig->RemoveNode(VariableNode);
		TestTrue(TEXT("Variable node removed"), GraphConfig->GetNodes().Num() == 1);
		
		GraphConfig->DeleteMember(Variable);
		TestTrue(TEXT("Variable member removed"), !GraphConfig->GetVariables().Contains(Variable));

		// Open graph config in editor if desired
		//OpenGraphConfigInEditor(GraphConfig);

		return true;
	}
	
	/**
	* 1) Add nodes to the graph, verify that NumNodes == AllNodeClasses
	* 2) Connect nodes together
	* 3) Use the Pin API to verify that the connection is between the correct nodes
	*/
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGraphNodeAddEdgeTest, "VirtualProduction.MovieGraph.GraphNodeAddEdgeTest", (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter));
	bool FGraphNodeAddEdgeTest::RunTest(const FString& Parameters)
	{
		// Create Config
		UMovieGraphConfig* GraphConfig = CreateNewMovieGraphConfig("GraphNodeAddEdgeTest");
		TestTrue(TEXT("MovieGraphConfig successfully created"), GraphConfig != nullptr);
		if (!GraphConfig)
		{
			return false;
		}

		// Add all nodes excepting variables, inputs and outputs
		TArray<UClass*> AllNodeClasses = GetAllDerivedClasses(UMovieGraphNode::StaticClass(), true);
		FilterOutUndesirableClasses(AllNodeClasses);
		for (UClass* Class : AllNodeClasses)
		{
			GraphConfig->ConstructRuntimeNode<UMovieGraphNode>(Class);
		}
		TestTrue(TEXT("All supported nodes types added"), GraphConfig->GetNodes().Num() == AllNodeClasses.Num());

		// Connect all edges		
		// Some nodes will be duplicated in between these arrays
		
		TArray<TObjectPtr<UMovieGraphNode>> NodesWithOutputs = {GraphConfig->GetInputNode()};
		TArray<TObjectPtr<UMovieGraphNode>> NodesWithInputs = {GraphConfig->GetOutputNode()};
		
		auto SeparateNodesIntoInputAndOutputTypes = [&]()
		{
			for (const TObjectPtr<UMovieGraphNode>& Node : GraphConfig->GetNodes())
			{
				if (!Node->GetInputPins().IsEmpty())
				{
					NodesWithInputs.Add(Node);
				}

				if (!Node->GetOutputPins().IsEmpty())
				{
					NodesWithOutputs.Add(Node);
				}

				const bool bHasNoInputsOrOutputs = Node->GetInputPins().Num() == 0 && Node->GetOutputPins().Num() == 0;
			
				TestTrue(*FString::Printf(TEXT("Node %s has inputs or outputs"), *Node.GetName()), !bHasNoInputsOrOutputs);
			}
		};
		SeparateNodesIntoInputAndOutputTypes();

		auto ConnectAllEdgesToAllNodes = [&]()
		{
			for (const TObjectPtr<UMovieGraphNode>& NodeFrom : NodesWithOutputs)
			{
				for (const TObjectPtr<UMovieGraphPin>& OutputPin : NodeFrom->GetOutputPins())
				{
					for (const TObjectPtr<UMovieGraphNode>& NodeTo : NodesWithInputs)
					{
						if (NodeFrom == NodeTo) // Don't try to self-connect
						{
							continue;
						}
					
						for (const TObjectPtr<UMovieGraphPin>& InputPin : NodeTo->GetInputPins())
						{
							if (OutputPin->CanCreateConnection(InputPin))
							{
								GraphConfig->AddLabeledEdge(
									NodeFrom, OutputPin->Properties.Label,
									NodeTo, InputPin->Properties.Label);

								TestTrue(*FString::Printf(TEXT("Edge added between %s and %s"), *NodeFrom->GetName(), *NodeTo->GetName()),
									OutputPin->GetAllConnectedPins().Contains(InputPin));
							}
						}
					}
				}
			}
		};
		ConnectAllEdgesToAllNodes();
		
		// Open graph config in editor if desired
		//OpenGraphConfigInEditor(GraphConfig);
		
		return true;
	}
	
	/**
	* 1) Add a node, connect it with default input and output nodes
	* 2) Remove an edge
	* 3) Use the Pin API to verify no more connection
	*/
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGraphNodeRemoveEdgeTest, "VirtualProduction.MovieGraph.GraphNodeRemoveEdgeTest", (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter));
	bool FGraphNodeRemoveEdgeTest::RunTest(const FString& Parameters)
	{
		// Create Config
		UMovieGraphConfig* GraphConfig = CreateNewMovieGraphConfig("GraphNodeRemoveEdgeTest");
		TestTrue(TEXT("MovieGraphConfig successfully created"), GraphConfig != nullptr);
		if (!GraphConfig)
		{
			return false;
		}

		// Add a node with an input and output for connection
		UMovieGraphNode* MiddleNode =
			GraphConfig->ConstructRuntimeNode<UMovieGraphBranchNode>(UMovieGraphBranchNode::StaticClass());
		TestTrue(TEXT("MiddleNode successfully added"), GraphConfig->GetNodes().Contains(MiddleNode));
		if (!MiddleNode)
		{
			return false;
		}

		UMovieGraphNode* InputNode = GraphConfig->GetInputNode();
		UMovieGraphNode* OutputNode = GraphConfig->GetOutputNode();
		if (!InputNode || !OutputNode)
		{
			AddError(*FString::Printf(TEXT("Unable to find Input and Output nodes.")));
			return false;
		}

		const TObjectPtr<UMovieGraphPin> InputNodeOutputPin = InputNode->GetOutputPins()[0];
		const TObjectPtr<UMovieGraphPin> MiddleNodeInputPin = MiddleNode->GetInputPins()[0];
		const TObjectPtr<UMovieGraphPin> MiddleNodeOutputPin = MiddleNode->GetOutputPins()[0];
		const TObjectPtr<UMovieGraphPin> OutputNodeInputPin = OutputNode->GetInputPins()[0];

		if (!InputNodeOutputPin || !MiddleNodeInputPin || !MiddleNodeOutputPin || !OutputNodeInputPin)
		{
			AddError(*FString::Printf(TEXT("Unable to find all Pins for Input, Middle and Output Nodes.")));
			return false;
		}

		// Add first edge
		GraphConfig->AddLabeledEdge(
			InputNode, InputNodeOutputPin->Properties.Label,
			MiddleNode, MiddleNodeInputPin->Properties.Label);

		TestTrue(*FString::Printf(TEXT("Edge added between %s and %s"), *InputNode->GetName(), *MiddleNode->GetName()),
			InputNodeOutputPin->GetAllConnectedPins().Contains(MiddleNodeInputPin));

		// Remove edge
		GraphConfig->RemoveEdge(InputNode, InputNodeOutputPin->Properties.Label,
			MiddleNode, MiddleNodeInputPin->Properties.Label);

		TestTrue(*FString::Printf(TEXT("Edge removed between %s and %s"), *InputNode->GetName(), *MiddleNode->GetName()),
			!InputNodeOutputPin->GetAllConnectedPins().Contains(MiddleNodeInputPin));

		// Add second edge
		GraphConfig->AddLabeledEdge(
			MiddleNode, MiddleNodeOutputPin->Properties.Label,
			OutputNode, OutputNodeInputPin->Properties.Label);

		TestTrue(*FString::Printf(TEXT("Edge added between %s and %s"), *MiddleNode->GetName(), *OutputNode->GetName()),
			MiddleNodeOutputPin->GetAllConnectedPins().Contains(OutputNodeInputPin));

		// Remove edge
		GraphConfig->RemoveEdge(
			MiddleNode, MiddleNodeOutputPin->Properties.Label,
			OutputNode, OutputNodeInputPin->Properties.Label);

		TestTrue(*FString::Printf(TEXT("Edge added between %s and %s"), *MiddleNode->GetName(), *OutputNode->GetName()),
			!MiddleNodeOutputPin->GetAllConnectedPins().Contains(OutputNodeInputPin));

		// Open graph config in editor if desired
		//OpenGraphConfigInEditor(GraphConfig);
		
		return true;
	}
}