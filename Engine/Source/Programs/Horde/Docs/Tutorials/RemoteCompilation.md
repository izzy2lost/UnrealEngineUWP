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

* Horde Server installation
* One or more machines to function as build workers

## Steps

1. On the build worker, open the Horde Server dashboard in a web browser. This will typically be
   `http://{{ SERVER_HOST_NAME }}:13340`. Note that the Horde Server is installed using http rather than https by
   default, so you may need to enter `http://` manually as part of the address.
2. Open the `Server` menu in the top right corner of the Horde Server dashboard, and select 'Tool Library'.
3. Download and run the `Horde Agent (Windows Installer)` tool, entering the same server address you used above
   when prompted.
4. Click on the `Agents` link from the `Server` menu and make sure the agent has registered with the server correctly.
  It should have automatically been added to the correct pool for its current platform.
5. Right-click on the agent and select enable. This will validate that you trust the agent, and will allow it to take on work. 
6. Configure UnrealBuildTool to use your remote worker.
