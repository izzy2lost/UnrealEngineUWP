// Copyright Epic Games, Inc. All Rights Reserved.


#include "Elements/Framework/TypedElementAttributeBinding.h"
#if WITH_TESTS
#include "Elements/Framework/TypedElementTestColumns.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Misc/AutomationTest.h"

namespace TypedElementDataStorageTests
{
	BEGIN_DEFINE_SPEC(TypedElementAttributeBindingTestsFixture, "EditorDataStorage.AttributeBinding", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

		ITypedElementDataStorageInterface* TedsInterface = nullptr;
		const FName TestTableName = TEXT("TestTable_AttributeBinding");
		TypedElementDataStorage::TableHandle TestTableHandle = TypedElementDataStorage::InvalidTableHandle;
		TypedElementDataStorage::RowHandle TestRowHandle = TypedElementDataStorage::InvalidRowHandle;

		TypedElementDataStorage::TableHandle RegisterTestTable() const
		{
			const TypedElementDataStorage::TableHandle Table = TedsInterface->FindTable(TestTableName);
			
			if (Table != TypedElementDataStorage::InvalidTableHandle)
			{
				return Table;
			}
			
			return TedsInterface->RegisterTable(
			{
				FTestColumnInt::StaticStruct(),
				FTestColumnString::StaticStruct()
			},
			TestTableName);
		}
	
		TypedElementDataStorage::RowHandle CreateTestRow(TypedElementDataStorage::TableHandle InTableHandle) const
		{
			const TypedElementDataStorage::RowHandle RowHandle = TedsInterface->AddRow(InTableHandle);
			return RowHandle;
		}
	
		void CleanupTestRow(TypedElementDataStorage::RowHandle InRowHandle) const
		{
			TedsInterface->RemoveRow(InRowHandle);
		}
	
	END_DEFINE_SPEC(TypedElementAttributeBindingTestsFixture)

	void TypedElementAttributeBindingTestsFixture::Define()
	{
		BeforeEach([this]()
		{
			UTypedElementRegistry* TypedElementRegistry = UTypedElementRegistry::GetInstance();
			TedsInterface = TypedElementRegistry->GetMutableDataStorage();
			TestTrue("", TedsInterface != nullptr);
			
			TestTableHandle = RegisterTestTable();
			TestNotEqual("Expecting valid table handle", TestTableHandle, TypedElementDataStorage::InvalidTableHandle);

			TestRowHandle = CreateTestRow(TestTableHandle);
			TestFalse("Expect valid row handle", TestRowHandle == TypedElementDataStorage::InvalidRowHandle);
		});

		Describe("", [this]()
		{
			Describe("Integer Attribute", [this]()
			{
				Describe("Direct integer attribute", [this]()
				{
					It("Direct attribute should update on updating column value", [this]()
					{
						constexpr int InitialValue = 10;
						constexpr int UpdatedValue = 20;
						
						// Add the test int column to the test row
						TedsInterface->AddColumn(TestRowHandle, FTestColumnInt{.TestInt =  InitialValue});
						FTestColumnInt* TestColumnInt = TedsInterface->GetColumn<FTestColumnInt>(TestRowHandle);

						TestNotNull("Expecting Valid Column", TestColumnInt);

						// Create an int attribute and bind it
						UE::EditorDataStorage::FAttributeBinder Binder(TestRowHandle);
						const TAttribute TestAttribute(Binder.BindData(&FTestColumnInt::TestInt));

						TestEqual("Expecting attribute value to match column value before modification", TestAttribute.Get(), TestColumnInt->TestInt);

						// Modify the value in the column
						TestColumnInt->TestInt = UpdatedValue;

						TestEqual("Expecting attribute value to update after modification", TestAttribute.Get(), UpdatedValue);
						TestEqual("Expecting attribute value to match column value after modification", TestAttribute.Get(), TestColumnInt->TestInt);
					});
				});

				Describe("Float attribute bound to integer column data", [this]()
				{
					It("Converted attribute should update on updating column value", [this]()
					{
						constexpr int InitialValue = 10;
						constexpr int UpdatedValue = 20;

						// Add the test int column to the test row
						TedsInterface->AddColumn(TestRowHandle, FTestColumnInt{.TestInt =  InitialValue});
						FTestColumnInt* TestColumnInt = TedsInterface->GetColumn<FTestColumnInt>(TestRowHandle);

						TestNotNull("Expecting valid column", TestColumnInt);

						// Create a float attribute and bind it by providing a conversion function
						UE::EditorDataStorage::FAttributeBinder Binder(TestRowHandle);
						const TAttribute<float> TestAttribute(Binder.BindData(&FTestColumnInt::TestInt,
							[](const int& Data)
							{
								return static_cast<float>(Data);
							}));

						TestEqual("Expecting attribute value to match column value before modification", static_cast<int>(TestAttribute.Get()), TestColumnInt->TestInt);

						// Modify the value in the column
						TestColumnInt->TestInt = UpdatedValue;

						TestEqual("Expecting attribute value to update after modification", static_cast<int>(TestAttribute.Get()), UpdatedValue);
						TestEqual("Expecting attribute value to match column value after modification", static_cast<int>(TestAttribute.Get()), TestColumnInt->TestInt);
					});
				});
			});

			Describe("String Attribute", [this]()
			{
				Describe("Direct string attribute", [this]()
				{
					It("Direct attribute should update on updating column value", [this]()
					{
						const FString InitialValue(TEXT("Test String"));
						const FString UpdatedValue(TEXT("Test string after modification"));
						
						// Add the test int column to the test row
						TedsInterface->AddColumn(TestRowHandle, FTestColumnString{.TestString = InitialValue});
						FTestColumnString* TestColumnString = TedsInterface->GetColumn<FTestColumnString>(TestRowHandle);

						TestNotNull("Expecting valid column", TestColumnString);

						// Create an int attribute and bind it
						UE::EditorDataStorage::FAttributeBinder Binder(TestRowHandle);
						const TAttribute TestAttribute(Binder.BindData(&FTestColumnString::TestString));

						TestEqual("Expecting attribute value to match column value before modification", TestAttribute.Get(), TestColumnString->TestString);

						// Modify the value in the column
						TestColumnString->TestString = UpdatedValue;
						
						TestEqual("Expecting attribute value to update after modification", TestAttribute.Get(), UpdatedValue);
						TestEqual("Expecting attribute value to match column value after modification", TestAttribute.Get(), TestColumnString->TestString);
					});
				});

				Describe("Text attribute bound to string column data", [this]()
				{
					It("Converted attribute should update on updating column value", [this]()
					{
						const FString InitialValue(TEXT("Test String"));
						const FString UpdatedValue(TEXT("Test string after modification"));

						// Add the test int column to the test row
						TedsInterface->AddColumn(TestRowHandle, FTestColumnString{.TestString = InitialValue});
						FTestColumnString* TestColumnString = TedsInterface->GetColumn<FTestColumnString>(TestRowHandle);

						TestNotNull("Expecting valid column", TestColumnString);

						// Create an int attribute and bind it
						UE::EditorDataStorage::FAttributeBinder Binder(TestRowHandle);
						const TAttribute<FText> TestAttribute(Binder.BindData(&FTestColumnString::TestString,
							[](const FString& Data)
							{
								return FText::FromString(Data);
							}));

						TestEqual("Expecting attribute value to match column value before modification", TestAttribute.Get().ToString(), TestColumnString->TestString);

						// Modify the value in the column
						TestColumnString->TestString = UpdatedValue;

						TestEqual("Expecting attribute value to update after modification", TestAttribute.Get().ToString(), UpdatedValue);
						TestEqual("Expecting attribute value to match column value after modification", TestAttribute.Get().ToString(), TestColumnString->TestString);
					});
				});
			});

			Describe("Default Value", [this]()
			{
				It("Default value should be used when column isn't present", [this]()
				{
					constexpr int DefaultValue = 10;
					
					UE::EditorDataStorage::FAttributeBinder Binder(TestRowHandle);

					// Create an int attribute and directly bind it
					const TAttribute TestIntAttribute(Binder.BindData(&FTestColumnInt::TestInt, DefaultValue));

					// Create a float attribute and bind it by providing a conversion function
					const TAttribute<float> TestFloatAttribute(Binder.BindData(&FTestColumnInt::TestInt,
						[](const int& Data)
						{
							return static_cast<float>(Data);
						}, DefaultValue));


					// Remove FTestColumnInt from TestRowHandle so the default value is used
					TedsInterface->RemoveColumn(TestRowHandle, FTestColumnInt::StaticStruct());

					TestEqual("Expecting int attribute value to match default value", TestIntAttribute.Get(), DefaultValue);
					TestEqual("Expecting float attribute value to match default value", static_cast<int>(TestFloatAttribute.Get()), DefaultValue);

				});
			});
		});

		AfterEach([this]()
		{
			CleanupTestRow(TestRowHandle);
			TestRowHandle = TypedElementDataStorage::InvalidRowHandle;
			TestTableHandle = TypedElementDataStorage::InvalidTableHandle;
			TedsInterface = nullptr;
		});
	}
}

#endif // WITH_TESTS