[Horde](../Home.md) > [Deployment](../Deployment.md) > [Server](Server.md) > appsettings.json (Server)

# appsettings.json (Server)

All Horde-specific settings are stored in a root object called `horde`. Other .NET functionality may be configured using properties in the root of this file.

Name | Type | Description
---- | ---- | -----------
`runModes` | [`RunMode`](#runmode-enum)`[]` | Modes that the server should run in. Runmodes can be used in a multi-server deployment to limit the operations that a particular instance will try to perform.
`dataDir` | `string` | Override the data directory used by Horde. Defaults to C:\ProgramData\HordeServer on Windows, {AppDir}/Data on other platforms.
`installed` | `boolean` | Whether the server is running in 'installed' mode. In this mode, on Windows, the default data directory will use the common application data folder (C:\ProgramData\Epic\Horde), and configuration data will be read from here and the registry. This setting is overridden to false for local builds from appsettings.Local.json.
`httpPort` | `integer` | Main port for serving HTTP.
`httpsPort` | `integer` | Port for serving HTTP with TLS enabled. Disabled by default.
`http2Port` | `integer` | Dedicated port for serving only HTTP/2.
`computeTunnelPort` | `integer` | Port to listen on for tunneling compute sockets to agents
`computeTunnelAddress` | `string` | What address (host:port) clients should connect to for compute socket tunneling Port may differ from  if Horde server is behind a reverse proxy/firewall
`databaseConnectionString` | `string` | MongoDB connection string
`databaseName` | `string` | MongoDB database name
`databasePublicCert` | `string` | Optional certificate to trust in order to access the database (eg. AWS public cert for TLS)
`databaseReadOnlyMode` | `boolean` | Access the database in read-only mode (avoids creating indices or updating content) Useful for debugging a local instance of HordeServer against a production database.
`serverPrivateCert` | `string` | Optional PFX certificate to use for encrypting agent SSL traffic. This can be a self-signed certificate, as long as it's trusted by agents.
`authMethod` | [`AuthMethod`](#authmethod-enum) | Issuer for tokens from the auth provider
`oidcAudience` | `string` | Audience for OIDC validation
`oidcAuthority` | `string` | Issuer for tokens from the auth provider
`oidcClientId` | `string` | Client id for the OIDC authority
`oidcClientSecret` | `string` | Client secret for the OIDC authority
`oidcSigninRedirect` | `string` | Optional redirect url provided to OIDC login
`oidcLocalRedirectUrls` | `string[]` | Optional redirect url provided to OIDC login for external tools (typically to a local server)
`oidcRequestedScopes` | `string[]` | OpenID Connect scopes to request when signing in
`oidcClaimNameMapping` | `string[]` | List of fields in /userinfo endpoint to try map to the standard name claim (see System.Security.Claims.ClaimTypes.Name)
`oidcClaimEmailMapping` | `string[]` | List of fields in /userinfo endpoint to try map to the standard email claim (see System.Security.Claims.ClaimTypes.Email)
`oidcClaimHordeUserMapping` | `string[]` | List of fields in /userinfo endpoint to try map to the Horde user claim (see HordeClaimTypes.User)
`oidcClaimHordePerforceUserMapping` | `string[]` | List of fields in /userinfo endpoint to try map to the Horde Perforce user claim (see HordeClaimTypes.PerforceUser)
`jwtIssuer` | `string` | Name of the issuer in bearer tokens from the server
`jwtSecret` | `string` | Secret key used to sign JWTs. This setting is typically only used for development. In prod, a unique secret key will be generated and stored in the DB for each unique server instance.
`jwtExpiryTimeHours` | `integer` | Length of time before JWT tokens expire, in hours
`adminClaimType` | `string` | The claim type for administrators
`adminClaimValue` | `string` | Value of the claim type for administrators
`corsEnabled` | `boolean` | Whether to enable Cors, generally for development purposes
`corsOrigin` | `string` | Allowed Cors origin
`enableNewAgentsByDefault` | `boolean` | Whether to automatically enable new agents by default. If false, new agents must manually be enabled before they can take on work.
`testDataRetainMonths` | `integer` | The number of months to retain test data
`schedulePollingInterval` | `string` | Interval between rebuilding the schedule queue with a DB query.
`noResourceBackOffTime` | `string` | Interval between polling for new jobs
`initiateJobBackOffTime` | `string` | Interval between attempting to assign agents to take on jobs
`unknownErrorBackOffTime` | `string` | Interval between scheduling jobs when an unknown error occurs
`redisConnectionConfig` | `string` | Config for connecting to Redis server(s). Setting it to null will disable Redis use and connection See format at https://stackexchange.github.io/StackExchange.Redis/Configuration.html
`logServiceWriteCacheType` | `string` | Type of write cache to use in log service Currently Supported: "InMemory" or "Redis"
`logStorage` | [`StorageBackendOptions`](#storagebackendoptions) | Settings for artifact storage
`artifactStorage` | [`StorageBackendOptions`](#storagebackendoptions) | Settings for artifact storage
`commitStorage` | [`TreeStoreOptions`](#treestoreoptions) | Configuration of tree storage
`logJsonToStdOut` | `boolean` | Whether to log json to stdout
`logSessionRequests` | `boolean` | Whether to log requests to the UpdateSession and QueryServerState RPC endpoints
`enableLogService` | `boolean` | Whether to enable the hosted LogService running background jobs
`fleetManagerV2` | [`FleetManagerType`](#fleetmanagertype-enum) | Default fleet manager to use (when not specified by pool)
`fleetManagerV2Config` | `string` | Config for the fleet manager (serialized JSON)
`awsAutoScalingQueueUrls` | `string[]` | AWS SQS queue URLs where lifecycle events from EC2 auto-scaling are received
`disableSchedules` | `boolean` | Whether to run scheduled jobs.
`scheduleTimeZone` | `string` | Timezone for evaluating schedules
`slackToken` | `string` | Bot token for interacting with Slack (xoxb-*)
`slackSocketToken` | `string` | Token for opening a socket to slack (xapp-*)
`slackAdminToken` | `string` | Admin user token for Slack (xoxp-*). This is only required when using the admin endpoints to invite users.
`slackUsers` | `string` | Filtered list of slack users to send notifications to. Should be Slack user ids, separated by commas.
`slackErrorPrefix` | `string` | Prefix to use when reporting errors
`slackWarningPrefix` | `string` | Prefix to use when reporting warnings
`configNotificationChannel` | `string` | Channel for sending messages related to config update failures
`updateStreamsNotificationChannel` | `string` | Channel to send stream notification update failures to
`jobNotificationChannel` | `string` | Slack channel to send job related notifications to. Multiple channels can be specified, separated by ;
`agentNotificationChannel` | `string` | Slack channel to send agent related notifications to.
`dashboardUrl` | `string` | The URl to use for generating links back to the dashboard.
`helpEmailAddress` | `string` | Help email address that users can contact with issues
`helpSlackChannel` | `string` | Help slack channel that users can use for issues
`p4SwarmUrl` | `string` | Url of P4 Swarm installation
`jiraUsername` | `string` | The Jira service account user name
`jiraApiToken` | `string` | The Jira service account API token
`jiraUrl` | `string` | The Uri for the Jira installation
`sharedDeviceCheckoutDays` | `integer` | The number of days shared device checkouts are held
`deviceProblemCooldownMinutes` | `integer` | The number of cooldown minutes for device problems
`deviceReportChannel` | `string` | Channel to send device reports to
`defaultAgentPoolSizeStrategy` | [`PoolSizeStrategy`](#poolsizestrategy-enum) | Default agent pool sizing strategy for pools that doesn't have one explicitly configured
`agentPoolScaleOutCooldownSeconds` | `integer` | Scale-out cooldown for auto-scaling agent pools (in seconds). Can be overridden by per-pool settings.
`agentPoolScaleInCooldownSeconds` | `integer` | Scale-in cooldown for auto-scaling agent pools (in seconds). Can be overridden by per-pool settings.
`globalThreadPoolMinSize` | `integer` | Set the minimum size of the global thread pool This value has been found in need of tweaking to avoid timeouts with the Redis client during bursts of traffic. Default is 16 for .NET Core CLR. The correct value is dependent on the traffic the Horde Server is receiving. For Epic's internal deployment, this is set to 40.
`withDatadog` | `boolean` | Whether to enable Datadog integration for tracing
`withAws` | `boolean` | Whether to enable Amazon Web Services (AWS) specific features
`configPath` | `string` | Path to the root config file. Relative to the server.json file by default.
`perforce` | [`PerforceConnectionSettings`](#perforceconnectionsettings)`[]` | Perforce connections for use by the Horde server (not agents)
`useLocalPerforceEnv` | `boolean` | Whether to use the local Perforce environment
`perforceConnectionPoolSize` | `integer` | Number of pooled perforce connections to keep
`enableUpgradeTasks` | `boolean` | Whether to enable the upgrade task source.
`enableConformTasks` | `boolean` | Whether to enable the conform task source.
`forceConfigUpdateOnStartup` | `boolean` | Forces configuration data to be read and updated as part of appplication startup, rather than on a schedule. Useful when running locally.
`openBrowser` | `boolean` | Whether to open a browser on startup
`bundleCacheDir` | `string` | Directory to use for cache data
`bundleCacheSize` | `integer` | Maximum size of the storage cache on disk, in megabytes
`featureFlags` | [`FeatureFlagSettings`](#featureflagsettings) | Experimental features to enable on the server.
`commits` | [`CommitSettings`](#commitsettings) | Options for the commit service
`telemetry` | [`BaseTelemetryConfig`](#basetelemetryconfig)`[]` | Settings for sending telemetry events to external services (for example Snowflake, ClickHouse etc)
`bundledTools` | [`BundledToolConfig`](#bundledtoolconfig)`[]` | Tools bundled along with the server. Data for each tool can be produced using the 'bundle create' command, and should be stored in the /tools/{id} directory.
`openTelemetry` | [`OpenTelemetrySettings`](#opentelemetrysettings) | Options for OpenTelemetry

## RunMode (Enum)

Type of run mode this process should use. Each carry different types of workloads. More than one mode can be active. But not all modes are not guaranteed to be compatible with each other and will raise an error if combined in such a way.

Name | Description
---- | -----------
`None` | Default no-op value (ASP.NET config will default to this for enums that cannot be parsed)
`Server` | Handle and respond to incoming external requests, such as HTTP REST and gRPC calls. These requests are time-sensitive and short-lived, typically less than 5 secs. If processes handling requests are unavailable, it will be very visible for users.
`Worker` | Run non-request facing workloads. Such as background services, processing queues, running work based on timers etc. Short periods of downtime or high CPU usage due to bursts are fine for this mode. No user requests will be impacted directly. If auto-scaling is used, a much more aggressive policy can be applied (tighter process packing, higher avg CPU usage).

## AuthMethod (Enum)

Authentication method used for logging users in

Name | Description
---- | -----------
`Anonymous` | No authentication enabled. *Only* for demo and testing purposes.
`Okta` | OpenID Connect authentication, tailored for Okta
`OpenIdConnect` | Generic OpenID Connect authentication, recommended for most
`Horde` | Authenticate using username and password credentials stored in Horde OpenID Connect (OIDC) is first and foremost recommended. But if you have a small installation (less than ~10 users) or lacking an OIDC provider, this is an option.

## StorageBackendOptions

Common settings object for different providers

Name | Type | Description
---- | ---- | -----------
`type` | [`StorageBackendType`](#storagebackendtype-enum) | The type of storage backend to use
`baseDir` | `string` | Base directory for filesystem storage
`awsBucketName` | `string` | Name of the bucket to use
`awsBucketPath` | `string` | Base path within the bucket
`awsCredentials` | [`AwsCredentialsType`](#awscredentialstype-enum) | Type of credentials to use
`awsRole` | `string` | ARN of a role to assume
`awsProfile` | `string` | The AWS profile to read credentials form
`awsRegion` | `string` | Region to connect to
`azureConnectionString` | `string` | Connection string for Azure
`azureContainerName` | `string` | Name of the container

## StorageBackendType (Enum)

Types of storage backend to use

Name | Description
---- | -----------
`FileSystem` | Local filesystem
`Aws` | AWS S3
`Azure` | Azure blob store
`Memory` | In-memory only (for testing)

## AwsCredentialsType (Enum)

Credentials to use for AWS

Name | Description
---- | -----------
`Default` | Use default credentials from the AWS SDK
`Profile` | Read credentials from the  profile in the AWS config file
`AssumeRole` | Assume a particular role. Should specify ARN in
`AssumeRoleWebIdentity` | Assume a particular role using the current environment variables.

## TreeStoreOptions

Options for storing trees

Name | Type | Description
---- | ---- | -----------
`bundle` | [`BundleOptions`](#bundleoptions) | Options for creating bundles
`chunking` | [`ChunkingOptions`](#chunkingoptions) | Options for chunking content
`type` | [`StorageBackendType`](#storagebackendtype-enum) | The type of storage backend to use
`baseDir` | `string` | Base directory for filesystem storage
`awsBucketName` | `string` | Name of the bucket to use
`awsBucketPath` | `string` | Base path within the bucket
`awsCredentials` | [`AwsCredentialsType`](#awscredentialstype-enum) | Type of credentials to use
`awsRole` | `string` | ARN of a role to assume
`awsProfile` | `string` | The AWS profile to read credentials form
`awsRegion` | `string` | Region to connect to
`azureConnectionString` | `string` | Connection string for Azure
`azureContainerName` | `string` | Name of the container

## BundleOptions

Options for configuring a bundle serializer

Name | Type | Description
---- | ---- | -----------
`maxVersion` | [`BundleVersion`](#bundleversion-enum) | Maximum version number of bundles to write
`maxBlobSize` | `integer` | Maximum payload size fo a blob
`compressionFormat` | [`BundleCompressionFormat`](#bundlecompressionformat-enum) | Compression format to use
`minCompressionPacketSize` | `integer` | Minimum size of a block to be compressed
`maxWriteQueueLength` | `integer` | Maximum amount of data to store in memory. This includes any background writes as well as bundles being built.

## BundleVersion (Enum)

Bundle version number

Name | Description
---- | -----------
`Initial` | Initial version number
`ExportAliases` | Added the BundleExport.Alias property
`RemoveAliases` | Back out change to include aliases. Will likely do this through an API rather than baked into the data.
`InPlace` | Use data structures which support in-place reading and writing.
`ImportHashes` | Add import hashes to imported nodes
`LatestV1` | Last version using the V1 pipeline
`PacketSequence` | Structure bundles as a sequence of self-contained packets (uses V2 code)
`Latest` | The current version number
`LatestV2` | Last version using the V2 pipeline
`LatestPlusOne` | Last item in the enum. Used for

## BundleCompressionFormat (Enum)

Indicates the compression format in the bundle

Name | Description
---- | -----------
`None` | Packets are uncompressed
`LZ4` | LZ4 compression
`Gzip` | Gzip compression
`Oodle` | Oodle compression (Selkie)
`Brotli` | Brotli compression

## ChunkingOptions

Options for creating file nodes

Name | Type | Description
---- | ---- | -----------
`leafOptions` | [`LeafChunkedDataNodeOptions`](#leafchunkeddatanodeoptions) | Options for creating leaf nodes
`interiorOptions` | [`InteriorChunkedDataNodeOptions`](#interiorchunkeddatanodeoptions) | Options for creating interior nodes

## LeafChunkedDataNodeOptions

Options for creating a specific type of file nodes

Name | Type | Description
---- | ---- | -----------
`minSize` | `integer` | Minimum chunk size
`maxSize` | `integer` | Maximum chunk size. Chunks will be split on this boundary if another match is not found.
`targetSize` | `integer` | Target chunk size for content-slicing

## InteriorChunkedDataNodeOptions

Options for creating interior nodes

Name | Type | Description
---- | ---- | -----------
`minChildCount` | `integer` | Minimum number of children in each node
`targetChildCount` | `integer` | Target number of children in each node
`maxChildCount` | `integer` | Maximum number of children in each node
`sliceThreshold` | `integer` | Threshold hash value for splitting interior nodes

## FleetManagerType (Enum)

Available fleet managers

Name | Description
---- | -----------
`Default` | Default fleet manager
`NoOp` | No-op fleet manager.
`Aws` | Fleet manager for handling AWS EC2 instances. Will create and/or terminate instances from scratch.
`AwsReuse` | Fleet manager for handling AWS EC2 instances. Will start already existing but stopped instances to reuse existing EBS disks.
`AwsRecycle` | Fleet manager for handling AWS EC2 instances. Will start already existing but stopped instances to reuse existing EBS disks.
`AwsAsg` | Fleet manager for handling AWS EC2 instances. Uses an EC2 auto-scaling group for controlling the number of running instances.

## PoolSizeStrategy (Enum)

Available pool sizing strategies

Name | Description
---- | -----------
`LeaseUtilization` | Strategy based on lease utilization
`JobQueue` | Strategy based on size of job build queue
`NoOp` | No-op strategy used as fallback/default behavior
`ComputeQueueAwsMetric` | A no-op strategy that reports metrics to let an external AWS auto-scaling policy scale the fleet
`LeaseUtilizationAwsMetric` | A no-op strategy that reports metrics to let an external AWS auto-scaling policy scale the fleet

## PerforceConnectionSettings

Perforce connection information for use by the Horde server (for reading config files, etc...)

Name | Type | Description
---- | ---- | -----------
`id` | `string` | Identifier for this server
`serverAndPort` | `string` | Server and port
`credentials` | [`PerforceCredentials`](#perforcecredentials) | Credentials for the server

## PerforceCredentials

Credentials for a Perforce user

Name | Type | Description
---- | ---- | -----------
`userName` | `string` | The username
`password` | `string` | Password for the user
`ticket` | `string` | Login ticket for the user (will be used instead of password if set)

## FeatureFlagSettings

Feature flags to aid rollout of new features.
Once a feature is running in its intended state and is stable, the flag should be removed. A name and date of when the flag was created is noted next to it to help encourage this behavior. Try having them be just a flag, a boolean.


## CommitSettings

Options for the commit service

Name | Type | Description
---- | ---- | -----------
`replicateMetadata` | `boolean` | Whether to mirror commit metadata to the database
`replicateContent` | `boolean` | Whether to mirror commit data to storage
`bundle` | [`BundleOptions`](#bundleoptions) | Options for how objects are packed together
`chunking` | [`ChunkingOptions`](#chunkingoptions) | Options for how objects are sliced

## BaseTelemetryConfig

Configuration for the telemetry sink

Name | Type | Description
---- | ---- | -----------
`id` | `string` | Unique ID for this sink config (any arbitrary string)
`type` | [`TelemetrySinkType`](#telemetrysinktype-enum) | Type of telemetry sink

## TelemetrySinkType (Enum)

Type of telemetry provider to use

Name | Description
---- | -----------
`None` | No telemetry sink (default)
`Epic` | Use the Epic telemetry sink
`ClickHouse` | Use the ClickHouse telemetry sink
`Mongo` | Mongo telemetry

## BundledToolConfig

Configuration for a tool bundled alongsize the server

Name | Type | Description
---- | ---- | -----------
`version` | `string` | Version string for the current tool data
`refName` | `string` | Ref name in the tools directory
`dataDir` | `string` | Directory containing blob data for this tool. If empty, the tools/{id} folder next to the server will be used.
`id` | `string` | Unique identifier for the tool
`name` | `string` | Name of the tool
`description` | `string` | Description for the tool
`public` | `boolean` | Whether this tool should be exposed for download on a public endpoint without authentication
`showInUgs` | `boolean` | Whether to show this tool for download in the UGS tools menu
`namespaceId` | `string` | Default namespace for new deployments of this tool
`acl` | [`AclConfig`](#aclconfig) | Permissions for the tool

## AclConfig

Parameters to update an ACL

Name | Type | Description
---- | ---- | -----------
`entries` | [`AclEntryConfig`](#aclentryconfig)`[]` | Entries to replace the existing ACL
`inherit` | `boolean` | Whether to inherit permissions from the parent ACL
`exceptions` | `string[]` | List of exceptions to the inherited setting

## AclEntryConfig

Individual entry in an ACL

Name | Type | Description
---- | ---- | -----------
`claim` | [`AclClaimConfig`](#aclclaimconfig) | Name of the user or group
`actions` | `string[]` | Array of actions to allow

## AclClaimConfig

New claim to create

Name | Type | Description
---- | ---- | -----------
`type` | `string` | The claim type
`value` | `string` | The claim value

## OpenTelemetrySettings

OpenTelemetry configuration for collection and sending of traces and metrics.

Name | Type | Description
---- | ---- | -----------
`enabled` | `boolean` | Whether OpenTelemetry exporting is enabled
`serviceName` | `string` | Service name
`serviceNamespace` | `string` | Service namespace
`serviceVersion` | `string` | Service version
`enableDatadogCompatibility` | `boolean` | Whether to enrich and format telemetry to fit presentation in Datadog
`attributes` | `string` `->` `string` | Extra attributes to set
`enableConsoleExporter` | `boolean` | Whether to enable the console exporter (for debugging purposes)
`protocolExporters` | `string` `->` [`OpenTelemetryProtocolExporterSettings`](#opentelemetryprotocolexportersettings) | Protocol exporters (key is a unique and arbitrary name)

## OpenTelemetryProtocolExporterSettings

Configuration for an OpenTelemetry exporter

Name | Type | Description
---- | ---- | -----------
`endpoint` | `string` | Endpoint URL. Usually differs depending on protocol used.
`protocol` | `string` | Protocol for the exporter ('grpc' or 'httpprotobuf')
