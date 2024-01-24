[Horde](../Home.md) > [Configuration](../Config.md) > UnrealGameSync Metadata Server

# UnrealGameSync Metadata Server

UnrealGameSync (**UGS**) is a tool designed to simplify syncing from Perforce, supporting retrieval of pre-built editor binaries for artists or correctly versioning the local build to allow modifying content for engineers. It acts as a convienient hub for surfacing build health, flagging issues, and scripting common workflow tasks outside the Unreal Editor.

For more information on UGS, see [the UE docs site](https://docs.unrealengine.com/5.0/en-US/unreal-game-sync-ugs-for-unreal-engine/).

Horde includes an updated version of the legacy _MetadataServer_ IIS web-app that ships alongside UGS, which integrates seamlessly with Horde's CI functionality.

## Configuration

To configure UnrealGameSync to source data from Horde, add the following lines in the `UnrealGameSync.ini` config file:

	[Default]
	ApiUrl=https://{{ horde_server_url }}/ugs

This config file can be in a project-specific location (eg. `{{ project_dir }}/Build/UnrealGameSync.ini`) or in a location that applies to all projects in a stream (eg. `{{ engine_dir }}/Programs/UnrealGameSync/UnrealGameSync.ini`).