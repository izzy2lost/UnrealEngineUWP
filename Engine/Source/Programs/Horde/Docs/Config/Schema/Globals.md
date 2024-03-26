[Horde](../../../README.md) > [Configuration](../../Config.md) > Globals.json

# Globals.json

Global configuration

Name | Type | Description
---- | ---- | -----------
`version` | `integer` | Version number for the server. Values are indicated by the .
`include` | [`ConfigInclude`](#configinclude)`[]` | Other paths to include
`macros` | [`ConfigMacro`](#configmacro)`[]` | Macros within the global scope
`dashboard` | [`DashboardConfig`](#dashboardconfig) | Settings for the dashboard
`projects` | [`ProjectConfig`](Projects.md)`[]` | List of projects
`pools` | [`PoolConfig`](#poolconfig)`[]` | List of pools
`downtime` | [`ScheduledDowntime`](#scheduleddowntime)`[]` | List of scheduled downtime
`perforceClusters` | [`PerforceCluster`](#perforcecluster)`[]` | List of Perforce clusters
`software` | [`AgentSoftwareConfig`](#agentsoftwareconfig)`[]` | List of costs of a particular agent type
`rates` | [`AgentRateConfig`](#agentrateconfig)`[]` | List of costs of a particular agent type
`networks` | [`NetworkConfig`](#networkconfig)`[]` | List of networks
`compute` | [`ComputeClusterConfig`](#computeclusterconfig)`[]` | List of compute profiles
`secrets` | [`SecretConfig`](#secretconfig)`[]` | List of secrets
`devices` | [`DeviceConfig`](#deviceconfig) | Device configuration
`tools` | [`ToolConfig`](#toolconfig)`[]` | List of tools hosted by the server
`maxConformCount` | `integer` | Maximum number of conforms to run at once
`agentShutdownIfDisabledGracePeriod` | `string` | Time to wait before shutting down an agent that has been disabled Used if no value is set on the actual pool.
`storage` | [`StorageConfig`](#storageconfig) | Storage configuration
`artifactTypes` | [`ArtifactTypeConfig`](#artifacttypeconfig)`[]` | Configuration for different artifact types
`telemetryStores` | [`TelemetryStoreConfig`](#telemetrystoreconfig)`[]` | Metrics to aggregate on the Horde server
`parameters` | `object` | General parameters for other tools. Can be queried through the api/v1/parameters endpoint.
`acl` | [`AclConfig`](#aclconfig) | Access control list

## ConfigInclude

Directive to merge config data from another source

Name | Type | Description
---- | ---- | -----------
`path` | `string` | Path to the config data to be included. May be relative to the including file's location.

## ConfigMacro

Declares a config macro

Name | Type | Description
---- | ---- | -----------
`name` | `string` | Name of the macro property
`value` | `string` | Value for the macro property

## DashboardConfig

Configuration for dashboard features

Name | Type | Description
---- | ---- | -----------
`showLandingPage` | `boolean` | Navigate to the landing page by default
`showCI` | `boolean` | Enable CI functionality
`showAgents` | `boolean` | Whether to show functionality related to agents, pools, and utilization on the dashboard.
`showAgentRegistration` | `boolean` | Whether to show the agent registration page. When using registration tokens from elsewhere this is not needed.
`showPerforceServers` | `boolean` | Show the Perforce server option on the server menu
`showDeviceManager` | `boolean` | Show the device manager on the server menu
`showTests` | `boolean` | Show automated tests on the server menu
`agentCategories` | [`DashboardAgentCategoryConfig`](#dashboardagentcategoryconfig)`[]` | Configuration for different agent pages
`poolCategories` | [`DashboardPoolCategoryConfig`](#dashboardpoolcategoryconfig)`[]` | Configuration for different pool pages
`analytics` | [`TelemetryViewConfig`](#telemetryviewconfig)`[]` | Configuration for telemetry views
`include` | [`ConfigInclude`](#configinclude)`[]` | Includes for other configuration files
`macros` | [`ConfigMacro`](#configmacro)`[]` | Macros within this configuration

## DashboardAgentCategoryConfig

Configuration for a category of agents

Name | Type | Description
---- | ---- | -----------
`name` | `string` | Name of the category
`condition` | `string` | Condition string to be evaluated for this page

## DashboardPoolCategoryConfig

Configuration for a category of pools

Name | Type | Description
---- | ---- | -----------
`name` | `string` | Name of the category
`condition` | `string` | Condition string to be evaluated for this page

## TelemetryViewConfig

A telemetry view of related metrics, divided into categofies

Name | Type | Description
---- | ---- | -----------
`id` | `string` | Identifier for the view
`name` | `string` | The name of the view
`variables` | [`TelemetryVariableConfig`](#telemetryvariableconfig)`[]` | The variables used to filter the view data
`categories` | [`TelemetryCategoryConfig`](#telemetrycategoryconfig)`[]` | The categories contained within the view

## TelemetryVariableConfig

A telemetry view variable used for filtering the charting data

Name | Type | Description
---- | ---- | -----------
`name` | `string` | The name of the variable for display purposes
`group` | `string` | The associated data group attached to the variable
`defaults` | `string[]` | The default values to select

## TelemetryCategoryConfig

A chart categody, will be displayed on the dashbord under an associated pivot

Name | Type | Description
---- | ---- | -----------
`name` | `string` | The name of the category
`charts` | [`TelemetryChartConfig`](#telemetrychartconfig)`[]` | The charts contained within the category

## TelemetryChartConfig

Telemetry chart configuraton

Name | Type | Description
---- | ---- | -----------
`name` | `string` | The name of the chart, will be displayed on the dashboard
`display` | [`TelemetryMetricUnitType`](#telemetrymetricunittype-enum) | The unit to display
`graph` | [`TelemetryMetricGraphType`](#telemetrymetricgraphtype-enum) | The graph type
`metrics` | [`TelemetryChartMetricConfig`](#telemetrychartmetricconfig)`[]` | List of configured metrics
`min` | `integer` | The min unit value for clamping chart
`max` | `integer` | The max unit value for clamping chart

## TelemetryMetricUnitType (Enum)

The units used to present the telemetry

Name | Description
---- | -----------
`Time` | Time duration
`Ratio` | Ratio 0-100%
`Value` | Artbitrary numeric value

## TelemetryMetricGraphType (Enum)

The type of

Name | Description
---- | -----------
`Line` | A line graph
`Indicator` | Key performance indicator (KPI) chart with thrasholds

## TelemetryChartMetricConfig

Metric attached to a telemetry chart

Name | Type | Description
---- | ---- | -----------
`id` | `string` | Associated metric id
`threshold` | `integer` | The threshold for KPI values
`alias` | `string` | The metric alias for display purposes

## PoolConfig

Mutable configuration for a pool

Name | Type | Description
---- | ---- | -----------
`id` | `string` | Unique id for this pool
`base` | `string` | Base pool config to copy settings from
`name` | `string` | Name of the pool
`condition` | `string` | Condition for agents to automatically be included in this pool
`properties` | `string` `->` `string` | Arbitrary properties related to this pool
`color` | [`PoolColor`](#poolcolor-enum) | Color to use for this pool on the dashboard
`enableAutoscaling` | `boolean` | Whether to enable autoscaling for this pool
`minAgents` | `integer` | The minimum number of agents to keep in the pool
`numReserveAgents` | `integer` | The minimum number of idle agents to hold in reserve
`conformInterval` | `string` | Interval between conforms. If zero, the pool will not conform on a schedule.
`scaleOutCooldown` | `string` | Cooldown time between scale-out events
`scaleInCooldown` | `string` | Cooldown time between scale-in events
`shutdownIfDisabledGracePeriod` | `string` | Time to wait before shutting down an agent that has been disabled
`sizeStrategy` | [`PoolSizeStrategy`](#poolsizestrategy-enum) | 
`sizeStrategies` | [`PoolSizeStrategyInfo`](#poolsizestrategyinfo)`[]` | List of pool sizing strategies for this pool. The first strategy with a matching condition will be picked.
`fleetManagers` | [`FleetManagerInfo`](#fleetmanagerinfo)`[]` | List of fleet managers for this pool. The first strategy with a matching condition will be picked. If empty or no conditions match, a default fleet manager will be used.
`leaseUtilizationSettings` | [`LeaseUtilizationSettings`](#leaseutilizationsettings) | Settings for lease utilization pool sizing strategy (if used)
`jobQueueSettings` | [`JobQueueSettings`](#jobqueuesettings) | Settings for job queue pool sizing strategy (if used)
`computeQueueAwsMetricSettings` | [`ComputeQueueAwsMetricSettings`](#computequeueawsmetricsettings) | Settings for job queue pool sizing strategy (if used)

## PoolColor (Enum)

Color to use for labels of this pool

Name | Description
---- | -----------
`Default` | 
`Blue` | 
`Orange` | 
`Green` | 
`Gray` | 

## PoolSizeStrategy (Enum)

Available pool sizing strategies

Name | Description
---- | -----------
`LeaseUtilization` | Strategy based on lease utilization
`JobQueue` | Strategy based on size of job build queue
`NoOp` | No-op strategy used as fallback/default behavior
`ComputeQueueAwsMetric` | A no-op strategy that reports metrics to let an external AWS auto-scaling policy scale the fleet
`LeaseUtilizationAwsMetric` | A no-op strategy that reports metrics to let an external AWS auto-scaling policy scale the fleet

## PoolSizeStrategyInfo

Metadata for configuring and picking a pool sizing strategy

Name | Type | Description
---- | ---- | -----------
`type` | [`PoolSizeStrategy`](#poolsizestrategy-enum) | Strategy implementation to use
`condition` | `string` | Condition if this strategy should be enabled (right now, using date/time as a distinguishing factor)
`config` | `string` | Configuration for the strategy, serialized as JSON
`extraAgentCount` | `integer` | Integer to add after pool size has been calculated. Can also be negative.

## FleetManagerInfo

Metadata for configuring and picking a fleet manager

Name | Type | Description
---- | ---- | -----------
`type` | [`FleetManagerType`](#fleetmanagertype-enum) | Fleet manager type implementation to use
`condition` | `string` | Condition if this strategy should be enabled (right now, using date/time as a distinguishing factor)
`config` | `string` | Configuration for the strategy, serialized as JSON

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

## LeaseUtilizationSettings

Lease utilization sizing settings for a pool

Name | Type | Description
---- | ---- | -----------
`sampleTimeSec` | `integer` | Time period for each sample
`numSamples` | `integer` | Number of samples to collect for calculating lease utilization
`numSamplesForResult` | `integer` | Min number of samples for a valid result
`minAgents` | `integer` | The minimum number of agents to keep in the pool
`numReserveAgents` | `integer` | The minimum number of idle agents to hold in reserve

## JobQueueSettings

Job queue sizing settings for a pool

Name | Type | Description
---- | ---- | -----------
`scaleOutFactor` | `number` | Factor translating queue size to additional agents to grow the pool with The result is always rounded up to nearest integer. Example: if there are 20 jobs in queue, a factor 0.25 will result in 5 new agents being added (20 * 0.25)
`scaleInFactor` | `number` | Factor by which to shrink the pool size with when queue is empty The result is always rounded up to nearest integer. Example: when the queue size is zero, a default value of 0.9 will shrink the pool by 10% (current agent count * 0.9)
`samplePeriodMin` | `integer` | How far back in time to look for job batches (that potentially are in the queue)
`readyTimeThresholdSec` | `integer` | Time spent in ready state before considered truly waiting for an agent<br>A job batch can be in ready state before getting picked up and executed. This threshold will help ensure only batches that have been waiting longer than this value will be considered.

## ComputeQueueAwsMetricSettings

Settings for

Name | Type | Description
---- | ---- | -----------
`computeClusterId` | `string` | Compute cluster ID to observe
`namespace` | `string` | AWS CloudWatch namespace to write metrics in

## ScheduledDowntime

Settings for the maintenance window

Name | Type | Description
---- | ---- | -----------
`startTime` | `string` | Start time
`finishTime` | `string` | Finish time
`frequency` | [`ScheduledDowntimeFrequency`](#scheduleddowntimefrequency-enum) | Frequency that the window repeats

## ScheduledDowntimeFrequency (Enum)

How frequently the maintence window repeats

Name | Description
---- | -----------
`Once` | Once
`Daily` | Every day
`Weekly` | Every week

## PerforceCluster

Information about a cluster of Perforce servers.

Name | Type | Description
---- | ---- | -----------
`name` | `string` | Name of the cluster
`serviceAccount` | `string` | Username for Horde to log in to this server. Will use the default user if not set.
`canImpersonate` | `boolean` | Whether the service account can impersonate other users
`supportsPartitionedWorkspaces` | `boolean` | Whether to use partitioned workspaces on this server
`servers` | [`PerforceServer`](#perforceserver)`[]` | List of servers
`credentials` | [`PerforceCredentials`](#perforcecredentials)`[]` | List of server credentials
`autoSdk` | [`AutoSdkWorkspace`](#autosdkworkspace)`[]` | List of autosdk streams

## PerforceServer

Information about an individual Perforce server

Name | Type | Description
---- | ---- | -----------
`serverAndPort` | `string` | The server and port. The server may be a DNS entry with multiple records, in which case it will be actively load balanced.
`healthCheck` | `boolean` | Whether to query the healthcheck address under each server
`resolveDns` | `boolean` | Whether to resolve the DNS entries and load balance between different hosts
`maxConformCount` | `integer` | Maximum number of simultaneous conforms on this server
`condition` | `string` | Optional condition for a machine to be eligable to use this server
`properties` | `string[]` | List of properties for an agent to be eligable to use this server

## PerforceCredentials

Credentials for a Perforce user

Name | Type | Description
---- | ---- | -----------
`userName` | `string` | The username
`password` | `string` | Password for the user
`ticket` | `string` | Login ticket for the user (will be used instead of password if set)

## AutoSdkWorkspace

Path to a platform and stream to use for syncing AutoSDK

Name | Type | Description
---- | ---- | -----------
`name` | `string` | Name of this workspace
`properties` | `string[]` | The agent properties to check (eg. "OSFamily=Windows")
`userName` | `string` | Username for logging in to the server
`stream` | `string` | Stream to use

## AgentSoftwareConfig

Selects different agent software versions by evaluating a condition

Name | Type | Description
---- | ---- | -----------
`toolId` | `string` | Tool identifier
`condition` | `string` | Condition for using this channel

## AgentRateConfig

Describes the monetary cost of agents matching a particular criteria

Name | Type | Description
---- | ---- | -----------
`condition` | `string` | Condition string
`rate` | `number` | Rate for this agent

## NetworkConfig

Describes a network The ID describes any logical grouping, such as region, availability zone, rack or office location.

Name | Type | Description
---- | ---- | -----------
`id` | `string` | ID for this network
`cidrBlock` | `string` | CIDR block
`description` | `string` | Human-readable description
`computeId` | `string` | Compute ID for this network (used when allocating compute resources)

## ComputeClusterConfig

Profile for executing compute requests

Name | Type | Description
---- | ---- | -----------
`id` | `string` | Name of the partition
`namespaceId` | `string` | Name of the namespace to use
`requestBucketId` | `string` | Name of the input bucket
`responseBucketId` | `string` | Name of the output bucket
`condition` | `string` | Filter for agents to include
`acl` | [`AclConfig`](#aclconfig) | Access control list

## AclConfig

Parameters to update an ACL

Name | Type | Description
---- | ---- | -----------
`entries` | [`AclEntryConfig`](#aclentryconfig)`[]` | Entries to replace the existing ACL
`profiles` | [`AclProfileConfig`](#aclprofileconfig)`[]` | Defines profiles which allow grouping sets of actions into named collections
`inherit` | `boolean` | Whether to inherit permissions from the parent ACL
`exceptions` | `string[]` | List of exceptions to the inherited setting

## AclEntryConfig

Individual entry in an ACL

Name | Type | Description
---- | ---- | -----------
`claim` | [`AclClaimConfig`](#aclclaimconfig) | Name of the user or group
`actions` | `string[]` | Array of actions to allow
`profiles` | `string[]` | List of profiles to grant

## AclClaimConfig

New claim to create

Name | Type | Description
---- | ---- | -----------
`type` | `string` | The claim type
`value` | `string` | The claim value

## AclProfileConfig

Configuration for an ACL profile. This defines a preset group of actions which can be given to a user via an ACL entry.

Name | Type | Description
---- | ---- | -----------
`id` | `string` | Identifier for this profile
`actions` | `string[]` | Actions to include
`excludeActions` | `string[]` | Actions to exclude from the inherited actions
`extends` | `string[]` | Other profiles to extend from

## SecretConfig

Configuration for a secret value

Name | Type | Description
---- | ---- | -----------
`id` | `string` | Identifier for this secret
`data` | `string` `->` `string` | Key/value pairs associated with this secret
`sources` | [`ExternalSecretConfig`](#externalsecretconfig)`[]` | Providers to source key/value pairs from
`acl` | [`AclConfig`](#aclconfig) | Defines access to this particular secret

## ExternalSecretConfig

Configuration for an external secret provider

Name | Type | Description
---- | ---- | -----------
`provider` | `string` | Name of the provider to use
`key` | `string` | Optional key indicating the parameter to set in the resulting data array
`path` | `string` | Optional value indicating what to fetch from the provider
`arguments` | `string` `->` `string` | Additional provider-specific arguments

## DeviceConfig

Configuration for devices

Name | Type | Description
---- | ---- | -----------
`platforms` | [`DevicePlatformConfig`](#deviceplatformconfig)`[]` | List of device platforms
`pools` | [`DevicePoolConfig`](#devicepoolconfig)`[]` | List of device pools

## DevicePlatformConfig

Configuration for a device platform

Name | Type | Description
---- | ---- | -----------
`id` | `string` | The id for this platform
`name` | `string` | Name of the platform
`models` | `string[]` | A list of platform models
`legacyNames` | `string[]` | Legacy names which older versions of Gauntlet may be using
`legacyPerfSpecHighModel` | `string` | Model name for the high perf spec, which may be requested by Gauntlet

## DevicePoolConfig

Configuration for a device pool

Name | Type | Description
---- | ---- | -----------
`id` | `string` | The id for this platform
`name` | `string` | The name of the pool
`poolType` | [`DevicePoolType`](#devicepooltype-enum) | The type of the pool
`projectIds` | `string[]` | List of project ids associated with pool

## DevicePoolType (Enum)

The type of device pool

Name | Description
---- | -----------
`Automation` | Available to CIS jobs
`Shared` | Shared by users with remote checking and checkouts

## ToolConfig

Options for configuring a tool

Name | Type | Description
---- | ---- | -----------
`id` | `string` | Unique identifier for the tool
`name` | `string` | Name of the tool
`description` | `string` | Description for the tool
`parentId` | `string` | Tool id to nest this tool under
`public` | `boolean` | Whether this tool should be exposed for download on a public endpoint without authentication
`showInUgs` | `boolean` | Whether to show this tool for download in the UGS tools menu
`showInDashboard` | `boolean` | Whether to show this tool for download in the dashboard
`namespaceId` | `string` | Default namespace for new deployments of this tool
`acl` | [`AclConfig`](#aclconfig) | Permissions for the tool

## StorageConfig

Configuration for storage

Name | Type | Description
---- | ---- | -----------
`backends` | [`BackendConfig`](#backendconfig)`[]` | List of storage backends
`namespaces` | [`NamespaceConfig`](#namespaceconfig)`[]` | List of namespaces for storage

## BackendConfig

Common settings object for different providers

Name | Type | Description
---- | ---- | -----------
`id` | `string` | The storage backend ID
`base` | `string` | Base backend to copy default settings from
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
`relayServer` | `string` | 
`relayToken` | `string` | 

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

## NamespaceConfig

Configuration of a particular namespace

Name | Type | Description
---- | ---- | -----------
`id` | `string` | Identifier for this namespace
`backend` | `string` | Backend to use for this namespace
`prefix` | `string` | Prefix for items within this namespace
`gcFrequencyHrs` | `number` | How frequently to run garbage collection, in hours.
`gcDelayHrs` | `number` | How long to keep newly uploaded orphanned objects before allowing them to be deleted, in hours.
`enableAliases` | `boolean` | Support querying exports by their aliases
`acl` | [`AclConfig`](#aclconfig) | Access list for this namespace

## ArtifactTypeConfig

Configuration for an artifact

Name | Type | Description
---- | ---- | -----------
`type` | `string` | Name of the artifact type
`name` | `string` | Legacy 'Name' property
`keepCount` | `integer` | Number of artifacts to retain
`keepDays` | `integer` | Number of days to retain artifacts of this type

## TelemetryStoreConfig

Config for metrics

Name | Type | Description
---- | ---- | -----------
`id` | `string` | Identifier for this store
`acl` | [`AclConfig`](#aclconfig) | Permissions for this store
`metrics` | [`MetricConfig`](#metricconfig)`[]` | Metrics to aggregate on the Horde server
`include` | [`ConfigInclude`](#configinclude)`[]` | Includes for other configuration files
`macros` | [`ConfigMacro`](#configmacro)`[]` | Macros within this configuration

## MetricConfig

Configures a metric to aggregate on the server

Name | Type | Description
---- | ---- | -----------
`id` | `string` | Identifier for this metric
`filter` | `string` | Filter expression to evaluate to determine which events to include. This query is evaluated against an array.
`property` | `string` | Property to aggregate
`groupBy` | `string` | Property to group by. Specified as a comma-separated list of JSON path expressions.
`function` | [`AggregationFunction`](#aggregationfunction-enum) | How to aggregate samples for this metric
`percentile` | `integer` | For the percentile function, specifies the percentile to measure
`interval` | `string` | Interval for each metric. Supports times such as "2d", "1h", "1h30m", "20s".

## AggregationFunction (Enum)

Method for aggregating samples into a metric

Name | Description
---- | -----------
`Count` | Count the number of matching elements
`Min` | Take the minimum value of all samples
`Max` | Take the maximum value of all samples
`Sum` | Sum all the reported values
`Average` | Average all the samples
`Percentile` | Estimates the value at a certain percentile
