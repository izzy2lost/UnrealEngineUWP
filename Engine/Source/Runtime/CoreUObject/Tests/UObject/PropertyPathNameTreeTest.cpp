// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyPathNameTree.h"

#if WITH_TESTS

#include "Tests/TestHarnessAdapter.h"

namespace UE
{

TEST_CASE_NAMED(FPropertyPathNameTreeTest, "CoreUObject::PropertyPathNameTree", "[Core][UObject][SmokeFilter]")
{
	const FName CountName(TEXTVIEW("Count"));
	const FName SizeName(TEXTVIEW("Size"));

	FPropertyTypeNameBuilder TypeBuilder;
	TypeBuilder.AddName(NAME_IntProperty);
	const FPropertyTypeName IntType = TypeBuilder.Build();

	TypeBuilder.Reset();
	TypeBuilder.AddName(NAME_FloatProperty);
	const FPropertyTypeName FloatType = TypeBuilder.Build();

	TypeBuilder.Reset();
	TypeBuilder.AddName(NAME_StructProperty);
	TypeBuilder.BeginParameters();
	TypeBuilder.AddName(NAME_Vector);
	TypeBuilder.EndParameters();
	const FPropertyTypeName VectorType = TypeBuilder.Build();

	SECTION("Empty")
	{
		FPropertyPathNameTree Tree;
		CHECK(Tree.IsEmpty());

		FPropertyPathName PathName;
		PathName.Push({CountName});
		Tree.Add(PathName);
		CHECK_FALSE(Tree.IsEmpty());

		Tree.Empty();
		CHECK(Tree.IsEmpty());
	}

	SECTION("Name")
	{
		FPropertyPathName PathName;
		PathName.Push({CountName});
		FPropertyPathNameTree Tree;
		Tree.Add(PathName);
		FPropertyPathNameTree* SubTree = &Tree;
		CHECK(Tree.Find(&SubTree, PathName));
		CHECK_FALSE(SubTree);
		const FPropertyPathNameTree::FConstIterator First = Tree.CreateConstIterator();
		FPropertyPathNameTree::FConstIterator It = Tree.CreateConstIterator();
		CHECK(First == It);
		CHECK_FALSE(First != It);
		CHECKED_IF(It)
		{
			CHECK(It.GetName() == CountName);
			CHECK(It.GetType().IsEmpty());
			CHECK_FALSE(It.GetSubTree());
			++It;
			CHECK_FALSE(It);
			CHECK_FALSE(First == It);
			CHECK(First != It);
		}
	}

	SECTION("NameType")
	{
		FPropertyPathName PathName;
		PathName.Push({CountName, IntType});
		FPropertyPathNameTree Tree;
		Tree.Add(PathName);
		FPropertyPathNameTree* SubTree = &Tree;
		CHECK(Tree.Find(&SubTree, PathName));
		CHECK_FALSE(SubTree);
		const FPropertyPathNameTree::FConstIterator First = Tree.CreateConstIterator();
		FPropertyPathNameTree::FConstIterator It = Tree.CreateConstIterator();
		CHECK(First == It);
		CHECK_FALSE(First != It);
		CHECKED_IF(It)
		{
			CHECK(It.GetName() == CountName);
			CHECK(It.GetType() == IntType);
			CHECK_FALSE(It.GetSubTree());
			++It;
			CHECK_FALSE(It);
			CHECK_FALSE(First == It);
			CHECK(First != It);
		}
	}

	SECTION("NameTypeIndex")
	{
		FPropertyPathName PathName;
		PathName.Push({CountName, IntType, 7});
		FPropertyPathNameTree Tree;
		Tree.Add(PathName);
		CHECK(Tree.Find(nullptr, PathName));
		PathName.SetIndex(5);
		CHECK(Tree.Find(nullptr, PathName));
		PathName.SetIndex(3);
		Tree.Add(PathName);
		PathName.SetIndex(INDEX_NONE);
		const FPropertyPathNameTree* SubTree = &Tree;
		CHECK(Tree.Find(&SubTree, PathName));
		CHECK_FALSE(SubTree);
		const FPropertyPathNameTree::FConstIterator First = Tree.CreateConstIterator();
		FPropertyPathNameTree::FConstIterator It = Tree.CreateConstIterator();
		CHECK(First == It);
		CHECK_FALSE(First != It);
		CHECKED_IF(It)
		{
			CHECK(It.GetName() == CountName);
			CHECK(It.GetType() == IntType);
			CHECK_FALSE(It.GetSubTree());
			++It;
			CHECK_FALSE(It);
			CHECK_FALSE(First == It);
			CHECK(First != It);
		}
	}

	SECTION("SameNameDiffType")
	{
		FPropertyPathName PathNameInt;
		PathNameInt.Push({CountName, IntType});
		FPropertyPathName PathNameFloat;
		PathNameFloat.Push({CountName, FloatType});
		FPropertyPathNameTree Tree;
		Tree.Add(PathNameInt);
		Tree.Add(PathNameFloat);
		CHECK(Tree.Find(nullptr, PathNameInt));
		CHECK(Tree.Find(nullptr, PathNameFloat));

		FPropertyPathNameTree::FConstIterator It = Tree.CreateConstIterator();
		CHECKED_IF(It)
		{
			CHECK(It.GetType() == IntType);
			CHECK_FALSE(It.GetSubTree());
			++It;
			CHECKED_IF(It)
			{
				CHECK(It.GetType() == FloatType);
				CHECK_FALSE(It.GetSubTree());
				++It;
				CHECK_FALSE(It);
			}
		}
	}

	SECTION("DiffNameSameType")
	{
		FPropertyPathName PathNameCount;
		PathNameCount.Push({CountName, IntType});
		FPropertyPathName PathNameSize;
		PathNameSize.Push({SizeName, IntType});
		FPropertyPathNameTree Tree;
		Tree.Add(PathNameCount);
		Tree.Add(PathNameSize);
		CHECK(Tree.Find(nullptr, PathNameCount));
		CHECK(Tree.Find(nullptr, PathNameSize));

		FPropertyPathNameTree::FConstIterator It = Tree.CreateConstIterator();
		CHECKED_IF(It)
		{
			CHECK(It.GetName() == CountName);
			CHECK_FALSE(It.GetSubTree());
			++It;
			CHECKED_IF(It)
			{
				CHECK(It.GetName() == SizeName);
				CHECK_FALSE(It.GetSubTree());
				++It;
				CHECK_FALSE(It);
			}
		}
	}

	SECTION("Tree")
	{
		FPropertyPathName ParentPathName;
		ParentPathName.Push({NAME_Vector, VectorType});
		FPropertyPathName PathNameA = ParentPathName;
		PathNameA.Push({CountName, IntType});
		FPropertyPathName PathNameB = ParentPathName;
		PathNameB.Push({SizeName, FloatType});
		FPropertyPathNameTree Tree;
		Tree.Add(PathNameA);
		Tree.Add(PathNameB);
		CHECK(Tree.Find(nullptr, ParentPathName));
		CHECK(Tree.Find(nullptr, PathNameA));
		CHECK(Tree.Find(nullptr, PathNameB));

		FPropertyPathNameTree::FConstIterator ParentIt = Tree.CreateConstIterator();
		CHECKED_IF(ParentIt)
		{
			CHECK(ParentIt.GetName() == NAME_Vector);
			CHECK(ParentIt.GetType() == VectorType);
			const FPropertyPathNameTree* ChildTree = ParentIt.GetSubTree();
			++ParentIt;
			CHECK_FALSE(ParentIt);
			CHECKED_IF(ChildTree)
			{
				FPropertyPathNameTree::FConstIterator ChildIt = ChildTree->CreateConstIterator();
				CHECKED_IF(ChildIt)
				{
					CHECK(ChildIt.GetName() == CountName);
					CHECK_FALSE(ChildIt.GetSubTree());
					++ChildIt;
					CHECKED_IF(ChildIt)
					{
						CHECK(ChildIt.GetName() == SizeName);
						CHECK_FALSE(ChildIt.GetSubTree());
						++ChildIt;
						CHECK_FALSE(ChildIt);
					}
				}
			}
		}

		FPropertyPathNameTree* ChildTree = nullptr;
		CHECK(Tree.Find(&ChildTree, ParentPathName));
		CHECKED_IF(ChildTree)
		{
			CHECK(ChildTree->Find(nullptr, PathNameA, 1));
			CHECK(ChildTree->Find(nullptr, PathNameB, 1));
		}

		FPropertyPathNameTree* MissingTree = &Tree;
		CHECK_FALSE(Tree.Find(&MissingTree, PathNameA, 1));
		CHECK(MissingTree == &Tree);
	}
}

} // UE

#endif // WITH_TESTS
