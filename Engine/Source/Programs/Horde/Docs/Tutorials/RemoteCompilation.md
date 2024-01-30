[Horde](../Home.md) > Getting Started: Remote Compilation

# Getting Started: Remote Compilation

## Introduction

Horde implements a platform for generic remote execution workloads, allowing clients to leverage idle CPU cycles on
other machines to accelerate workloads that would otherwise be executed locally. Horde's remote execution platform
allows issuing explicit commands to remote agents sequentially, such as "upload these files", "run this process",
"send these files back", and so on.

**Unreal Build Accelerator** is a tool that implements lightweight virtualization for operating environment of a
third party program (such as a C++ compiler), allowing it to run on a remote machine - requesting information from the
initiating machine in a just-in-time manner as necessary. The remotely executed process behaves as if it's executing
on the local machine, seeing the same view of the file system and so on, and files are transferred to and from the
remote machine behind the scenes as necessary.

## Prerequisites

* Install the [Horde server](InstallServer.md)
* One or more machines to function as build workers

## Steps

1. Install the Horde Server [as described here](InstallServer.md).
2. Download the `Horde Agent` installer from the tools page on the server. For Windows, it's easiest to use the MSI
   installer. Install the agent on a worker machine, entering the URL of the Horde Server when prompted.
3. Click on the `Agents` link from the `Server` menu and make sure the agent has registered with the server correctly.
  It should have automatically been added to the correct pool.
4. Configure UnrealBuildTool to use your remote worker.