// Copyright Epic Games, Inc.All Rights Reserved.

#include "Utilities/ShooterTestsActorTest.h"

#if WITH_AUTOMATION_TESTS

#include "Components/CQTestBlueprintHelper.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "Equipment/LyraPickupDefinition.h"
#include "Misc/Paths.h"
#include "ObjectBuilder.h"
#include "Weapons/LyraWeaponInstance.h"
#include "Weapons/LyraWeaponSpawner.h"

/**
 * Creates a standalone test object using the name from the first parameter, in the case `InputCrouchAnimationTest`, which inherits from `ShooterTestsActorAnimationTest<Derived, AsserterType>` to provide us our testing functionality.
 * The second parameter specifies the category and subcategories used for displaying within the UI
 * 
 * Note that this test uses the ACTOR_ANIMATION_TEST macro, which is defined in `/Utilities/ShooterTestsActorTest.h`, to inherit from a base class with user-defined variables and methods. Reference that document for more information.
 *
 * The test object will test animation playback during specific inputs while crouched. All variables are reset after each test iteration.
 * 
 * The test makes use of the `TestCommandBuilder` to queue up latent commands to be executed on every Tick of the Engine/Editor
 * `Do` and `Then` steps will execute within a single tick
 * `Until` steps will keep executing each tick until the predicate has evaluated to true or the timeout period has elapsed. The latter will fail the test.
 *
 * Each TEST_METHOD will register with the `InputCrouchAnimationTest` test object and has the variables and methods from `InputCrouchAnimationTest` available for use.
 */
ACTOR_ANIMATION_TEST(InputCrouchAnimationTest, "Project.Functional Tests.ShooterTests.Actor.Animation")
{
	// Make a call to our base Constructor to set the directory and name of the level to load
	InputCrouchAnimationTest() : ShooterTestsActorAnimationTest(TEXT("/ShooterTests/Maps"), TEXT("L_ShooterTest_Basic"))
	{
	}
	
	/**
	 * Run before each TEST_METHOD to load our level, initialize our Player, and Player components needed for the tests before to execute successfully.
	 * The test also makes sure that the Player is playing a crouching idle animation after toggling the Player's crouched state.
	 * If an ASSERT_THAT fails at any point, the TEST_METHODS will also fail as this means that our test prerequisites were not setup
	 * 
	 * Note that because we're derived from a user-defined base class, that we will also call our base `Setup`. It's importanrt to know that `BEFORE_EACH` is a macro that wraps around `virtual void Setup() override`
	 */
	BEFORE_EACH()
	{
		ShooterTestsActorAnimationTest::Setup();

		TestCommandBuilder
			.Do([this]() { GetExpectedAnimation(FShooterTestsAnimationTestHelper::PistolCrouchIdleAnimationName); })
			.Then([this]() { PawnActions->ToggleCrouch(); })
			.Until([this]() { return AnimationTestHelper.IsAnimationPlaying(PlayerMesh, ExpectedAnimation); });
	}
	
	/**
	 * Each test is registered with the name of the type of movement being tested and checks both the expected animation and the input action used to trigger the animation
	 * Due to the nature of the test being the same with the exception of the animation name and the input action, we have a helper method implemented in from our base class being used
	 */
	TEST_METHOD(PlayerCrouched_ForwardMovement)
	{
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::PistolCrouchWalkForwardAnimationName, [this]() { PawnActions->MoveForward(); });
	}
	
	TEST_METHOD(PlayerCrouched_BackwardMovement)
	{
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::PistolCrouchWalkBackwardAnimationName, [this]() { PawnActions->MoveBackward(); });
	}
	
	TEST_METHOD(PlayerCrouched_StrafeLeftMovement)
	{
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::PistolCrouchStrafeLeftAnimationName, [this]() { PawnActions->StrafeLeft(); });
	}
	
	TEST_METHOD(PlayerCrouched_StrafeRightMovement)
	{
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::PistolCrouchStrafeRightAnimationName, [this]() { PawnActions->StrafeRight(); });
	}
	
	TEST_METHOD(PlayerCrouched_RotateLeftMovement)
	{
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::PistolCrouchRotateLeftAnimationName, [this]() { PawnActions->RotateLeft(); });
	}
	
	TEST_METHOD(PlayerCrouched_RotateRightMovement)
	{
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::PistolCrouchRotateRightAnimationName, [this]() { PawnActions->RotateRight(); });
	}
};

/**
 * Creates a standalone test object using the name from the first parameter, in the case `WeaponMeleeAnimationTest`, that inherits from `ShooterTestsActorAnimationTest<Derived, AsserterType>` to provide us our testing functionality.
 * The second parameter specifies the category and subcategories used for displaying within the UI
 * The third parameter specifies the flags as to what context the test will run in and the filter to be applied for the test to appear in the UI
 *
 * Note that this test uses the ACTOR_ANIMATION_TEST macro, which is defined in `/Utilities/ShooterTestsActorTest.h`, to inherit from a base class with user-defined variables and methods. Reference that document for more information.
 * This is a test which will be run in the Editor context as this test spawns objects based on Blueprints found on the local filesystem. Assets require to be cooked along the game to run in the game/client
 *
 * The test object will test spawning weapons for the Player to equip. Test will also verify that the proper melee animation is triggered for the respective input and equipped weapon. All variables are reset after each test iteration.
 * The test makes use of the `TestCommandBuilder` to queue up latent commands to be executed on every Tick of the Engine/Editor
 * `StartWhen` steps will keep executing each tick until the predicate has evaluated to true or the timeout period has elapsed. The latter will fail the test.
 *
 * Each TEST_METHOD will register with the `WeaponMeleeAnimationTest` test object and has the variables and methods from `WeaponMeleeAnimationTest` available for use.
 */
ACTOR_ANIMATION_TEST_WITH_FLAGS(WeaponMeleeAnimationTest, "Project.Functional Tests.ShooterTests.Actor.Animation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	FCQTestBlueprintHelper BlueprintHelper;

	ALyraWeaponSpawner* WeaponSpawnerPad{ nullptr };

	// Make a call to our base Constructor to set the directory and name of the level to load
	WeaponMeleeAnimationTest() : ShooterTestsActorAnimationTest(TEXT("/ShooterTests/Maps"), TEXT("L_ShooterTest_Basic"))
	{
	}
	
	// Checks to see if the specified Weapon is currently equipped by the player
	bool IsCurrentlyEquippedWeapon(const FString& WeaponName)
	{
		if (ULyraEquipmentManagerComponent* EquipmentManager = Player->FindComponentByClass<ULyraEquipmentManagerComponent>())
		{
			if (ULyraWeaponInstance* WeaponInstance = EquipmentManager->GetFirstInstanceOfType<ULyraWeaponInstance>())
			{
				const bool bIsWeaponEquipped = IsValid(WeaponInstance) && IsValid(WeaponInstance->GetInstigator());
				return bIsWeaponEquipped && WeaponInstance->GetClass()->GetName().Equals(WeaponName);
			}
		}
	
		return false;
	}
	
	// Spawns the weapon pad with the appropriate weapon to be picked up by the Player
	void SpawnWeaponSpawnerPad(const FString& WeaponDataAssetPath, const FString& WeaponDataAsset)
	{
		const FString PadDirectory = TEXT("/ShooterCore/Blueprint");
		const FString WeaponDirectory = FPaths::Combine(TEXT("/ShooterCore/Weapons"), WeaponDataAssetPath);
	
		UClass* WeaponSpawnerPadBp = BlueprintHelper.GetBlueprintClass(PadDirectory, TEXT("B_WeaponSpawner"));
		ULyraWeaponPickupDefinition* WeaponDefinition = Cast<ULyraWeaponPickupDefinition>(BlueprintHelper.FindDataBlueprint(WeaponDirectory, WeaponDataAsset));
		ASSERT_THAT(IsNotNull(WeaponDefinition));
	
		WeaponSpawnerPad = &TObjectBuilder<ALyraWeaponSpawner>(*Spawner, WeaponSpawnerPadBp)
			.SetParam("WeaponDefinition", WeaponDefinition)
			.Spawn(Player->GetTransform());
	
		ASSERT_THAT(IsNotNull(WeaponSpawnerPad));
	}

	// Waits until the weapon is spawned and equipped
	void EquipSpawnedWeapon(const FString& WeaponDataAssetPath, const FString& WeaponDataAsset, FString&& EquippedWeaponInstanceName)
	{
		SpawnWeaponSpawnerPad(WeaponDataAssetPath, WeaponDataAsset);

		TestCommandBuilder
			.StartWhen([this, EquippedWeaponInstanceName = MoveTemp(EquippedWeaponInstanceName)]() { return IsCurrentlyEquippedWeapon(EquippedWeaponInstanceName); });
	}
	
	/**
	 * Each test is registered with the name of the type of movement being tested and checks both the expected animation and the input action used to trigger the animation
	 * Due to the nature of the test being the same with the exception of the animation name and the input action, we use helper methods to implement the steps
	 */
	TEST_METHOD(WeaponMelee_Pistol)
	{
		EquipSpawnedWeapon(TEXT("/Pistol/"), TEXT("WeaponPickupData_Pistol"), TEXT("B_WeaponInstance_Pistol_C"));
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::PistolMeleeAnimationName, [this]() { PawnActions->PerformMelee(); });
	}
	
	TEST_METHOD(WeaponMelee_Rifle)
	{
		EquipSpawnedWeapon(TEXT("/Rifle/"), TEXT("WeaponPickupData_Rifle"), TEXT("B_WeaponInstance_Rifle_C"));
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::RifleMeleeAnimationName, [this]() { PawnActions->PerformMelee(); });
	}
	
	TEST_METHOD(WeaponMelee_Shotgun)
	{
		EquipSpawnedWeapon(TEXT("/Shotgun/"), TEXT("WeaponPickupData_Shotgun"), TEXT("B_WeaponInstance_Shotgun_C"));
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::ShotgunMeleeAnimationName, [this]() { PawnActions->PerformMelee(); });
	}
};

#endif // WITH_AUTOMATION_TESTS