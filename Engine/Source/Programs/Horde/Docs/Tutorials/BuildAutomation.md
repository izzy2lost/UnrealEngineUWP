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

* Install the [Horde server](InstallServer.md)
* One or more machines to function as build workers

## Steps

1. Install the Horde Server [as described here](InstallServer.md).
2. Open your Horde installation folder with an administrator account (`C:\Program Files\Epic Games\Horde\Server`).
3. Open the `globals.json` file.
4. Configure your Perforce server in the `perforceClusters` section of the globals.json file.
5. Uncomment the example `ue5` project listed in the `projects` section at the top of the `globals.json` file.
6. Open the `ue5-main.stream.json` in the same directory as your `globals.json` file.
7. Update the `name` property to refer to the stream containing your project.
8. Update the `Project` and `ProjectPath` macros to refer to your project.
9. Download the `Horde Agent` installer from the tools page on the server. For Windows, it's easiest to use the MSI
   installer. Install the agent on a worker machine, entering the URL of the Horde Server when prompted.
10. Click on the `Agents` link from the `Server` menu and make sure the agent has registered with the server correctly.
  It should have automatically been added to the correct pool.
11. Attempt a test build.
