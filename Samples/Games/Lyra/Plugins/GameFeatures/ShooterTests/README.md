# Shooter Tests Plugin

**Shooter Tests** is a plugin which is bundled as part of the **Lyra** game content and contains tests which are run against **Lyra**. This document is meant to serve as a guide on working with the plugin and providing a detailed overview on the provided tests.

This guide is split up into two main sections each with their own focus. The first section provides an overview of the plugin while the second section goes into detail about the different test types and tests available
- [Plugin Overview](#plugin-overview)
  - [Plugin Structure](#plugin-structure)
  - [How to run tests provided by the plugin](#how-to-run-tests-provided-by-the-plugin)
- [Tests Within the Plugin](#tests-within-the-plugin)
  - [CQTests](#cqtests)
    - [Animation Test Prerequisites](#animation-test-prerequisites)
    - [InputCrouchAnimationTest](#inputcrouchanimationtest)
    - [WeaponMeleeAnimationTest](#weaponmeleeanimationtest)
    - [AbilitySpawnerMapTest](#abilityspawnermaptest)
  - [Blueprint Functional Tests](#blueprint-functional-tests)
    - [B\_Test\_AutoRun](#b_test_autorun)
    - [B\_Test\_FireWeapon](#b_test_fireweapon)
- [Resources](#resources)

## Plugin Overview

The scope of this document is to only cover the **Shooter Tests** plugin. For a more generalized guide to plugins, including how to create a new plugin or enabling/disabling existing plugins, please see [the online documentation on plugins](https://dev.epicgames.com/documentation/en-us/unreal-engine/plugins-in-unreal-engine) for more information.

This section provides an overview of the plugin. The plugin comes bundled as part of the **Lyra** starter content which can be downloaded from [the Epic Games Launcher](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine) and can be found in `/LyraStarterGame/Plugins/GameFeatures/ShooterTests`. The underlying sections below will go into further detail about different aspects of the plugin. Specifics include: the structure of the plugin, how to enable the plugin for use within Lyra, and how to run the tests packaged with the plugin.

### Plugin Structure

Below is the general document structure of the plugin

```
.
└── ShooterTests/
    ├── Binaries/
    │   └── Platform dependant built libraries
    ├── Content/
    │   ├── Blueprint
    │   ├── Input
    │   ├── Maps
    │   ├── System
    │   ├── ShooterTests.uasset
    ├── Intermediate/
    │   └── Platform dependant generated code
    ├── Resources/
    │    └── Icon128.png
    └── Source/
        └── ShooterTestsRuntime/
            └── Private/
```

The focus and scope of this document will be on the `Content` and `Source` directories as this will be where the tests and resources needed for tests will reside. For example, the [Blueprint Functional Tests](#blueprint-functional-tests) will have a Blueprint class for the test stored within the `Blueprint` directory, but the map that loads the Blueprint asset will be found in the `Map` directory. While it seems that the pieces of the test are scattered within the plugin, they all reside in the appropriate folder as some assets are reused in other tests. It's best practice to split up your components and organize them in a way that makes sense as opposed to bundling all pieces of a test together as the latter can lead to duplication of assets and a larger plugin size.

### How to run tests provided by the plugin

This section requires knowledge about the **Automation System** and how to navigate the **Automation** tab of the **Session Frontend** to find and execute tests. Please see [the online documentation on the Automation System](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-system-user-guide-in-unreal-engine) for more information. With the **Session Frontend** opened to the **Automation** tab, we can navigate to `Project -> Functional Tests -> Shooter Tests` in order to get to the tests currently packaged within the plugin. These tests can also be reached by using the **Search** bar on top and using the keyword **ShooterTests** to filter out and only show the tests in this plugin. See the section immediately below for more detailed explanation on what types of tests are included and what is being tested.

## Tests Within the Plugin

The plugin demonstrates tests using either Blueprint or code. This section describes various tests packaged with the plugin.

* [CQTests](#cqtests)
* [Blueprint Functional Tests](#blueprint-functional-tests)

### CQTests

**CQTest**, or Code Quality Tests, are a method of creating functional tests using C++. The **CQTest** framework resides in a plugin that is provided with Unreal Engine and is enabled within **ShooterTests**. Unreal Engine provides multiple testing frameworks, but the focus on CQTest was decided due to providing before/after functionality that is paired with each test case. Another benefit is that **CQTest** resets the state of each test automatically, making sure that each test is atomic in that there is no worry about leaking objects to or from another test. Please refer to the readme documentation located in `/Engine/Plugins/Test/CQTest` for a deeper understanding of **CQTest**.

**Shooter Tests** are tests implemented using the **CQTest** framework and they are implemented within their respective categories. Within the categories, it's possible to have subcategories to help further define the type or functionality expected to be tested. While [Blueprint Functional Tests](#blueprint-functional-tests) are categorized in the **Automation** tab by the map that they reside in. Similar to how clicking on a [Blueprint Functional Tests](#blueprint-functional-tests) will load the level with the Functional Test Actor, clicking on a **CQTest** will open the code file where the test is implemented.

Tests created using the **CQTest** framework allows for custom categorization when declaring the `TEST_CLASS` or `TEST` itself. For example

```
TEST(SimpleTest, "Project.Functional Tests.ShooterTests.Tests")
{
    ASSERT_THAT(IsTrue(true));
}
```

will show an item labeled `SimpleTest` under `Project -> Functional Tests -> ShooterTests -> Tests` within the **Automation** tab. More information about the **CQTest** framework is documented in the readme mentioned above and the examples outlined below.

Here are the current categories and subcategories of the tests within the plugin:

* **Actor**
  * **Animation**
    * [InputCrouchAnimationTest](#inputcrouchanimationtest)
    * [WeaponMeleeAnimationTest](#weaponmeleeanimationtest)
* **GameplayAbility**
  * [AbilitySpawnerMapTest](#abilityspawnermaptest)

#### Animation Test Prerequisites

Please note that the [InputCrouchAnimationTest](#inputcrouchanimationtest) and the [WeaponMeleeAnimationTest](#weaponmeleeanimationtest) below follow a custom implementation of the `TEST_CLASS`. **CQTest** provides the following macros when creating tests:

* `TEST`
* `TEST_CLASS`
* `TEST_CLASS_WITH_BASE`
* `TEST_CLASS_WITH_FLAGS`
* `TEST_CLASS_WITH_BASE_AND_FLAGS`
* `TEST_METHOD`

The Animation tests use the above to create 2 new macros:

* `ACTOR_ANIMATION_TEST`
  * Wraps the `TEST_CLASS_WITH_BASE` macro specifying the `ShooterTestsActorAnimationTest` as the base struct.
* `ACTOR_ANIMATION_TEST_WITH_FLAGS`
  * Wraps the `TEST_CLASS_WITH_BASE_AND_FLAGS` macro specifying the `ShooterTestsActorAnimationTest` as the base struct.

The newly defined macros take in a new user defined base class called `ShooterTestsActorAnimationTest` which has been created to help with testing the animation functionality. However, `ShooterTestsActorAnimationTest` is also derived from `ShooterTestsActorBaseTest`. The reason being is similar to how we have a category of tests for an `Actor` with a subcategory of `Animation`, we also have a base class for the Actor called `ShooterTestsActorBaseTest` with `ShooterTestsActorAnimationTest` being derived. This way we can keep the members needed for animations separate from tests that just need or will expand upon the Actor.

To use our newly defined `ACTOR_ANIMATION_TEST` macros, we need to specify `ShooterTestsActorAnimationTest` as the base class, but if `ShooterTestsActorAnimationTest` is derived from `ShooterTestsActorBaseTest`, how do we exactly create a base class to then be used in our macro? If we focus on `ShooterTestsActorBaseTest` we can see that this class inherits from `public TTest<Derived, AsserterType>` which is a templated object. From there we can create the `ShooterTestsActorAnimationTest` object which derives from `public ShooterTestsActorBaseTest<Derived, AsserterType>`, again with the template specified. With this we can now have our `ShooterTestsActorAnimationTest` object which provides access and functionality to the animation tests as well as providing functionality from `ShooterTestsActorBaseTest` for our Actor.

The animation tests make use of helper functionality provided by `ShooterTestsAnimationTestHelper` and `ShooterTestsInputTestHelper`.

**ShooterTestsAnimationTestHelper**
The `ShooterTestsAnimationTestHelper` object that exists in this file aids in finding `UAnimationAsset` objects and checking if the asset is playing. The object requires a `USkeletalMeshComponent` in order to retrieve animation assets and information. `FindAnimationAsset` will search through the `USkeletalMeshComponent` to make sure that an asset with the same name exists within the component. `IsAnimationPlaying` iterates through all of the animation instances of the `USkeletalMeshComponent` before checking any active montages and sync groups for the expected animation to be playing.

**ShooterTestsInputTestHelper**
Within this file are multiple input objects that the **Lyra** player has access to. We have multiple `FTestAction` objects for each `Input Action` that is tested against. Currently the following `Input Actions` are implemented and being handled:

* `FToggleCrouchTestAction`
* `FMeleeTestAction`
* `FMoveTestAction`
* `FLookTestAction`

In addition to the actions mentioned above, both `FMoveTestAction` and `FLookTestAction` have additional derived objects to handle the different directions along the 2D axis. These include:

* `FMoveForwardTestAction`
* `FMoveBackwardTestAction`
* `FStrafeLeftTestAction`
* `FStrafeRightTestAction`
* `FRotateLeftTestAction`
* `FRotateRightTestAction`

While the **Lyra** player has more `Input Actions` available, these are what are currently handled based on the current test coverage and more will be added as more tests are created.

To be able to trigger all these `Input Actions` the `FShooterTestsPawnTestActions` object extends from `FInputTestActions` to specify each action that will be performed. While all the input follows the same flow of calling `PerformAction` on a `FTestAction` object, we expand upon the actions to make the tests more descriptive as to what is tested against and what is expected to happen.

#### InputCrouchAnimationTest

**Please see the section above about [Animation Test Prerequisites](#animation-test-prerequisites) before going deeper into this section as there are custom implementations of base **CQTest** functionality that may cause confusion.**

The **InputCrouchAnimationTest** is a test object created from the macro `ACTOR_ANIMATION_TEST` and the implementation can be found in `/ShooterTests/Source/ShooterTestsRuntime/Private/ShooterTestsActorAnimationTests.cpp`. These test that the expected animations are played during certain movement actions while crouched. 

The breakdown of the implementation is as follows:
We create our test object with the `ACTOR_ANIMATION_TEST` macro and the parameters `InputCrouchAnimationTest` and `"Project.Functional Tests.ShooterTests.Actor.Animation"`. As mentioned in the [Animation Test Prerequisites](#animation-test-prerequisites), the macro creates our test object with a `TTestRunner` and an instance of the `InputCrouchAnimationTest`. Because the macro uses `ShooterTestsActorAnimationTest` as our base object, which is also derived from `ShooterTestsActorBaseTest`, we get access to all of the member variables and methods provided by these objects. This will allow us to create our `TEST_METHOD` without the need to setup the `PlayerController` or any other components as the `BEFORE_EACH` will handle that for us.

The `InputCrouchAnimationTest` constructor is defined as a way to call the base object's constructor with an initializer list to provide the directory where the map to load is located and the name of the map to load. This is due to the fact that the `ShooterTestsActorBaseTest` will create an instance of a `FMapTestSpawner`. This object handles loading of maps whenever `ShooterTestsActorBaseTest::Setup()` is called.

`BEFORE_EACH` goes through and prepares the level for the test as the macro has defined a `virtual void Setup() override` which is what allows the call to `ShooterTestsActorAnimationTest::Setup();` to be made. `ShooterTestsActorAnimationTest::Setup();` actually calls the base object's `Setup`, which in this case will be `ShooterTestsActorBaseTest`. What the setup will then do is start loading the map that was specified in the Constructor and waits until the level has been loaded.

**Note that while the level has been loaded and marked for use, the actual assets may not have fully populated within the level yet.**

The `TestCommandBuilder` is then used to build the steps needed to ensure that the level is setup prior to the test.

* Wait before starting until the map has been initialized and a player `Pawn` has been found for use. This step defaults to a timeout of 10 seconds, but we increase the timeout to 30 seconds to take into account additional loading done via the loading screen.
* Then, with the `Pawn` loaded a call to `ShooterTestsActorAnimationTest::PreparePlayerPawn()` is made, which also calls `ShooterTestsActorBaseTest::PreparePlayerPawn()`
  * `ShooterTestsActorBaseTest::PreparePlayerPawn()` handles setting up the main player and functionality tied to the player which will be used in a later step when determining if the player has fully spawned in.
  * `ShooterTestsActorAnimationTest::PreparePlayerPawn()` handles setting up functionality tied to testing animations.
* With the functionality now setup, we wait until the player is deemed as being fully spawned. This is done because there is a `GameplayEffect` tied to the player which is triggered on spawn. This effect toggles the input handling which could impact tests as we need input to trigger our animations.

At this point we are in the `BEFORE_EACH` of the **InputCrouchAnimationTest** which will again use the `TestCommandBuilder` to continue building on our steps from our base `Setup` now that the player and animation functionality is setup and loaded. The steps here will get the player to be crouched since each `TEST_METHOD` needs the player in this state to continue.

* Do, a command which will get the animation for `MM_Pistol_Crouch_Idle` to be tested against
* Then, trigger the crouch `Input Action` to be executed
* Wait until the player is crouched and playing the expected animation retrieved in the above step

With the `BEFORE_EACH` now setup to be performed before each of our `TEST_METHOD`, we can create all the tests which will verify that the correct animation is playing for each `Input Action`. Because there are multiple tests within the object that all perform the same test flow, but differ in the expected animation to be playing and the `Input Action` needed to be triggered, the method `TestInputActionAnimation` has been implemented as part of the `ShooterTestsActorAnimationTest` object to perform the flow needed with the different parameters. The method fetches the `UAnimationAsset` from the name of the animation before continuing building upon the `TestCommandBuilder`

* Do, the specified input command
* Wait until the expected animation is playing as this can occur over multiple ticks

#### WeaponMeleeAnimationTest

**Please see the section above about [Animation Test Prerequisites](#animation-test-prerequisites) before going deeper into this section as there are custom implementations of base **CQTest** functionality that may cause confusion.**

The **WeaponMeleeAnimationTest** is a test object created from the macro `ACTOR_ANIMATION_TEST_WITH_FLAGS` and the implementation can be found in `/ShooterTests/Source/ShooterTestsRuntime/Private/ShooterTestsActorAnimationTests.cpp`. These test that the expected animations are played during certain movement actions while the player has certain weapons equipped. 

The breakdown of the implementation is as follows:
We create our test object with the `ACTOR_ANIMATION_TEST_WITH_FLAGS` macro and the parameters `InputCrouchAnimationTest`, `"Project.Functional Tests.ShooterTests.Actor.Animation"`, and we specify the flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`. As mentioned in the [Animation Test Prerequisites](#animation-test-prerequisites), the macro creates our test object with a `TTestRunner` and an instance of the `InputCrouchAnimationTest`. Because the macro uses `ShooterTestsActorAnimationTest` as our base object, which is also derived from `ShooterTestsActorBaseTest`, we get access to all of the member variables and methods provided by these objects. This will allow us to create our `TEST_METHOD` without the need to setup the `PlayerController` or any other components as the `BEFORE_EACH` will handle that for us. Because these tests are also spawning items to be used, we specify the flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` to only run these tests within the Editor context.

The **WeaponMeleeAnimationTest** object has additional variables specified to be use.

* `FCQTestBlueprintHelper` will assist with finding Blueprints for the objects which be used to create our `ALyraWeaponSpawner* WeaponSpawnerPad`.
* `WeaponSpawnerPad` will create the weapons for the `Pawn` to pickup and equip for the tests to then perform a melee `Input Action` to trigger an animation to play and be tested against.

The `WeaponMeleeAnimationTest` constructor is defined as a way to call the base object's constructor with an initializer list to provide the directory where the map to load is located and the name of the map to load. This is due to the fact that the `ShooterTestsActorBaseTest` will create an instance of a `FMapTestSpawner`. This object handles loading of maps whenever `ShooterTestsActorBaseTest::Setup()` is called.

Even though we don't specify it, the `BEFORE_EACH` of our base object goes through and prepares the level for the test as the macro has defined a `virtual void Setup() override` which is what allows the call to `ShooterTestsActorAnimationTest::Setup();` to be made. `ShooterTestsActorAnimationTest::Setup();` actually calls the base object's `Setup`, which in this case will be `ShooterTestsActorBaseTest`. What the setup will then do is start loading the map that was specified in the Constructor and waits until the level has been loaded.

**Note that while the level has been loaded and marked for use, the actual assets may not have fully populated within the level yet.**

The `TestCommandBuilder` is then used to build the steps needed to ensure that the level is setup prior to the test.

* Wait before starting until the map has been initialized and a player `Pawn` has been found for use. This step defaults to a timeout of 10 seconds, but we increase the timeout to 30 seconds to take into account additional loading done via the loading screen.
* Then, with the `Pawn` loaded a call to `ShooterTestsActorAnimationTest::PreparePlayerPawn()` is made, which also calls `ShooterTestsActorBaseTest::PreparePlayerPawn()`
  * `ShooterTestsActorBaseTest::PreparePlayerPawn()` handles setting up the main player and functionality tied to the player which will be used in a later step when determining if the player has fully spawned in.
  * `ShooterTestsActorAnimationTest::PreparePlayerPawn()` handles setting up functionality tied to testing animations.
* With the functionality now setup, we wait until the player is deemed as being fully spawned. This is done because there is a `GameplayEffect` tied to the player which is triggered on spawn. This effect toggles the input handling which could impact tests as we need input to trigger our animations.

Each `TEST_METHOD`, while tests the same flow, tests with different weapons equipped to make sure that the correct animation is playing. We are testing against 3 distinct weapons each with their own distinct melee animation. Each test will call the `EquipSpawnedWeapon` which takes in 3 parameters, the directory where the asset can be found, the name of the asset which stores the data to be loaded, and the name of the weapon once created and equipped. The method will call the `SpawnWeaponSpawnerPad` method, which takes in the first 2 of the supplied parameters, to use the `FCQTestBlueprintHelper::GetBlueprintClass` to find our `UClass` of the `ALyraWeaponSpawner` to be spawned. We also call `FCQTestBlueprintHelper::FindDataBlueprint` to get our `ULyraWeaponPickupDefinition` of the weapon instance that will be supplied as a parameter for the `ALyraWeaponSpawner`. With the objects found from `FCQTestBlueprintHelper`, the `TObjectBuilder` is then used to create an instance of the `ALyraWeaponSpawner` to be spawned right under the player `Pawn` with the `WeaponDefinition` parameter set to spawn the desired weapon specified by the test.

Using the `TestCommandBuilder`, we build our steps to start the test the expected weapon is equipped by the player `Pawn`. This is done by checking the `ULyraEquipmentManagerComponent` against the first `ULyraWeaponInstance`, which will be our currently equipped weapon. We also make sure that the equipped weapon matches the expected name of the weapon instance so that we're not testing against a weapon that we're not expecting and that the functionality of equipping a newly spawned weapon is expected. Once we have made sure that the expected weapon is equipped, we can then add the steps to the steps to the `TestCommandBuilder` to perform our melee `Input Action` before waiting to make sure that the correct animation montage was played.

#### AbilitySpawnerMapTest

The **AbilitySpawnerMapTest** is a test object created from the macro `TEST_CLASS_WITH_FLAGS` and the implementation can be found in `/ShooterTests/Source/ShooterTestsRuntime/Private/ShooterTestsMapTests.cpp`. These test gameplay behavior is appropriately applied to the player `Pawn`.

The breakdown of the implementation is as follows:
We create our test object with the `TEST_CLASS_WITH_FLAGS` macro and the parameters `AbilitySpawnerMapTest`, `"Project.Functional Tests.ShooterTests.GameplayAbility"`, and we specify the flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`. Because these tests are also spawning items to be used, we specify the flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` to only run these tests within the Editor context.

The **AbilitySpawnerMapTest** object has additional variables specified to be use.

* `FMapTestSpawner` will spawn the test world for the player `Pawn` and other objects to be spawned into.
* `FCQTestBlueprintHelper` will assist with finding Blueprints for the objects which be used to create our `AActor* GameplayEffectPad`.
* `ALyraCharacter* Player` is the player `Pawn` in the world that the `GameplayEffect` will be applied to.
* `ULyraAbilitySystemComponent* AbilitySystemComponent` is where the information about the `Player` is stored and handled by.
* `const ULyraHealthSet* HealthSet` is where the information about the `Player` health is stored.

In addition to variables, there are also some methods implemented to help with our tests. `DoDamageToPlayer` is a method that takes in a `double` parameter used to apply as our `Player` damage. The method uses the `ULyraAssetManager` to then get the game data for the damage GameplayEffect and makes sure that the handle to the effect is valid. With a valid handle the call can then be made by the `AbilitySystemComponent` to specify the effect and the amount of damage to be applied.

The `SpawnGameplayPad` method takes a parameter for the name of the GameplayEffect. The method calls into `FCQTestBlueprintHelper::GetBlueprintClass` to look at the GameplayDirectory for the specified effect name and returns a `UClass` of the desired effect. Another call to `FCQTestBlueprintHelper::GetBlueprintClass` is done, but this time we are specifically looking for the `UClass` of the GameplayEffect pad. Using the `TObjectBuilder`, we spawn an `AActor` of the GameplayEffect pad with the class of the desired effect to spawn with.

The final method, `IsPlayerDamaged`, just checks to see if the current health is less than the max allowed health.

With the variables and methods supplied by this object, we can now start going into the methods which will describe the flow of the tests. First is the `BEFORE_EACH` which goes through and prepares the level for the test by using the `FMapTestSpawner` to specify our level location and name before waiting until the level has been loaded. Once the level is loaded the `TestCommandBuilder` is then used to build the steps needed to ensure that the level is setup prior to the test.

* Start when we are able to find a valid player `Pawn`. The default timeout is 10 seconds, but **Lyra** uses a loading screen to load additional objects. Because of this we increase the timeout to 30 seconds.
* Do, set the `Player` found from `FMapTestSpawner::FindFirstPlayerPawn`. We also make sure that the `AbilitySystemComponent` is valid and that `HealthSet` is set to the max possible health.

After the `BEFORE_EACH`, we then have `TEST_METHOD` that can be run:
**PlayerOnDamageSpawner_Eventually_LosesHealth**
Uses the `TestCommandBuilder` to build steps that will check that the `Player` loses health when the damage GameplayEffect is spawned.

* Start when the `AbilitySystemComponent` can find the gameplay tag for `TAG_Gameplay_DamageImmunity`
* Then, call the method `SpawnGameplayPad` to spawn our GameplayPad with the damage GameplayEffect.
* Run until we see that the player has been damaged by the effect, `IsPlayerDamaged`.

**PlayerMissingHealth_OnHealSpawner_RestoresHealth**
Uses the `TestCommandBuilder` to build steps that will check that the `Player` loses health when the heal GameplayEffect is spawned.

* Start when the `AbilitySystemComponent` can find the gameplay tag for `TAG_Gameplay_DamageImmunity`
* Then, call the method `DoDamageToPlayer` to apply 10 points of damage directly to our `Player`.
* Run until we see that the player has been damaged by the effect, `IsPlayerDamaged`.
* Then, call the method `SpawnGameplayPad` to spawn our GameplayPad with the healing GameplayEffect.
* Run until we see that the player has been healed by the effect by checking that our player has not been damaged, `!IsPlayerDamaged`.

### Blueprint Functional Tests

The **Shooter Tests** plugin has a few Blueprint functional tests which can be found in `/GameFeatures/ShooterTests/Content/Blueprint`. These tests can be viewed within the Blueprint Editor to help get a better understanding of how the tests are setup and what nodes they are using to accomplish testing the functionality. Please note that when viewing these tests from the **Automation** tab of the **Session Frontend**, the Blueprint Functional Test will reside under the name of the Level. For example, the test [B_Test_AutoRun](#b_test_autorun) will be located under the level name of `L_ShooterTest_Autorun` and clicking on the test itself will open the level unless the Editor already has the level opened. Some of the tests implemented using a Blueprint Functional Test Actor:

* [B_Test_AutoRun](#b_test_autorun)
* [B_Test_FireWeapon](#b_test_fireweapon)

Please see [the online documentation on Blueprint Functional Tests](https://dev.epicgames.com/documentation/en-us/unreal-engine/functional-testing-in-unreal-engine) for more information about how to get get started with creating tests of what a Blueprint Functional Test Actor is.

#### B_Test_AutoRun

The **B_Test_AutoRun** Blueprint Functional Test is setup to check that the **Hero** `Lyra Player Controller` can have the `Input Action` for auto run enabled. Opening up and viewing this Blueprint within the Editor will help show the following:
* How to setup the `Event Start Test` event as the main test entry point
* Using assertion nodes to validate conditions and behaviors
* Creating and triggering of custom events
* Use of Delays to allow other latent commands to execute
* Setting and getting of private Blueprint Function Variables
* Working with Blueprint Components (**Target Point**)
* Working with the local PlayerController to inject input
* Performing checks on every `Event Tick` to validate a successful test condition
* Using functionality to check overlaps between actors

#### B_Test_FireWeapon

The **B_Test_FireWeapon** Blueprint Functional Test is setup to check that the **Hero** has ammo for their starting weapon and upon firing, validates that the weapon has successfully fired by checking if the ammo count has decreased. Opening up and viewing this Blueprint within the Editor will help show the following:
* How to setup the `Event Start Test` event as the main test entry point
* Using Blueprint Macros for reusability
* Using assertion nodes to validate conditions and behaviors
* Creating and triggering of custom events
* Use of Delays to allow other latent commands to execute
* Working with the local PlayerController to inject input

## Resources
* [Plugins in Unreal Engine](https://dev.epicgames.com/documentation/en-us/unreal-engine/plugins-in-unreal-engine)
* [Lyra Sample](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine)
* [Unreal Automation System User guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-system-user-guide-in-unreal-engine)
* [Blueprint Functional Tests](https://dev.epicgames.com/documentation/en-us/unreal-engine/functional-testing-in-unreal-engine)
* [Blueprint Macro](https://dev.epicgames.com/documentation/en-us/unreal-engine/macros-in-unreal-engine)