[Horde](../Home.md) > [Configuration](../Config.md) > Agents

# Agents

## Installing the Horde Agent

For information about deploying new agents, see [Horde > Deployment > Agent](../Deployment/Agent.md).

# Pools

Pools are a way to group agents with similar characteristics, allowing assignment of work to any machine 
in the group rather than needing to specify a machine individually. The set of pool names is defined in 
the [Globals.json](Schema/Globals.md#poolconfig) file.

Membership to a pool may be defined explicitly, through the agents page under the Server menu in the 
dashboard, or dynamically, because an agent matches the condition specified on the pool definition. 

## Remoting to Agents

If you have a fleet of machines which require identical login credentials, you can configure UnrealGameSync to open Remote Desktop sessions from links in the Horde dashboard.

To enable this functionality, open **Credential Manager** from the Windows Control Panel and select **Windows Credentials**. 
Click the **Add a new generic gredential...** link to create a new entry and name it `UnrealGameSync:RDP`. Enter the login username and password as appropriate.

The **Remote Desktop** button on agent dialogs in Horde will open a URL of the form `ugs://rdp?host=[NameOrIP]`. 
UnrealGameSync is configured to handle `ugs://` links by default, and intercepts these links and adds a Windows login entry for the given `NameOrIP` before launching the remote desktop application.

