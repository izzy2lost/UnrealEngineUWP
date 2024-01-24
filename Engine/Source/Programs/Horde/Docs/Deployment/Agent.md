[Horde](../Home.md) > [Deployment](../Deployment.md) > Agents

# Agents

## Installation

### MSI Installer (Windows)

A standalone agent can be installed from the same MSI as the server ([TODO](http)). 

Installing the agent in this manner also installs a background application that shows the agent status in the Windows notification area, and allows configuring the agent to only run when the machine is idle.

Note that agents installed in this manner will need to be upgraded manually for new Horde versions.

### Download from Server

The Horde server can be used as a download source for the Horde agent. Distributing the agent in this way allows the agent to upgrade automatically to new versions added to the server.

To install from a browser, go to the Horde dashboard and navigate to the the `Server > Agents` menu item, then click on the `Download Agent` link.

Alternatively, you can download the agent via the command line using the following commands. The `AUTH-TOKEN` parameter referenced here can be obtained by having an admin user log into the `http://[HORDE-SERVER-URL]/account` page and clicking on the **Get agent software download token** link. 

#### Windows (PowerShell)

    Invoke-WebRequest -Uri https://[HORDE-SERVER-URL]/api/v1/agentsoftware/default/zip -OutFile C:\Horde\HordeAgent.zip -Headers @{ 'Authorization' = 'Bearer [AUTH-TOKEN]' }
    Expand-Archive -LiteralPath C:\Horde\HordeAgent.zip -DestinationPath C:\Horde -Force

The -Headers parameter and value are not required when using an unauthenticated server.

#### Mac & Linux

    curl https://[HORDE-SERVER-URL]/api/v1/agentsoftware/default/zip --output ~/Horde/HordeAgent.zip -H "Authorization: Bearer [AUTH-TOKEN]"
    unzip -o ~/Horde/HordeAgent.zip -d ~/Horde/

The -H parameter and value are not required when using an unauthenticated server.

## Setup

### General

Agent settings are configured through the [`appsettings.json`](AgentSettings.md) file in the server directory. All Horde-specific settings are stored under the `horde` top-level key, with middleware and standard .NET settings under other root keys.

### Server Profiles

The agent's [`appsettings.json`](AgentSettings.md) file can contain settings for connecting to multiple Horde servers through the `ServerProfiles` property. Setting up multiple profiles can be useful when running Horde in multiple environments (eg. dev vs production), and each [server profile](AgentSettings.md#serverprofile) contains a name, server URL and authentication token.

Server profiles are referenced by name. The default profile is configured through the `Server` property, or via the `-Server=..` command line argument when launching the agent.

When scripting agent deployment, you can either modify the build hosted by the server to include the desired configuration by default, or use the agent's `SetServer` command to modify the configuration file after downloading it. This command can be invoked as:

    dotnet HordeAgent.dll SetServer -Name=.. -Url=.. -Token=...

Adding the `-Default` argument will configure this server to be used by default. Run with the `-Help` argument for a full list of available options.

### Registration

Navigating to the `http://[HORDE-SERVER-URL]/account` page with an admin user logged in will include a **Get agent registration token** link. This token can be embedded into the default agent config file, or passed to the `SetServer` command (see above).

The first time that an agent connects to the server, it will generate a unique connection token for itself. 

On Windows, connection tokens are stored in:

    C:\Users\[User]\AppData\Local\Horde.Agent\servers.json

On Mac/Linux, connection tokens are stored in:

    ~/.local/share/Horde.Agent/servers.json

### Running as a service

#### Windows

Running the MSI installer will configure the Horde Agent to run as a background service by default. When downloading the agent directly from the server and configuring it manually, a service can be registered by running the following command:

    dotnet HordeAgent.dll service install [-UserName=..] [-Password=..]

Where -UserName and -Password specify credentials for the account to run the service under. 

The service may be uninstalled using the following command:

    dotnet HordeAgent.dll service uninstall

#### Mac

Create a `/Library/LaunchAgents/epic.hordeagent.plist` file describing the daemon configuration (substituting the `{{ horde_service_account }}` variables as appropriate).

	<?xml version="1.0" encoding="UTF-8"?>
	<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
	<plist version="1.0">
	<dict>
		<key>RunAtLoad</key>
		<true/>

		<key>ProgramArguments</key>
		<array>
			<string>/usr/local/bin/dotnet</string>
			<string>HordeAgent.dll</string>
			<string>service</string>
			<string>run</string>
			<string>-server=Prod</string>
			<string>-workingdir=/Users/{{ horde_service_account }}/Build</string>
		</array>

		<key>EnvironmentVariables</key>
		<dict>
			<key>PATH</key>
			<string>/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin</string>
		</dict>

		<key>GroupName</key>
		<string>admin</string>

		<key>WorkingDirectory</key>
		<string>/Users/{{ horde_service_account }}/Horde</string>

		<key>UserName</key>
		<string>{{ horde_service_account }}</string>

		<key>KeepAlive</key>
		<true/>

		<key>Label</key>
		<string>epic.hordeagent</string>

		<key>StandardErrorPath</key>
		<string>/Users/{{ horde_service_account }}/Library/Logs/hordeagent_error.log</string>

		<key>ExitTimeOut</key>
		<integer>10</integer>
	</dict>
	</plist>

Adjust `/etc/newsyslog.conf` Log file out size limit (optional):

	append '/Library/Logs/hordeagent_error.log 700 2 1000 * J'

Set any Horde agent environment variables you want defined outside of the plist (optional):

	launchctl setenv Horde:WorkingDirectory {horde_working_directory}

Launch the daemon:

	launchctl load -w /Library/LaunchAgents/epic.hordeagent.plist

#### Linux

Create a user to run the agent service. The Unreal Editor cannot be run as root on Linux, so the horde-agent service needs to run as a non-root user. The working directory for the agent needs to be recursively owned by that user. The user must have `sudo` access in order to restart/shutdown/autoscale Horde agents.

Create a service descriptor file in `/etc/systemd/system/horde-agent.service` (substitute the `{{ horde_path }}`, `{{ horde_working_directory }}` and `{{ horde_service_account }}` variables as appropriate):

	[Unit]
	Description=Horde Agent

	[Service]
	ExecStart=dotnet {{ horde_path }} {{ horde_working_directory }}
	WorkingDirectory={{ horde_working_directory }}

	Restart=always
	RestartSec=5
	SyslogIdentifier=horde-agent

	StandardOutput=append:{{ horde_working_directory }}/log.txt
	StandardError=append:{{ horde_working_directory }}/err-log.txt

	User={{ horde_service_account }}

	[Install]
	WantedBy=multi-user.target

Launch the daemon:

	systemctl daemon-reload

### Working Directory

The default location for data used by the agent (Perforce workspaces, caches, scratch space) is `C:\ProgramData\HordeAgent` on Windows and the application directory on Mac/Linux.

This path can be overriden using the `WorkingDir` property in the agent's [`appsettings.json`](AgentSettings.md) file.

In order to prevent agents being affected by runaway jobs filling up the disk with data, it's a good idea to have the agent store data on a drive other than the system disk. Setting the `%TEMP%` and `%TMP%` environment variables to this drive on Windows is also recommended.

### Mounting Network Shares

The agent can be configured to mount certain network shares at startup before taking on any work using the `Shares` property in the agent's [`appsettings.json`](AgentSettings.md) file.
