// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/CodeGenerator_FirstPass.h"

#include "MuT/CodeGenerator.h"
#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/MutableTrace.h"
#include "MuR/Operations.h"
#include "MuR/Parameters.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Platform.h"
#include "MuT/AST.h"
#include "MuT/ASTOpConstantBool.h"
#include "MuT/ASTOpParameter.h"
#include "MuT/CompilerPrivate.h"
#include "MuT/ErrorLog.h"
#include "MuT/ErrorLogPrivate.h"
#include "MuT/NodeComponent.h"
#include "MuT/NodeModifierMeshClipDeformPrivate.h"
#include "MuT/NodeModifierMeshClipMorphPlanePrivate.h"
#include "MuT/NodeModifierMeshClipWithMeshPrivate.h"
#include "MuT/NodeModifierMeshClipWithUVMaskPrivate.h"
#include "MuT/NodeObject.h"
#include "MuT/NodeObjectGroupPrivate.h"
#include "MuT/NodeObjectNew.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeSurface.h"
#include "MuT/NodeSurfaceEdit.h"
#include "MuT/NodeSurfaceNew.h"
#include "MuT/NodeSurfaceVariation.h"
#include "MuT/NodeSurfaceSwitch.h"
#include "MuT/NodeScalarEnumParameterPrivate.h"

namespace mu
{

	//---------------------------------------------------------------------------------------------
	FirstPassGenerator::FirstPassGenerator()
	{
		// Default conditions when there is no restriction accumulated.
		FConditionContext noCondition;
        CurrentCondition.Add(noCondition);
        CurrentStateCondition.Add(StateCondition());
	}


	//---------------------------------------------------------------------------------------------
    void FirstPassGenerator::Generate( Ptr<mu::ErrorLog> InErrorLog,
                                       const Node* Root,
									   bool bIgnoreStates,
									   CodeGenerator* InGenerator )
	{
		MUTABLE_CPUPROFILER_SCOPE(FirstPassGenerate);

		Generator = InGenerator;
		ErrorLog = InErrorLog;

		// Step 1: collect all objects, surfaces and object conditions
        if (Root)
		{
 			Generate_Generic(Root);
 		}

		// Step 2: Collect all tags and a list of the surfaces that activate them
		for (int32 s=0; s<Surfaces.Num(); ++s)
		{
			// Collect the tags in new surfaces
			for (int32 t=0; t< Surfaces[s].Node->Tags.Num(); ++t)
			{
				int32 tag = -1;
                const FString& tagStr = Surfaces[s].Node->Tags[t];
                for (int32 i = 0; i<Tags.Num() && tag<0; ++i)
				{
                    if (Tags[i].Tag == tagStr)
					{
						tag = i;
					}
				}

				// New tag?
				if (tag < 0)
				{
                    tag = Tags.Num();
					FTag newTag;
                    newTag.Tag = tagStr;
                    Tags.Add(newTag);
				}

                if (Tags[tag].Surfaces.Find(s)==INDEX_NONE)
				{
                    Tags[tag].Surfaces.Add(s);
				}
			}

            // Collect the tags in edit surfaces
            for (int32 e=0; e< Surfaces[s].Edits.Num(); ++e)
            {
                const FSurface::FEdit& edit = Surfaces[s].Edits[e];
                for (int32 t=0; t<edit.Node->Tags.Num(); ++t)
                {
                    int32 tag = -1;
					const FString& tagStr = edit.Node->Tags[t];

                    for (int32 i = 0; i<Tags.Num() && tag<0; ++i)
                    {
                        if (Tags[i].Tag == tagStr)
                        {
                            tag = i;
                        }
                    }

                    // New tag?
                    if (tag < 0)
                    {
                        tag = Tags.Num();
						FTag newTag;
                        newTag.Tag = tagStr;
                        Tags.Add(newTag);
                    }

                    if (Tags[tag].Edits.Find({s,e}) == INDEX_NONE)
                    {
                        Tags[tag].Edits.Add({s,e});
                    }
                }
            }

		}

        // Step 3: Create default state if necessary
        if ( bIgnoreStates )
        {
            States.Empty();
        }

        if ( States.IsEmpty() )
        {
            FObjectState data;
            data.Name = "Default";
            States.Emplace( data, Root );
        }
	}


	void FirstPassGenerator::Generate_Generic(const Node* Root)
	{
		if (!Root)
		{
			return;
		}

		if (Root->GetType()==NodeSurfaceNew::GetStaticType())
		{
			Generate_SurfaceNew(static_cast<const NodeSurfaceNew*>(Root));
		}
		else if (Root->GetType() == NodeSurfaceEdit::GetStaticType())
		{
			Generate_SurfaceEdit(static_cast<const NodeSurfaceEdit*>(Root));
		}
		else if (Root->GetType() == NodeSurfaceVariation::GetStaticType())
		{
			Generate_SurfaceVariation(static_cast<const NodeSurfaceVariation*>(Root));
		}
		else if (Root->GetType() == NodeSurfaceSwitch::GetStaticType())
		{
			Generate_SurfaceSwitch(static_cast<const NodeSurfaceSwitch*>(Root));
		}
		else if (Root->GetType() == NodeComponentNew::GetStaticType())
		{
			Generate_ComponentNew(static_cast<const NodeComponentNew*>(Root));
		}
		else if (Root->GetType() == NodeComponentEdit::GetStaticType())
		{
			Generate_ComponentEdit(static_cast<const NodeComponentEdit*>(Root));
		}
		else if (Root->GetType() == NodeComponentSwitch::GetStaticType())
		{
			Generate_ComponentSwitch(static_cast<const NodeComponentSwitch*>(Root));
		}
		else if (Root->GetType() == NodeComponentVariation::GetStaticType())
		{
			Generate_ComponentVariation(static_cast<const NodeComponentVariation*>(Root));
		}
		else if (Root->GetType() == NodeObjectNew::GetStaticType())
		{
			Generate_ObjectNew(static_cast<const NodeObjectNew*>(Root));
		}
		else if (Root->GetType() == NodeObjectGroup::GetStaticType())
		{
			Generate_ObjectGroup(static_cast<const NodeObjectGroup*>(Root));
		}
		else if (Root->GetType() == NodeLOD::GetStaticType())
		{
			Generate_LOD(static_cast<const NodeLOD*>(Root));
		}
		else if (Root->GetType() == NodeModifier::GetStaticType())
		{
			Generate_Modifier(static_cast<const NodeModifier*>(Root));
		}
		else
		{
			check(false);
		}
	}

	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_Modifier(const NodeModifier* InNode)
	{
		check(CurrentLOD>=0);

		// Add the data about this modifier
		FModifier thisData;
		thisData.Node = InNode;
		thisData.ObjectCondition = CurrentCondition.Last().ObjectCondition;
		thisData.StateCondition = CurrentStateCondition.Last();
		thisData.LOD = CurrentLOD;
		thisData.PositiveTags = CurrentPositiveTags;
		thisData.NegativeTags = CurrentNegativeTags;
		Modifiers.Add(thisData);
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_SurfaceNew(const NodeSurfaceNew* InNode)
	{
		// Add the data about this surface
		FSurface thisData;
		thisData.Node = InNode;
		thisData.Component = CurrentComponent;
		thisData.LOD = CurrentLOD;
		thisData.ObjectCondition = CurrentCondition.Last().ObjectCondition;
		thisData.StateCondition = CurrentStateCondition.Last();
		thisData.PositiveTags = CurrentPositiveTags;
		thisData.NegativeTags = CurrentNegativeTags;
		Surfaces.Add(thisData);
	}


	//---------------------------------------------------------------------------------------------
    void FirstPassGenerator::Generate_SurfaceEdit(const NodeSurfaceEdit* InNode)
	{
		// Store a reference to this node in the surface data for the surface that this node is
		// editing.
		FSurface* Surface = Surfaces.FindByPredicate([&InNode](const FSurface& s)
        {
            // Are we editing the main surface node of this surface?
            if (s.Node.get() == InNode->Parent.get()) return true;

            // Are we editing an edit node modifying this surface?
            for (const FSurface::FEdit& e: s.Edits)
            {
                if (InNode->Parent && e.Node== InNode->Parent)
                {
                    return true;
                }
            }

            return false;
        });
		
		// The surface could be missing if the parent is not in the hierarchy. This could happen
		// with wrong input or in case of partial models for preview.
		if (Surface)
		{
			FSurface::FEdit edit;
			edit.PositiveTags = CurrentPositiveTags;
			edit.NegativeTags = CurrentNegativeTags;
            edit.Node = InNode;
            edit.Condition = CurrentCondition.Last().ObjectCondition;
			Surface->Edits.Add(edit);
		}
		else
		{
			ErrorLog->GetPrivate()->Add("Missing parent object for edit node.", ELMT_WARNING, InNode->GetMessageContext());
		}
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_SurfaceVariation(const NodeSurfaceVariation* InNode)
	{
        switch(InNode->Type)
        {

        case NodeSurfaceVariation::VariationType::Tag:
        {
            // Any of the tags in the variations would prevent the default surface
			TArray<FString> OldNegativeTags = CurrentNegativeTags;
            for (int32 v=0; v< InNode->Variations.Num(); ++v)
            {
                CurrentNegativeTags.Add(InNode->Variations[v].Tag);
            }

            for(const Ptr<NodeSurface>& n: InNode->DefaultSurfaces)
            {
				Generate_Generic(n.get());
			}
            for(const Ptr<NodeModifier>& n: InNode->DefaultModifiers)
            {
				Generate_Modifier(n.get());
            }

            CurrentNegativeTags = OldNegativeTags;

            for (int32 v=0; v< InNode->Variations.Num(); ++v)
            {
                CurrentPositiveTags.Add(InNode->Variations[v].Tag);
                for (const Ptr<NodeSurface>& s : InNode->Variations[v].Surfaces)
                {
					Generate_Generic(s.get());
                }

                for (const Ptr<NodeModifier>& s : InNode->Variations[v].Modifiers)
                {
					Generate_Modifier(s.get());
				}

                CurrentPositiveTags.Pop();

                // Tags have an order in a variation node: the current tag should prevent any following
                // variation surface
                CurrentNegativeTags.Add(InNode->Variations[v].Tag);
            }

            CurrentNegativeTags = OldNegativeTags;

            break;
        }


        case NodeSurfaceVariation::VariationType::State:
        {
            int32 stateCount = States.Num();

            // Default
            {
                // Store the states for the default branch here
                StateCondition defaultStates;
                {
					StateCondition AllTrue;
					AllTrue.Init(true,stateCount);
                    defaultStates = CurrentStateCondition.Last().IsEmpty()
                            ? AllTrue
                            : CurrentStateCondition.Last();

                    for (const NodeSurfaceVariation::FVariation& v: InNode->Variations)
                    {
                        for( size_t s=0; s<stateCount; ++s )
                        {
                            if (States[s].Key.Name==v.Tag)
                            {
                                // Remove this state from the default options, since it has its own variation
                                defaultStates[s] = false;
                            }
                        }
                    }
                }

                CurrentStateCondition.Add(defaultStates);

                for (const Ptr<NodeSurface>& n : InNode->DefaultSurfaces)
                {
					Generate_Generic(n.get());
				}
                for (const Ptr<NodeModifier>& n : InNode->DefaultModifiers)
                {
					Generate_Modifier(n.get());
				}

                CurrentStateCondition.Pop();
            }

            // Variation branches
            for (const auto& v: InNode->Variations)
            {
                // Store the states for this variation here
				StateCondition variationStates;
				variationStates.Init(false,stateCount);

                for( int32 StateIndex=0; StateIndex<stateCount; ++StateIndex)
                {
                    if (States[StateIndex].Key.Name==v.Tag)
                    {
                        variationStates[StateIndex] = true;
                    }
                }

                CurrentStateCondition.Add(variationStates);

                for (const Ptr<NodeSurface>& n : v.Surfaces)
                {
					Generate_Generic(n.get());
				}
                for (const Ptr<NodeModifier>& n : v.Modifiers)
                {
					Generate_Modifier(n.get());
				}

                CurrentStateCondition.Pop();
            }

            break;
        }

        default:
            // Case not implemented.
            check(false);
            break;
        }
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_SurfaceSwitch(const NodeSurfaceSwitch* InNode)
	{
		if (InNode->Options.Num() == 0)
		{
			// No options in the switch!
			return;
		}

		// Prepare the enumeration parameter
		CodeGenerator::FGenericGenerationOptions Options;
		CodeGenerator::FScalarGenerationResult ScalarResult;
		if (InNode->Parameter)
		{
			Generator->GenerateScalar( ScalarResult, Options, InNode->Parameter );
		}
		else
		{
			// This argument is required
			ScalarResult.op = Generator->GenerateMissingScalarCode(TEXT("Switch variable"), 0.0f, InNode->GetMessageContext());
		}

		// Parse the options
		for (int32 t = 0; t < InNode->Options.Num(); ++t)
		{
			// Create a comparison operation as the boolean parameter for the child
			Ptr<ASTOpFixed> ParamOp = new ASTOpFixed();
			ParamOp->op.type = OP_TYPE::BO_EQUAL_INT_CONST;
			ParamOp->SetChild(ParamOp->op.args.BoolEqualScalarConst.value, ScalarResult.op);
			ParamOp->op.args.BoolEqualScalarConst.constant = (int16)t;

			// Combine the new condition with previous conditions coming from parent objects
			if (CurrentCondition.Last().ObjectCondition)
			{
				Ptr<ASTOpFixed> op = new ASTOpFixed();
				op->op.type = OP_TYPE::BO_AND;
				op->SetChild(op->op.args.BoolBinary.a, CurrentCondition.Last().ObjectCondition);
				op->SetChild(op->op.args.BoolBinary.b, ParamOp);
				ParamOp = op;
			}

			FConditionContext data;
			data.ObjectCondition = ParamOp;
			CurrentCondition.Push(data);

			if (InNode->Options[t])
			{
				Generate_Generic(InNode->Options[t].get());
			}

			CurrentCondition.Pop();
		}
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_ComponentNew(const NodeComponentNew* InNode)
	{
		// Add the data about this surface
		FComponent ThisData;
		ThisData.Component = InNode;
		ThisData.ObjectCondition = CurrentCondition.Last().ObjectCondition;
		ThisData.PositiveTags = CurrentPositiveTags;
		ThisData.NegativeTags = CurrentNegativeTags;
		Components.Add(ThisData);

        CurrentComponent = InNode;

		CurrentLOD = 0;
		for (const Ptr<NodeLOD>& c : InNode->LODs)
		{
			if (c)
			{
				Generate_LOD(c.get());
			}
			++CurrentLOD;
		}
		CurrentLOD = -1;

		CurrentComponent = nullptr;
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_ComponentEdit(const NodeComponentEdit* InNode)
	{
		CurrentComponent = InNode->GetParentComponentNew();

		CurrentLOD = 0;
		for (const Ptr<NodeLOD>& c : InNode->LODs)
		{
			if (c)
			{
				Generate_LOD(c.get());
			}
			++CurrentLOD;
		}
		CurrentLOD = -1;

		CurrentComponent = nullptr;
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_ComponentVariation(const NodeComponentVariation* InNode)
	{
		// Any of the tags in the variations would prevent the default surface
		TArray<FString> OldNegativeTags = CurrentNegativeTags;
		for (int32 v = 0; v < InNode->Variations.Num(); ++v)
		{
			CurrentNegativeTags.Add(InNode->Variations[v].Tag);
		}

		Generate_Generic(InNode->DefaultComponent.get());

		CurrentNegativeTags = OldNegativeTags;

		for (int32 v = 0; v < InNode->Variations.Num(); ++v)
		{
			CurrentPositiveTags.Add(InNode->Variations[v].Tag);
			Generate_Generic(InNode->Variations[v].Component.get());

			CurrentPositiveTags.Pop();

			// Tags have an order in a variation node: the current tag should prevent any following variation
			CurrentNegativeTags.Add(InNode->Variations[v].Tag);
		}

		CurrentNegativeTags = OldNegativeTags;
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_ComponentSwitch(const NodeComponentSwitch* InNode)
	{
		if (InNode->Options.Num() == 0)
		{
			// No options in the switch!
			return;
		}

		// Prepare the enumeration parameter
		CodeGenerator::FGenericGenerationOptions Options;
		CodeGenerator::FScalarGenerationResult ScalarResult;
		if (InNode->Parameter)
		{
			Generator->GenerateScalar(ScalarResult, Options, InNode->Parameter);
		}
		else
		{
			// This argument is required
			ScalarResult.op = Generator->GenerateMissingScalarCode(TEXT("Switch variable"), 0.0f, InNode->GetMessageContext());
		}

		// Parse the options
		for (int32 t = 0; t < InNode->Options.Num(); ++t)
		{
			// Create a comparison operation as the boolean parameter for the child
			Ptr<ASTOpFixed> ParamOp = new ASTOpFixed();
			ParamOp->op.type = OP_TYPE::BO_EQUAL_INT_CONST;
			ParamOp->SetChild(ParamOp->op.args.BoolEqualScalarConst.value, ScalarResult.op);
			ParamOp->op.args.BoolEqualScalarConst.constant = (int16)t;

			// Combine the new condition with previous conditions coming from parent objects
			if (CurrentCondition.Last().ObjectCondition)
			{
				Ptr<ASTOpFixed> op = new ASTOpFixed();
				op->op.type = OP_TYPE::BO_AND;
				op->SetChild(op->op.args.BoolBinary.a, CurrentCondition.Last().ObjectCondition);
				op->SetChild(op->op.args.BoolBinary.b, ParamOp);
				ParamOp = op;
			}

			FConditionContext data;
			data.ObjectCondition = ParamOp;
			CurrentCondition.Push(data);

			if (InNode->Options[t])
			{
				Generate_Generic(InNode->Options[t].get());
			}

			CurrentCondition.Pop();
		}
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_LOD(const NodeLOD* InNode)
	{
		for (const Ptr<NodeSurface>& c : InNode->Surfaces)
		{
			if (c)
			{
				Generate_Generic(c.get());
			}
		}

		for (const Ptr<NodeModifier>& c : InNode->Modifiers)
		{
			if (c)
			{
				Generate_Modifier(c.get());
			}
		}
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_ObjectNew(const NodeObjectNew* InNode)
	{
		// Add the data about this object
		FObject thisData;
		thisData.Node = InNode;
        thisData.Condition = CurrentCondition.Last().ObjectCondition;
		Objects.Add(thisData);

        // Accumulate the model states
        for ( const FObjectState& s: InNode->States )
        {
            States.Emplace( s, InNode );

            if ( s.RuntimeParams.Num() > MUTABLE_MAX_RUNTIME_PARAMETERS_PER_STATE )
            {
                FString Msg = FString::Printf( TEXT("State [%s] has more than %d runtime parameters. Their update may fail."), 
					*s.Name,
                    MUTABLE_MAX_RUNTIME_PARAMETERS_PER_STATE);
				ErrorLog->GetPrivate()->Add(Msg, ELMT_ERROR, InNode->GetMessageContext());
            }
        }

		// Process the components
		for (const Ptr<NodeComponent>& Component : InNode->Components)
		{
			if (Component)
			{
				Generate_Generic(Component.get());
			}
		}

		// Process the children
		for (const Ptr<NodeObject>& Child : InNode->Children)
		{
			if (Child)
			{
				Generate_Generic(Child.get());
			}
		}
	}


	//---------------------------------------------------------------------------------------------
	void FirstPassGenerator::Generate_ObjectGroup(const NodeObjectGroup* InNode)
	{
		const NodeObjectGroup::Private* Private = InNode->GetPrivate();

		// Prepare the enumeration parameter if necessary
        Ptr<ASTOpParameter> enumOp;
        if (Private->m_type==NodeObjectGroup::CS_ALWAYS_ONE ||
			Private->m_type==NodeObjectGroup::CS_ONE_OR_NONE )
        {
            Ptr<ASTOpParameter> op = new ASTOpParameter();
            op->type = OP_TYPE::NU_PARAMETER;

            op->parameter.m_name = Private->Name;
			const TCHAR* CStr = ToCStr(Private->Uid);
			op->parameter.m_uid.ImportTextItem(CStr, 0, nullptr, nullptr);
            op->parameter.m_type = PARAMETER_TYPE::T_INT;
            op->parameter.m_defaultValue.Set<ParamIntType>(Private->DefaultValue);

            if (Private->m_type==NodeObjectGroup::CS_ONE_OR_NONE )
            {
                FParameterDesc::FIntValueDesc nullValue;
                nullValue.m_value = -1;
                nullValue.m_name = "None";
                op->parameter.m_possibleValues.Add( nullValue );
            }

			ParameterNodes.Add(InNode, op);

            enumOp = op;
        }


        // Parse the child objects
		for ( int32 t=0; t< Private->m_children.Num(); ++t )
        {
            if ( mu::Ptr<const NodeObject> pChildNode = Private->m_children[t] )
            {
                // Overwrite the implicit condition
                Ptr<ASTOp> paramOp = 0;
                switch (Private->m_type )
                {
                    case NodeObjectGroup::CS_TOGGLE_EACH:
                    {           
						if (pChildNode->GetType() == NodeObjectGroup::GetStaticType())
						{
							FString Msg = FString::Printf(TEXT("The Group Node [%s] has type Toggle and its direct child is a Group node, which is not allowed. Change the type or add a Child Object node in between them."),
								*Private->Name);
							ErrorLog->GetPrivate()->Add(Msg, ELMT_ERROR, InNode->GetMessageContext());
						}
						else
						{
							// Create a new boolean parameter
							Ptr<ASTOpParameter> op = new ASTOpParameter();
							op->type = OP_TYPE::BO_PARAMETER;

							op->parameter.m_name = pChildNode->GetName();
							const TCHAR* CStr = ToCStr(pChildNode->GetUid());
							op->parameter.m_uid.ImportTextItem(CStr, 0, nullptr, nullptr);
							op->parameter.m_type = PARAMETER_TYPE::T_BOOL;
							op->parameter.m_defaultValue.Set<ParamBoolType>(false);

							ParameterNodes.Add(pChildNode, op);

							paramOp = op;
						}

                        break;
                    }

                    case NodeObjectGroup::CS_ALWAYS_ALL:
                    {
                        // Create a constant true boolean that the optimiser will remove later.
                        Ptr<ASTOpConstantBool> op = new ASTOpConstantBool();
                        op->value = true;

                        paramOp = op;
                        break;
                    }

                    case NodeObjectGroup::CS_ONE_OR_NONE:
                    case NodeObjectGroup::CS_ALWAYS_ONE:
                    {
                        // Add the option to the enumeration parameter
                        FParameterDesc::FIntValueDesc value;
                        value.m_value = (int16)t;
                        value.m_name = pChildNode->GetName();
                        enumOp->parameter.m_possibleValues.Add( value );

                        check(enumOp);

                        // Create a comparison operation as the boolean parameter for the child
                        Ptr<ASTOpFixed> op = new ASTOpFixed();
                        op->op.type = OP_TYPE::BO_EQUAL_INT_CONST;
                        op->SetChild( op->op.args.BoolEqualScalarConst.value, enumOp);
                        op->op.args.BoolEqualScalarConst.constant = (int16_t)t;

                        paramOp = op;
                        break;
                    }

                    default:
                        check( false );
                }

                // Combine the new condition with previous conditions coming from parent objects
                if (CurrentCondition.Last().ObjectCondition)
                {
                    Ptr<ASTOpFixed> op = new ASTOpFixed();
                    op->op.type = OP_TYPE::BO_AND;
                    op->SetChild( op->op.args.BoolBinary.a,CurrentCondition.Last().ObjectCondition);
                    op->SetChild( op->op.args.BoolBinary.b,paramOp);
                    paramOp = op;
                }

				FConditionContext data;
                data.ObjectCondition = paramOp;
                CurrentCondition.Add( data );

				Generate_Generic(pChildNode.get());

                CurrentCondition.Pop();
            }
        }
 	}

}

