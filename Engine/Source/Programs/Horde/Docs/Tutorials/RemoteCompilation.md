[Horde](../Home.md) > Getting Started: Remote Compilation

# Getting Started: Remote Compilation

## Introduction

Horde implements a platform for generic remote execution workloads, allowing clients to leverage idle CPU cycles on
other machines to accelerate workloads that would otherwise be executed locally. Horde's remote execution platform
allows issuing explicit commands to remote agents sequentially, such as "upload these files", "run this process",
"send these files back", and so on.

**Unreal Build Accelerator** is a tool that implements lightweight virtualization for
third party programs (such as C++ compilers), allowing it to run on a remote machine - requesting information from the
initiating machine as it's required. The remotely executed process behaves as if it's executing
on the local machine, seeing the same view of the file system and so on, and files are transferred to and from the
remote machine behind the scenes as necessary.

Unreal Build Tool can use Unreal Build Accelerator together with Horde to offload build tasks to connected agents,
spreading the workload over multiple machines.

## Prerequisites

* A machine to function as the Horde Server.
* One or more machines to function as Horde Agents. We currently recommend dedicated machines for this purpose.
* A workstation with a UE project under development.
* Network connectivity between your workstation and Horde Agents on port range 7000-7010.

## Steps

### Horde Server

1. Install the Horde Server by running `Engine\Extras\Horde\UnrealHordeServer.msi` on Windows.
   * The Horde Server can also be deployed on [Linux using Docker](../Deployment/Server.md#docker-images-linux).
   * By default, Horde is configured to use [ports 13340 (HTTP) and 13342 (HTTP/2)](../Deployment/Server.md#ports). 
     We recommend setting up [HTTPS](../Deployment/Server.md#https) for production deployments.
   * See also: [Deployment > Server](../Deployment/Server.md)

### Horde Agents

1. Navigate to the installed [Horde Server](#horde-server) in a web browser on the agent machine.
   * This is typically `http://{{ HOST_NAME_OR_IP_ADDRESS }}:13340` with a default installation.
   * Note that the Horde Server defaults to HTTP hosting by default (not HTTPS), so you may need
     to enter `http://` manually as part of the address.
2. Open the **Tools** menu at the top of the dashboard, and select **Downloads**.
3. Download and run **Horde Agent (Windows Installer)**.
   * Enter the same server address you used above when prompted, and choose an empty working directory for
     the remote execution sandbox.
   * We recommend choosing a drive with at least 100gb of free space for C++ compilation.
4. Leave the `Enroll with Server` option checked at the end of the installation, or locate the Unreal icon
   in the system notification area, right click on it, and select `Enroll with Server`.
5. Choose your agent from the list, and select **Enroll**. This process will validate that you trust the agent, 
   and will allow it to take on work.

### Workstation

1. On the machine initiating the build, ensure your UE project is synced and builds locally.
2. Configure UnrealBuildTool to use your Horde Server by updating
   `Engine/Saved/UnrealBuildTool/BuildConfiguration.xml` with the following:

   ```xml
   <?xml version="1.0" encoding="utf-8" ?>
   <Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">

       <BuildConfiguration>
           <!-- Enable support for UnrealBuildAccelerator -->
           <bAllowUBAExecutor>true</bAllowUBAExecutor>
       </BuildConfiguration>

       <Horde>
           <!-- Address of the Horde server -->
           <Server>http://{{ SERVER_HOST_NAME }}:13340</Server>

           <!-- Pool of machines to offload work to. Horde configures Win-UE5 by default. -->
           <WindowsPool>Win-UE5</WindowsPool>
       </Horde>

       <UnrealBuildAccelerator>
           <!-- Enable for visualizing UBA's progress (optional) -->
           <bLaunchVisualizer>true</bLaunchVisualizer>
       </UnrealBuildAccelerator>

   </Configuration>
   ```

   Replace `SERVER_HOST_NAME` with the appropriate address of your Horde server installation.

   * `BuildConfiguration.xml` can be sourced from many locations in the filesystem depending your preference
     including locations typically under source control. See 
     [Build Configuration](https://docs.unrealengine.com/5.3/en-US/build-configuration-for-unreal-engine/) 
     in the UnrealBuildTool documentation for more details.

3. Compile your project through your IDE as normal. You should observe log lines such as:

   ```text
   [Worker0] Connected to AGENT-1 (10.0.10.172) under lease 65d48fe1eb6ff84c8197a9b0
   ...
   [17/5759] Compile [x64] Module.CoreUObject.2.cpp [RemoteExecutor: AGENT-1]
   ```

   This indicates work is being spread to multiple agents. If you enabled the UBA visualizer, you can also see
   a graphical overview how the build progresses over multiple machines.

   For debugging and tuning purposes, it can be useful to force remote execution all compile workfloads. To do
   so, enable the following option in your `BuildConfiguration.xml` file or pass `-UBAForceRemote` on the 
   UnrealBuildTool command line:

   ```xml
   <UnrealBuildAccelerator>
       <bForceBuildAllRemote>true</bForceBuildAllRemote>
   </UnrealBuildAccelerator>
   ```
