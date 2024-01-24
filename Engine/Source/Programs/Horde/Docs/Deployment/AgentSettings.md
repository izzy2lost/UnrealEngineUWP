[Horde](../Home.md) > [Deployment](../Deployment.md) > [Agent](Agent.md) > appsettings.json (Agent)

# appsettings.json (Agent)

All Horde-specific settings are stored in a root object called `horde`. Other .NET functionality may be configured using properties in the root of this file.

Name | Type | Description
---- | ---- | -----------
`serverProfiles` | [`ServerProfile`](#serverprofile)`[]` | Known servers to connect to
`server` | `string` | The default server, unless overridden from the command line
`name` | `string` | Name of agent to report as when connecting to server. By default, the computer's hostname will be used.
`ephemeral` | `boolean` | Whether agent should register as being ephemeral. Doing so will not persist any long-lived data on the server and once disconnected it's assumed to have been deleted permanently. Ideal for short-lived agents, such as spot instances on AWS EC2.
`executor` | `string` | The executor to use for jobs. Defaults to the Perforce executor.
`localExecutor` | [`LocalExecutorSettings`](#localexecutorsettings) | Settings for the local executor
`perforceExecutor` | [`PerforceExecutorSettings`](#perforceexecutorsettings) | Settings for the perforce executor
`workingDir` | `string` | Working directory
`shareMountingEnabled` | `boolean` | Whether to mount the specified list of network shares
`shares` | [`MountNetworkShare`](#mountnetworkshare)`[]` | List of network shares to mount
`processNamesToTerminate` | `string[]` | List of process names to terminate after a job
`processesToTerminate` | [`ProcessToTerminate`](#processtoterminate)`[]` | List of process names to terminate after a lease completes, but not after a job step
`wineExecutablePath` | `string` | Path to Wine executable. If null, execution under Wine is disabled
`containerEngineExecutablePath` | `string` | Path to container engine executable, such as /usr/bin/podman. If null, execution of compute workloads inside a container is disabled
`writeStepOutputToLogger` | `boolean` | Whether to write step output to the logging device
`enableAwsEc2Support` | `boolean` | Queries information about the current agent through the AWS EC2 interface
`useLocalStorageClient` | `boolean` | Option to use a local storage client rather than connecting through the server. Primarily for convenience when debugging / iterating locally.
`computePort` | `integer` | Incoming port for listening for compute work. Needs to be tied with a lease.
`enableTelemetry` | `boolean` | Whether to send telemetry back to Horde server
`telemetryReportInterval` | `integer` | How often to report telemetry events to server in milliseconds
`bundleCacheDir` | `string` | Directory to use for caching bundles
`bundleCacheSize` | `integer` | Maximum size of the bundle cache, in megabytes.
`properties` | `string` `->` `string` | Key/value properties in addition to those set internally by the agent

## ServerProfile

Information about a server to use

Name | Type | Description
---- | ---- | -----------
`name` | `string` | Name of this server profile
`environment` | `string` | Name of the environment (currently just used for tracing)
`url` | `string` | Url of the server
`token` | `string` | Bearer token to use to initiate the connection
`thumbprint` | `string` | Thumbprint of a certificate to trust. Allows using self-signed certs for the server.
`thumbprints` | `string[]` | Thumbprints of certificates to trust. Allows using self-signed certs for the server.

## LocalExecutorSettings

Settings for the local executor

Name | Type | Description
---- | ---- | -----------
`workspaceDir` | `string` | Path to the local workspace to use with the local executor
`runSteps` | `boolean` | Whether to actually execute steps, or just do job setup

## PerforceExecutorSettings

Settings for the perforce executor

Name | Type | Description
---- | ---- | -----------
`runConform` | `boolean` | Whether to run conform jobs

## MountNetworkShare

Describes a network share to mount

Name | Type | Description
---- | ---- | -----------
`mountPoint` | `string` | Where the share should be mounted on the local machine. Must be a drive letter for Windows.
`remotePath` | `string` | Path to the remote resource

## ProcessToTerminate

Specifies a process to terminate

Name | Type | Description
---- | ---- | -----------
`name` | `string` | Name of the process
`when` | [`TerminateCondition`](#terminatecondition-enum)`[]` | When to terminate this process

## TerminateCondition (Enum)

Flags for processes to terminate

Name | Description
---- | -----------
`None` | Not specified; terminate in all circumstances
`BeforeSession` | When a session starts
`BeforeConform` | Before running a conform
`BeforeBatch` | Before executing a batch
`AfterBatch` | Terminate at the end of a batch
`AfterStep` | After a step completes
