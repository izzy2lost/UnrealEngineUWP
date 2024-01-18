// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/CodeOptimiser.h"
#include "MuT/ErrorLogPrivate.h"
#include "MuT/AST.h"
#include "MuT/StreamsPrivate.h"

#include "MuR/ModelPrivate.h"
#include "MuR/SystemPrivate.h"
#include "MuR/Operations.h"
#include "MuR/OpMeshMerge.h"

#include "MuT/ASTOpInstanceAdd.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/ASTOpConstantResource.h"
#include "MuT/ASTOpConstantBool.h"
#include "MuT/ASTOpMeshApplyShape.h"
#include "MuT/ASTOpMeshBindShape.h"
#include "MuT/ASTOpMeshClipMorphPlane.h"
#include "MuT/ASTOpMeshApplyPose.h"
#include "MuT/ASTOpImageRasterMesh.h"

#include <unordered_set>

#include "MuR/MutableRuntimeModule.h"

namespace mu
{

	namespace
	{

		struct FMeshEntry
		{
			Ptr<const Mesh> Mesh;
			Ptr<ASTOpConstantResource> Op;

			bool operator==(const FMeshEntry& o) const
			{
				return Mesh == o.Mesh || *Mesh ==*o.Mesh;
			}
		};

		struct FImageEntry
		{
			Ptr< const Image> Image;
			Ptr<ASTOpConstantResource> Op;

			bool operator==(const FImageEntry& o) const
			{
				return Image == o.Image || *Image == *o.Image;
			}
		};

		struct FLayoutEntry
		{
			Ptr< const Layout> Layout;
			Ptr<ASTOpConstantResource> Op;

			bool operator==(const FLayoutEntry& o) const
			{
				return Layout == o.Layout || *Layout == *o.Layout;
			}
		};

		struct custom_mesh_equal
		{
			bool operator()( const MeshPtrConst& a, const MeshPtrConst& b ) const
			{
				return a==b || *a==*b;
			}
		};

		struct custom_image_equal
		{
			bool operator()( const ImagePtrConst& a, const ImagePtrConst& b ) const
			{
				return a==b || *a==*b;
			}
		};

		struct custom_layout_equal
		{
			bool operator()( const LayoutPtrConst& a, const LayoutPtrConst& b ) const
			{
				return a==b || *a==*b;
			}
		};
	}


	//-------------------------------------------------------------------------------------------------
	void DuplicatedDataRemoverAST( ASTOpList& roots )
	{
		MUTABLE_CPUPROFILER_SCOPE(DuplicatedDataRemoverAST);

		TArray<Ptr<ASTOpConstantResource>> AllMeshOps;
		TArray<Ptr<ASTOpConstantResource>> AllImageOps;
		TArray<Ptr<ASTOpConstantResource>> AllLayoutOps;

		// Gather constants
		{
			MUTABLE_CPUPROFILER_SCOPE(Gather);

			ASTOp::Traverse_TopRandom_Unique_NonReentrant( roots, [&](Ptr<ASTOp> n)
			{
				switch ( n->GetOpType() )
				{

				case OP_TYPE::ME_CONSTANT:
				{
					ASTOpConstantResource* typedNode = static_cast<ASTOpConstantResource*>(n.get());
					AllMeshOps.Add(typedNode);
					break;
				}

				case OP_TYPE::IM_CONSTANT:
				{
					ASTOpConstantResource* typedNode = static_cast<ASTOpConstantResource*>(n.get());
					AllImageOps.Add(typedNode);
					break;
				}

				case OP_TYPE::LA_CONSTANT:
				{
					ASTOpConstantResource* typedNode = static_cast<ASTOpConstantResource*>(n.get());
					AllLayoutOps.Add(typedNode);
					break;
				}

				//    These should be part of the duplicated code removal, in AST.
				//            // Names
				//            case OP_TYPE::IN_ADDMESH:
				//            case OP_TYPE::IN_ADDIMAGE:
				//            case OP_TYPE::IN_ADDVECTOR:
				//            case OP_TYPE::IN_ADDSCALAR:
				//            case OP_TYPE::IN_ADDCOMPONENT:
				//            case OP_TYPE::IN_ADDSURFACE:

				default:
					break;
				}

				return true;
			});
		}


		// Compare meshes
		{
			MUTABLE_CPUPROFILER_SCOPE(CompareMeshes);

			TMultiMap< SIZE_T, FMeshEntry > Meshes;

			for (Ptr<ASTOpConstantResource>& typedNode : AllMeshOps)
			{
				SIZE_T Key = typedNode->GetValueHash();

				Ptr<ASTOp> Found;

				TArray<FMeshEntry*, TInlineAllocator<4>> Candidates;
				Meshes.MultiFindPointer(Key, Candidates, false);

				if (!Candidates.IsEmpty())
				{
					Ptr<const Mesh> mesh = static_cast<const Mesh*>(typedNode->GetValue().get());

					for (FMeshEntry* It : Candidates)
					{
						if (!It->Mesh)
						{
							It->Mesh = static_cast<const Mesh*>(It->Op->GetValue().get());
						}

						if (custom_mesh_equal()(mesh, It->Mesh))
						{
							Found = It->Op;
							break;
						}
					}
				}

				if (Found)
				{
					ASTOp::Replace(typedNode, Found);
				}
				else
				{
					// The mesh will be loaded only if it needs to be compared
					FMeshEntry e;
					e.Op = typedNode;
					Meshes.Add(Key, e);
				}
			}
		}

		// Compare images
		{
			MUTABLE_CPUPROFILER_SCOPE(CompareImages);

			TMultiMap< SIZE_T, FImageEntry > Images;

			for (Ptr<ASTOpConstantResource>& typedNode : AllImageOps)
			{
				SIZE_T Key = typedNode->GetValueHash();

				Ptr<ASTOp> Found;

				TArray<FImageEntry*,TInlineAllocator<4>> Candidates;
				Images.MultiFindPointer(Key, Candidates, false);
				
				if (!Candidates.IsEmpty())
				{
					Ptr<const Image> image = static_cast<const Image*>(typedNode->GetValue().get());

					for (FImageEntry* It: Candidates)
					{
						if (!It->Image)
						{
							It->Image = static_cast<const Image*>(It->Op->GetValue().get());
						}

						if (custom_image_equal()(image, It->Image))
						{
							Found = It->Op;
							break;
						}
					}
				}

				if (Found)
				{
					ASTOp::Replace(typedNode, Found);
				}
				else
				{
					// The image will be loaded only if it needs to be compared
					FImageEntry e;
					e.Op = typedNode;
					Images.Add(Key, e);
				}
			}
		}

		// Compare layouts
		{
			MUTABLE_CPUPROFILER_SCOPE(CompareLayouts);

			TMultiMap< SIZE_T, FLayoutEntry > Layouts;

			for (Ptr<ASTOpConstantResource>& typedNode : AllLayoutOps)
			{
				SIZE_T Key = typedNode->GetValueHash();

				Ptr<ASTOp> Found;

				TArray<FLayoutEntry*, TInlineAllocator<4>> Candidates;
				Layouts.MultiFindPointer(Key, Candidates, false);

				if (!Candidates.IsEmpty())
				{
					Ptr<const Layout> layout = static_cast<const Layout*>(typedNode->GetValue().get());

					for (FLayoutEntry* It : Candidates)
					{
						if (!It->Layout)
						{
							It->Layout = static_cast<const Layout*>(It->Op->GetValue().get());
						}

						if (custom_layout_equal()(layout, It->Layout))
						{
							Found = It->Op;
							break;
						}
					}
				}

				if (Found)
				{
					ASTOp::Replace(typedNode, Found);
				}
				else
				{
					FLayoutEntry e;
					e.Op = typedNode;
					Layouts.Add(Key, e);
				}
			}
		}

	}


	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	namespace
	{
		struct op_pointer_hash
		{
			inline size_t operator() (const mu::Ptr<ASTOp>& n) const
			{
				return n->Hash();
			}
		};

		struct op_pointer_equal
		{
			inline bool operator() (const mu::Ptr<ASTOp>& a, const mu::Ptr<ASTOp>& b) const
			{
				return *a==*b;
			}
		};
	}

	void DuplicatedCodeRemoverAST( ASTOpList& roots )
	{
		MUTABLE_CPUPROFILER_SCOPE(DuplicatedCodeRemoverAST);

		// Visited nodes, per type
		std::unordered_set<Ptr<ASTOp>,op_pointer_hash,op_pointer_equal> visited[int(OP_TYPE::COUNT)];

		ASTOp::Traverse_BottomUp_Unique_NonReentrant( roots, [&](Ptr<ASTOp>& n)
		{
			auto& container = visited[(int)n->GetOpType()];

			// debug
	//        for( const auto& o : container )
	//        {
	//            if (*n==*o)
	//            {
	//                auto thisHash = n->Hash();
	//                auto otherHash = o->Hash();
	//                check(thisHash==otherHash);
	//            }
	//        }

			// Insert will tell us if it was already there
			auto it = container.insert(n);
			if( !it.second )
			{
				// It wasn't inserted, so it was already there
				ASTOp::Replace(n,*it.first);
			}
		});
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	bool FindOpTypeVisitor::Apply( FProgram& program, OP::ADDRESS rootAt )
	{
		bool found = false;

		// If the program added new operations, we haven't visited them.
		m_visited.SetNumZeroed( program.m_opAddress.Num() );

		m_pending.Reset();
		m_pending.Reserve( program.m_opAddress.Num()/4 );
		m_pending.Add({ false,rootAt });

		// Don't early out to be able to complete parent op cached flags
		while ( !m_pending.IsEmpty() )
		{
			TPair<bool,int> item = m_pending.Pop();
			OP::ADDRESS at = item.Value;

			// Not cached?
			if (m_visited[at]<=1)
			{
				if (item.Key)
				{
					// Item indicating we finished with all the children of a parent
					check(m_visited[at]==1);

					bool subtreeFound = false;
					ForEachReference( program, at, [&](OP::ADDRESS ref)
					{
						subtreeFound = subtreeFound || m_visited[ref]==3;
					});

					m_visited[at] = subtreeFound ? 3 : 2;
				}
				else if (!found)
				{
					if (m_typesToFind.Find(program.GetOpType(at)) != INDEX_NONE )
					{
						m_visited[at] = 3; // visited, ops found
						found = true;
					}
					else
					{
						check(m_visited[at]==0);

						m_visited[at] = 1;
						m_pending.Add({ true,at });

						ForEachReference( program, at, [&](OP::ADDRESS ref)
						{
							if (ref && m_visited[ref]==0)
							{
								m_pending.Add({ false,ref });
							}
						});
					}
				}
				else
				{
					// We won't process it.
					m_visited[at] = 0;
				}
			}
		}

		return m_visited[rootAt]==3;
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	IsConstantVisitor::IsConstantVisitor()
	{
		TArray<OP_TYPE> parameterTypes;
		parameterTypes.Add( OP_TYPE::BO_PARAMETER );
		parameterTypes.Add( OP_TYPE::NU_PARAMETER );
		parameterTypes.Add( OP_TYPE::SC_PARAMETER );
		parameterTypes.Add( OP_TYPE::CO_PARAMETER );
		parameterTypes.Add( OP_TYPE::PR_PARAMETER );
		parameterTypes.Add( OP_TYPE::IM_PARAMETER );
		m_findOpTypeVisitor = MakeUnique<FindOpTypeVisitor>(parameterTypes);
	}


	//---------------------------------------------------------------------------------------------
	bool IsConstantVisitor::Apply( FProgram& program, OP::ADDRESS at )
	{
		return !m_findOpTypeVisitor->Apply(program,at);
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	class ConstantTask 
	{
	private:

		//
		mu::Ptr<ASTOp> m_result;

	public:
		// input
		mu::Ptr<ASTOp> m_source;
		bool bUseDiskCache = false;
		int ImageCompressionQuality = 0;


		void Run(FImageOperator ImOp)
		{
			MUTABLE_CPUPROFILER_SCOPE(ConstantTask_Run);

			// We need the clone because linking modifies ASTOp state
			mu::Ptr<ASTOp> cloned = ASTOp::DeepClone( m_source );

			OP_TYPE type = cloned->GetOpType();
			DATATYPE dtype = GetOpDataType(type);

			Ptr<Settings> pSettings = new Settings;
			pSettings->SetProfile( false );
			pSettings->SetImageCompressionQuality( ImageCompressionQuality );
			SystemPtr pSystem = new System( pSettings );

			pSystem->GetPrivate()->ImagePixelFormatOverride = ImOp.FormatImageOverride;

			// Don't generate mips suring linking here.
			FLinkerOptions LinkerOptions(ImOp);
			LinkerOptions.MinTextureResidentMipCount = 255;

			TSharedPtr<const Model> model = MakeShared<Model>();
			ASTOp::FullLink( cloned, model->GetPrivate()->m_program, &LinkerOptions);
			OP::ADDRESS at = cloned->linkedAddress;

			FProgram::FState state;
			state.m_root = at;
			model->GetPrivate()->m_program.m_states.Add(state);

			ParametersPtr localParams = Model::NewParameters(model);
			pSystem->GetPrivate()->BeginBuild( model );

			// Calculate the value and replace this op by a constant
			switch( dtype )
			{
			case DT_MESH:
			{
				MUTABLE_CPUPROFILER_SCOPE(ConstantMesh);

				mu::Ptr<const Mesh> pMesh = pSystem->GetPrivate()->BuildMesh( model, localParams.get(), at );

				if (pMesh)
				{
					mu::Ptr<ASTOpConstantResource> constantOp = new ASTOpConstantResource();
					constantOp->type = OP_TYPE::ME_CONSTANT;
					constantOp->SetValue( pMesh, bUseDiskCache );
					m_result = constantOp;
				  }
				break;
			}

			case DT_IMAGE:
			{
				MUTABLE_CPUPROFILER_SCOPE(ConstantImage);

				mu::Ptr<const Image> pImage = pSystem->GetPrivate()->BuildImage( model, localParams.get(), at, 0, 0 );

				if (pImage)
				{
					mu::Ptr<ASTOpConstantResource> constantOp = new ASTOpConstantResource();
					constantOp->type = OP_TYPE::IM_CONSTANT;
					constantOp->SetValue( pImage, bUseDiskCache );
					m_result = constantOp;
				}
				break;
			}

			case DT_LAYOUT:
			{
				MUTABLE_CPUPROFILER_SCOPE(ConstantLayout);

				mu::Ptr<const Layout> pLayout = pSystem->GetPrivate()->BuildLayout( model, localParams.get(), at );

				if (pLayout)
				{
					mu::Ptr<ASTOpConstantResource> constantOp = new ASTOpConstantResource();
					constantOp->type = OP_TYPE::LA_CONSTANT;
					constantOp->SetValue( pLayout, bUseDiskCache );
					m_result = constantOp;
				}
				break;
			}

			case DT_BOOL:
			{
				MUTABLE_CPUPROFILER_SCOPE(ConstantBool);

				bool value = pSystem->GetPrivate()->BuildBool( model, localParams.get(), at );

				{
					mu::Ptr<ASTOpConstantBool> constantOp = new ASTOpConstantBool();
					constantOp->value = value;
					m_result = constantOp;
				}
				break;
			}

			case DT_COLOUR:
			{
				MUTABLE_CPUPROFILER_SCOPE(ConstantBool);

				FVector4f Result(0, 0, 0, 0);
				Result = pSystem->GetPrivate()->BuildColour( model, localParams.get(), at );

				{
					mu::Ptr<ASTOpFixed> constantOp = new ASTOpFixed();
					constantOp->op.type = OP_TYPE::CO_CONSTANT;
					constantOp->op.args.ColourConstant.value[0] = Result[0];
					constantOp->op.args.ColourConstant.value[1] = Result[1];
					constantOp->op.args.ColourConstant.value[2] = Result[2];
					constantOp->op.args.ColourConstant.value[3] = Result[3];
					m_result = constantOp;
				}
				break;
			}

			case DT_INT:
			case DT_SCALAR:
			case DT_STRING:
			case DT_PROJECTOR:
				// TODO
				break;

			default:
				break;
			}

			pSystem->GetPrivate()->EndBuild();

		}

		void Complete()
		{
			// This runs in the managing thread
			ASTOp::Replace( m_source, m_result );
		}
	};


	//---------------------------------------------------------------------------------------------
	bool ConstantGeneratorAST( const CompilerOptions::Private* options, Ptr<ASTOp>& root )
	{
		MUTABLE_CPUPROFILER_SCOPE(ConstantGenerator);

		bool modified = false;

		// don't do this if constant optimization has been disabled, usually for debugging.
		if (!options->OptimisationOptions.bConstReduction)
		{
			return false;
		}


		// Calculate constant-subtree and special-op flags
		{
			MUTABLE_CPUPROFILER_SCOPE(ConstantGenerator_CalculateFlags);

			ASTOp::Traverse_BottomUp_Unique( root, [&](Ptr<ASTOp>& n)
			{
				bool constantSubtree = true;
				switch (n->GetOpType())
				{
				case OP_TYPE::BO_PARAMETER:
				case OP_TYPE::NU_PARAMETER:
				case OP_TYPE::SC_PARAMETER:
				case OP_TYPE::CO_PARAMETER:
				case OP_TYPE::PR_PARAMETER:
				case OP_TYPE::IM_PARAMETER:
					constantSubtree = false;
					break;
				default:
					// Propagate from children
					n->ForEachChild( [&constantSubtree](ASTChild& c)
					{
						if (c)
						{
							constantSubtree = constantSubtree && c->m_constantSubtree;
						}
					});
					break;
				}
				n->m_constantSubtree = constantSubtree;

				// We avoid generating constants for these operations, to avoid the memory
				// explosion.
				// TODO: Make compiler options for some of them
				// TODO: Some of them are worth if the code below them is unique.
				bool hasSpecialOpInSubtree = false;
				switch (n->GetOpType())
				{
				case OP_TYPE::IM_BLANKLAYOUT:
				case OP_TYPE::IM_REFERENCE:
				case OP_TYPE::IM_COMPOSE:
				//case OP_TYPE::IM_RASTERMESH:            // TODO review this one
				case OP_TYPE::ME_MERGE:
				case OP_TYPE::ME_CLIPWITHMESH:
				case OP_TYPE::ME_CLIPMORPHPLANE:
				case OP_TYPE::ME_APPLYPOSE:
				case OP_TYPE::ME_REMOVEMASK:
				case OP_TYPE::ME_ADDTAGS:
				case OP_TYPE::IM_PLAINCOLOUR:
					hasSpecialOpInSubtree = true;
					break;

				default:
					// Propagate from children
					n->ForEachChild( [&](ASTChild& c)
					{
						if (c)
						{
							hasSpecialOpInSubtree = hasSpecialOpInSubtree || c->m_hasSpecialOpInSubtree;
						}
					});
					break;
				}
				n->m_hasSpecialOpInSubtree = hasSpecialOpInSubtree;
			});

		}

		TArray< Ptr<ASTOp> > roots;
		roots.Add(root);

		// Generate constant operations
		ASTOp::Traverse_TopDown_Unique_Imprecise( roots, [&](Ptr<ASTOp>& n)
		{
			bool recurse = true;

			OP_TYPE type = n->GetOpType();
			DATATYPE dtype = GetOpDataType(type);

			bool constant = false;
			if (dtype!=DT_INSTANCE)
			{
				constant = n->m_constantSubtree;
			}

			// See if it is a case of instructions we want to avoid, but with special parameters that make
			// them ok
			bool specialCase = false;
			if (constant)
			{
				//const OP& op = program.m_code[at];
				// TODO
	//                if (n->GetOpType()==OP_TYPE::IM_COMPOSE)
	//                {
	//                    // Get the layout
	//                    // In case we fill all of the target image anyway
	//                    ParametersPtr pParams = m_pModel->NewParameters();

	//                    const auto& args = op.args.ImageCompose;
	//                    LayoutPtrConst pLayout = m_pSystem->GetPrivate()->BuildLayout
	//                        ( m_pModel.get(), pParams.get(), args.layout );

	//                    if ( pLayout &&
	//                         pLayout->GetPrivate()->IsSingleBlockAndFull() )
	//                    {
	//                        specialCase = true;
	//                    }
	//                }

	//                else
				if (n->GetOpType()==OP_TYPE::IM_RASTERMESH)
				{
					const ASTOpImageRasterMesh* Raster = static_cast<const ASTOpImageRasterMesh*>(n.get());
					if ( !Raster->image )
					{
						specialCase = true;
					}
				}
			}

			// See if the subtree contains operations that we don't want to collapse even if
			// they are constant
			if ( constant && !specialCase)
			{
				if ( n->m_hasSpecialOpInSubtree )
				{
					constant = false;
				}
			}

			// If children are constant
			if ( constant
				 && type!=OP_TYPE::BO_CONSTANT
				 && type!=OP_TYPE::NU_CONSTANT
				 && type!=OP_TYPE::SC_CONSTANT
				 && type!=OP_TYPE::CO_CONSTANT
				 && type!=OP_TYPE::IM_CONSTANT
				 && type!=OP_TYPE::ME_CONSTANT
				 && type!=OP_TYPE::LA_CONSTANT
				 && type!=OP_TYPE::PR_CONSTANT
				 && type!=OP_TYPE::NONE
				)
			{
				switch( dtype )
				{
				case DT_MESH:
				case DT_IMAGE:
				case DT_LAYOUT:
				case DT_BOOL:
				case DT_COLOUR:
				{
					recurse = false;
					modified = true;

					ConstantTask* constantTask = new ConstantTask;
					constantTask->bUseDiskCache = options->OptimisationOptions.bUseDiskCache;
					constantTask->m_source = n;
					constantTask->ImageCompressionQuality = options->ImageCompressionQuality;

					FImageOperator ImOp = FImageOperator::GetDefault(options->ImageFormatFunc);
					constantTask->Run(ImOp);
					constantTask->Complete();
					delete constantTask;

					break;
				}

				case DT_INT:
				case DT_SCALAR:
				case DT_STRING:
				case DT_PROJECTOR:
					// TODO
					break;

				case DT_INSTANCE:
					// nothing to do
					break;

				default:
					break;
				}

			}

			return recurse;
		});

		return modified;
	}


	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	CodeOptimiser::CodeOptimiser(Ptr<CompilerOptions> options, TArray<FStateCompilationData>& states )
		: m_states( states )
	{
		m_options = options;
	}


	//-------------------------------------------------------------------------------------------------
	void CodeOptimiser::FullOptimiseAST( ASTOpList& roots, int32 Pass )
	{
		bool modified = true;
		int numIterations = 0;
		while (modified && (!m_optimizeIterationsMax || (m_optimizeIterationsLeft>0) || !numIterations) )
		{
			--m_optimizeIterationsLeft;
			++numIterations;
			UE_LOG(LogMutableCore, Verbose, TEXT("Main optimise iteration %d, max %d, left %d"),
					numIterations, m_optimizeIterationsMax, m_optimizeIterationsLeft);

			modified = false;

			// All kind of optimisations that depend on the meaning of each operation
			// \TODO: We are doing it for all states.
			UE_LOG(LogMutableCore, Verbose, TEXT(" - semantic optimiser"));
			modified |= SemanticOptimiserAST( roots, m_options->GetPrivate()->OptimisationOptions, Pass );
			//AXE_INT_VALUE("Mutable", Verbose, "ast size", (int64_t)ASTOp::CountNodes(roots));
			UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));
			ASTOp::LogHistogram(roots);

			UE_LOG(LogMutableCore, Verbose, TEXT(" - sink optimiser"));
			modified |= SinkOptimiserAST( roots, m_options->GetPrivate()->OptimisationOptions );
			//AXE_INT_VALUE("Mutable", Verbose, "ast size", (int64_t)ASTOp::CountNodes(roots));
			ASTOp::LogHistogram(roots);

			// Image size operations are treated separately
			UE_LOG(LogMutableCore, Verbose, TEXT(" - size optimiser"));
			modified |= SizeOptimiserAST( roots );
			//AXE_INT_VALUE("Mutable", Verbose, "ast size", (int64_t)ASTOp::CountNodes(roots));

			// We cannot run the logic optimiser here because the constant memory explodes.
			// TODO: Conservative logic optimiser
			if (!modified)
			{
	//                AXE_INT_VALUE("Mutable", Verbose, "program size", (int64_t)program.m_opAddress.Num());
	//                UE_LOG(LogMutableCore, Verbose, " - logic optimiser");
	//                LocalLogicOptimiser log;
	//                modified |= log.Apply( program, (int)s );
			}
		}

	//    ASTOp::LogHistogram(roots);

		UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated code remover"));
		DuplicatedCodeRemoverAST( roots );
		UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));

		ASTOp::LogHistogram(roots);

		UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated data remover"));
		DuplicatedDataRemoverAST( roots );
		UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));

		ASTOp::LogHistogram(roots);

		// Generate constants
		for ( auto& r: roots )
		{
			UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));
			UE_LOG(LogMutableCore, Verbose, TEXT(" - constant generator"));

			// Constant subtree generation
			modified = ConstantGeneratorAST( m_options->GetPrivate(), r );
		}

		UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));

	//    ASTOp::LogHistogram(roots);

		// \todo: this code deduplication here, takes very long and doesn't do much... investigate!
	//    UE_LOG(LogMutableCore, Verbose, " - duplicated code remover");
	//    DuplicatedCodeRemoverAST( roots );
	//    AXE_INT_VALUE("Mutable", Verbose, "ast size", (int64_t)ASTOp::CountNodes(roots));

		ASTOp::LogHistogram(roots);

		UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated data remover"));
		DuplicatedDataRemoverAST( roots );
		UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));

		//if (!modified)
		{
			//for ( size_t s=0;  s<program.m_states.size(); ++s )
			{
				ASTOp::LogHistogram(roots);
				UE_LOG(LogMutableCore, Verbose, TEXT(" - logic optimiser"));
				modified |= LocalLogicOptimiserAST( roots );
				ASTOp::LogHistogram(roots);
			}

			//AXE_INT_VALUE("Mutable", Verbose, "ast size", (int64_t)ASTOp::CountNodes(roots));
		}

		ASTOp::LogHistogram(roots);
	}


	//---------------------------------------------------------------------------------------------
	// The state represents if there is a parent operation requiring skeleton for current mesh subtree.
	class CollectAllMeshesForSkeletonVisitorAST : public Visitor_TopDown_Unique_Const<uint8_t>
	{
	public:

		CollectAllMeshesForSkeletonVisitorAST( const ASTOpList& roots  )
		{
			Traverse( roots, false );
		}

		// List of meshes that require a skeleton
		vector<mu::Ptr<ASTOpConstantResource>> m_meshesRequiringSkeleton;

	private:

		// Visitor_TopDown_Unique_Const<uint8_t> interface
		bool Visit( const mu::Ptr<ASTOp>& node ) override
		{
			// \todo: refine to avoid instruction branches with irrelevant skeletons.

			uint8_t currentProtected = GetCurrentState();

			switch (node->GetOpType())
			{

			case OP_TYPE::ME_CONSTANT:
			{
				mu::Ptr<ASTOpConstantResource> typedOp = static_cast<ASTOpConstantResource*>(node.get());

				if (currentProtected)
				{
					if ( std::find(m_meshesRequiringSkeleton.begin(), m_meshesRequiringSkeleton.end(), typedOp)
						 ==
						 m_meshesRequiringSkeleton.end() )
					{
						m_meshesRequiringSkeleton.push_back(typedOp);
					}
				}

				return false;
			}

			case OP_TYPE::ME_CLIPMORPHPLANE:
			{
				ASTOpMeshClipMorphPlane* typedOp = static_cast<ASTOpMeshClipMorphPlane*>(node.get());
				if (typedOp->vertexSelectionType == OP::MeshClipMorphPlaneArgs::VS_BONE_HIERARCHY)
				{
					// We need the skeleton for the source mesh
					RecurseWithState( typedOp->source.child(), true );
					return false;
				}

				return true;
			}

			case OP_TYPE::ME_APPLYPOSE:
			{
				ASTOpMeshApplyPose* typedOp = static_cast<ASTOpMeshApplyPose*>(node.get());

				// We need the skeleton for both meshes
				RecurseWithState(typedOp->base.child(), true);
				RecurseWithState(typedOp->pose.child(), true);
				return false;

				break;
			}

			case OP_TYPE::ME_BINDSHAPE:
			{
				ASTOpMeshBindShape* typedOp = static_cast<ASTOpMeshBindShape*>(node.get());
				if (typedOp->bReshapeSkeleton)
				{
					RecurseWithState(typedOp->Mesh.child(), true);
					return false;
				}

				break;
			}

			case OP_TYPE::ME_APPLYSHAPE:
			{
				ASTOpMeshApplyShape* typedOp = static_cast<ASTOpMeshApplyShape*>(node.get());
				if (typedOp->bReshapeSkeleton)
				{
					RecurseWithState(typedOp->Mesh.child(), true);
					return false;
				}

				break;
			}

			default:
				break;
			}

			return true;
		}

	};


	//---------------------------------------------------------------------------------------------
	// This stores an ADD_MESH op with the child meshes collected and the final skeleton to use
	// for this op.
	struct FAddMeshSkeleton
	{
		mu::Ptr<ASTOp> m_pAddMeshOp;
		TArray<mu::Ptr<ASTOpConstantResource>> m_contributingMeshes;
		mu::Ptr<Skeleton> m_pFinalSkeleton;

		FAddMeshSkeleton( const mu::Ptr<ASTOp>& pAddMeshOp,
						  TArray<mu::Ptr<ASTOpConstantResource>>& contributingMeshes,
						  const mu::Ptr<Skeleton>& pFinalSkeleton )
		{
			m_pAddMeshOp = pAddMeshOp;
			m_contributingMeshes = std::move(contributingMeshes);
			m_pFinalSkeleton = pFinalSkeleton;
		}
	};


	//---------------------------------------------------------------------------------------------
	void SkeletonCleanerAST( TArray<mu::Ptr<ASTOp>>& roots, const FModelOptimizationOptions& options )
	{
		// This collects all the meshes that require a skeleton because they are used in operations
		// that require it.
		CollectAllMeshesForSkeletonVisitorAST requireSkeletonCollector( roots );

		TArray<FAddMeshSkeleton> replacementsFound;

		ASTOp::Traverse_TopDown_Unique_Imprecise( roots, [&](mu::Ptr<ASTOp>& at )
		{
			// Only recurse instance construction ops.
			bool processChildren = GetOpDataType(at->GetOpType())==DT_INSTANCE;

			if ( at->GetOpType() == OP_TYPE::IN_ADDMESH )
			{
				ASTOpInstanceAdd* typedNode = static_cast<ASTOpInstanceAdd*>(at.get());
				mu::Ptr<ASTOp> meshRoot = typedNode->value.child();

				if (meshRoot)
				{
					// Gather constant meshes contributing to the final mesh
					TArray<mu::Ptr<ASTOpConstantResource>> subtreeMeshes;
					TArray<mu::Ptr<ASTOp>> tempRoots;
					tempRoots.Add(meshRoot);
					ASTOp::Traverse_TopDown_Unique_Imprecise( tempRoots, [&](mu::Ptr<ASTOp>& lat )
					{
						// \todo: refine to avoid instruction branches with irrelevant skeletons.
						if ( lat->GetOpType() == OP_TYPE::ME_CONSTANT )
						{
							mu::Ptr<ASTOpConstantResource> typedOp = static_cast<ASTOpConstantResource*>(lat.get());
							if ( subtreeMeshes.Find(typedOp)
								 ==
								 INDEX_NONE )
							{
								subtreeMeshes.Add(typedOp);
							}
						}
						return true;
					});

					// Create a mesh just with the unified skeleton
					SkeletonPtr pFinalSkeleton = new Skeleton;
					for (const auto& meshAt: subtreeMeshes)
					{
						MeshPtrConst pMesh = static_cast<const Mesh*>(meshAt->GetValue().get());
						mu::Ptr<const Skeleton> sourceSkeleton = pMesh ? pMesh->GetSkeleton() : nullptr;
						if (sourceSkeleton)
						{
							ExtendSkeleton(pFinalSkeleton.get(),sourceSkeleton.get());
						}
					}

					replacementsFound.Emplace( at, subtreeMeshes, pFinalSkeleton );
				}
			}

			return processChildren;
		});


		// Iterate all meshes again
		ASTOp::Traverse_TopDown_Unique_Imprecise( roots, [&](mu::Ptr<ASTOp>& at )
		{
			if (at->GetOpType()==OP_TYPE::ME_CONSTANT)
			{
				ASTOpConstantResource* typedOp = static_cast<ASTOpConstantResource*>(at.get());

				for(FAddMeshSkeleton& Rep: replacementsFound)
				{
					if (Rep.m_contributingMeshes.Contains(at))
					{
						mu::Ptr<const Mesh> pMesh =  static_cast<const Mesh*>(typedOp->GetValue().get());
						pMesh->CheckIntegrity();

						Ptr<Mesh> NewMesh = new Mesh();
						bool bOutSuccess = false;
						MeshRemapSkeleton(NewMesh.get(), pMesh.get(), Rep.m_pFinalSkeleton.get(), bOutSuccess);

						if (bOutSuccess)
						{
							NewMesh->CheckIntegrity();
							mu::Ptr<ASTOpConstantResource> newOp = new ASTOpConstantResource();
							newOp->type = OP_TYPE::ME_CONSTANT;
							newOp->SetValue(NewMesh, options.bUseDiskCache);

							ASTOp::Replace(at, newOp);
						}
					}
				}
			}
			return true;
		});

	//    for( auto& rep: replacementsFound )
	//    {
	//        check(rep.m_pAddMeshOp->GetOpType()==OP_TYPE::IN_ADDMESH);
	//        auto newAddMeshRootTyped = dynamic_cast<ASTOpInstanceAdd*>(rep.m_pAddMeshOp.get());
	//        auto meshRoot = newAddMeshRootTyped->value.child();

	//        // Add new skeleton constant instruction
	//        Ptr<ASTOpConstantResource> cop = new ASTOpConstantResource();
	//        cop->type = OP_TYPE::ME_CONSTANT;
	//        cop->SetValue( rep.m_pFinalSkeleton, options.bUseDiskCache );

	//        // Add new "apply skeleton" instruction
	//        Ptr<ASTOpFixed> skop = new ASTOpFixed();
	//        skop->op.type = OP_TYPE::ME_SETSKELETON;
	//        skop->SetChild( skop->op.args.MeshSetSkeleton.source, meshRoot);
	//        skop->SetChild( skop->op.args.MeshSetSkeleton.skeleton, cop);

	//        // We need a new mesh add instruction, it is not safe to edit the current one.
	//        auto newOp = mu::Clone<ASTOpInstanceAdd>(newAddMeshRootTyped);
	//        newOp->value = skop;

	//        // TODO BLEH UNSAFE?
	//        ASTOp::Replace(rep.m_pAddMeshOp,newOp);
	//    }
	}


	//---------------------------------------------------------------------------------------------
	void CodeOptimiser::OptimiseAST()
	{
		MUTABLE_CPUPROFILER_SCOPE(OptimiseAST);

		// Gather all the roots
		TArray<Ptr<ASTOp>> roots;
		for(const auto& s:m_states)
		{
			roots.Add(s.root);
		}

		UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));

		if ( m_options->GetPrivate()->OptimisationOptions.bEnabled )
		{
			// We use 4 times the count because at the time we moved to sharing this count it
			// was being used 4 times, and we want to keep the tests consistent.
			m_optimizeIterationsLeft = m_options->GetPrivate()->OptimisationOptions.MaxOptimisationLoopCount * 4;
			m_optimizeIterationsMax = m_optimizeIterationsLeft;

			// The first duplicated data remover has the special mission of removing
			// duplicated data (meshes) that may have been specified in the source
			// data, before we make it diverge because of different uses, like layout
			// creation
			UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated data remover"));
			DuplicatedDataRemoverAST( roots );
			UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));

			ASTOp::LogHistogram(roots);

			UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated code remover"));
			DuplicatedCodeRemoverAST( roots );
			UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));

			// Special optimization stages
			if ( m_options->GetPrivate()->OptimisationOptions.bUniformizeSkeleton )
			{
				UE_LOG(LogMutableCore, Verbose, TEXT(" - skeleton cleaner"));
				ASTOp::LogHistogram(roots);

				SkeletonCleanerAST( roots, m_options->GetPrivate()->OptimisationOptions );
				UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));
				ASTOp::LogHistogram(roots);
			}

			// First optimisation stage. It tries to resolve all the image sizes. This is necessary
			// because some operations cannot be applied correctly until the image size is known
			// like the grow-map generation.
			bool modified = true;
			int numIterations = 0;
			while (modified)
			{
				MUTABLE_CPUPROFILER_SCOPE(FirstStage);

				--m_optimizeIterationsLeft;
				++numIterations;
				UE_LOG(LogMutableCore, Verbose, TEXT("First optimise iteration %d, max %d, left %d"),
						numIterations, m_optimizeIterationsMax, m_optimizeIterationsLeft);

				modified = false;

				UE_LOG(LogMutableCore, Verbose, TEXT(" - size optimiser"));
				modified |= SizeOptimiserAST( roots );
			}

			UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));
			ASTOp::LogHistogram(roots);

			// Main optimisation stage
			{
				MUTABLE_CPUPROFILER_SCOPE(MainStage);
				FullOptimiseAST( roots, 0 );
				UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));
				ASTOp::LogHistogram(roots);

				FullOptimiseAST( roots, 1 );
				UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));
				ASTOp::LogHistogram(roots);
			}

			// Constant resolution stage: resolve referenced assets.
			{
				MUTABLE_CPUPROFILER_SCOPE(ReferenceResolution);
				FullOptimiseAST(roots, 2);
			}

			// Main optimisation stage again for data-aware optimizations
			{
				MUTABLE_CPUPROFILER_SCOPE(FinalStage);
				FullOptimiseAST(roots, 0);
				UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));
				ASTOp::LogHistogram(roots);

				FullOptimiseAST(roots, 1);
				UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));
				ASTOp::LogHistogram(roots);
			}

			// Analyse mesh constants to see which of them are in optimised mesh formats, and set the flags.
			ASTOp::Traverse_BottomUp_Unique_NonReentrant( roots, [&](Ptr<ASTOp>& n)
			{
				if (n->GetOpType()==OP_TYPE::ME_CONSTANT)
				{
					ASTOpConstantResource* typed = static_cast<ASTOpConstantResource*>(n.get());
					auto pMesh = static_cast<const Mesh*>(typed->GetValue().get());
					pMesh->ResetStaticFormatFlags();
					typed->SetValue( pMesh,
									 m_options->GetPrivate()->OptimisationOptions.bUseDiskCache );
				}
			});

			// Make sure we didn't lose track of pointers
			for ( int32 s=0;  s<m_states.Num(); ++s )
			{
				check( roots.Contains( m_states[s].root ) );
			}

			ASTOp::LogHistogram(roots);

			{
				MUTABLE_CPUPROFILER_SCOPE(StatesStage);

				// Optimise for every state
				OptimiseStatesAST( );

				// Optimise the data formats (TODO)
				//OperationFlagGenerator flagGen( pResult.get() );
			}

			ASTOp::LogHistogram(roots);
		}

		// Minimal optimisation of constant subtrees
		else if ( m_options->GetPrivate()->OptimisationOptions.bConstReduction )
		{
			// The first duplicated data remover has the special mission of removing
			// duplicated data (meshes) that may have been specified in the source
			// data, before we make it diverge because of different uses, like layout
			// creation
			UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated data remover"));
			DuplicatedDataRemoverAST( roots );
			//AXE_INT_VALUE("Mutable", Verbose, "ast size", (int64_t)ASTOp::CountNodes(roots));

			UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated code remover"));
			DuplicatedCodeRemoverAST( roots );
			UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("ast size"), int64(ASTOp::CountNodes(roots)));

			// Constant resolution stage: resolve referenced assets.
			{
				MUTABLE_CPUPROFILER_SCOPE(ReferenceResolution);
				FullOptimiseAST(roots, 2);
			}

			for ( int32 s=0;  s<m_states.Num(); ++s )
			{
				UE_LOG(LogMutableCore, Verbose, TEXT(" - constant generator"));
				ConstantGeneratorAST( m_options->GetPrivate(), m_states[s].root );
				//AXE_INT_VALUE("Mutable", Verbose, "ast size", (int64_t)ASTOp::CountNodes(roots));
			}

			UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated data remover"));
			DuplicatedDataRemoverAST( roots );
			//AXE_INT_VALUE("Mutable", Verbose, "ast size", (int64_t)ASTOp::CountNodes(roots));

			UE_LOG(LogMutableCore, Verbose, TEXT(" - duplicated code remover"));
			DuplicatedCodeRemoverAST( roots );
			//AXE_INT_VALUE("Mutable", Verbose, "ast size", (int64_t)ASTOp::CountNodes(roots));

			// Make sure we didn't lose track of pointers
			for ( int32 s=0;  s<m_states.Num(); ++s )
			{
				check( roots.Contains( m_states[s].root ) );
			}
		}

		ASTOp::LogHistogram(roots);

	}

}
