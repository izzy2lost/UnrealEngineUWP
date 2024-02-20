[Horde](../Home.md) > Getting Started: Build Automation

# Getting Started: Build Automation

## Introduction

Horde implements a build automation system using BuildGraph as a scripting language, which supports Windows, Mac and
Linux. It integrates closely with Horde's remote execution capabilities, UnrealGameSync, and other tools in the
Unreal Engine ecosystem.

The terms Continuous Integration (CI) and Continuous Delivery (CD) are common monikers for the purposes of build
automation; ensuring that the state of a project is continuously being monitored and that builds are produced
regularly. The terms are used interchangably in this documentation.

## Prerequisites

* Horde Server installation
* One or more machines to function as build workers
* A Perforce server with a UE project

## Steps

1. Open config and data folder for the server (`C:\ProgramData\Epic\Horde\Server`).
2. Open the `globals.json` file.
3. Configure your Perforce server in the `perforceClusters` section of the globals.json file.
4. Uncomment the example `ue5` project listed in the `projects` section at the top of the `globals.json` file.
5. Open the `ue5-main.stream.json` in the same directory as your `globals.json` file.
6. Update the `name` property to refer to the stream containing your project.
7. Update the `Project` and `ProjectPath` macros to refer to your project.
8. Download the `Horde Agent` installer from the tools page on the server. For Windows, it's easiest to use the MSI
   installer. Install the agent on a worker machine, entering the URL of the Horde Server when prompted.
9. Click on the `Agents` link from the `Server` menu and make sure the agent has registered with the server correctly.
   A new agent must manually be enabled. It should have automatically been added to the correct pool.
10. Attempt a test build.
