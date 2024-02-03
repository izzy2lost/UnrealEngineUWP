[Horde](../../Home.md) > [Configuration](../../Config.md) > *.project.json

# *.project.json

Stores configuration for a project

Name | Type | Description
---- | ---- | -----------
`id` | `string` | The project id
`name` | `string` | Name for the new project
`path` | `string` | Direct include path for the project config. For backwards compatibility with old config files when including from a GlobalConfig object.
`include` | [`ConfigInclude`](#configinclude)`[]` | Includes for other configuration files
`macros` | [`ConfigMacro`](#configmacro)`[]` | Macros within the global scope
`order` | `integer` | Order of this project on the dashboard
`logo` | `string` | Path to the project logo
`pools` | [`PoolConfig`](#poolconfig)`[]` | List of pools for this project
`categories` | [`ProjectCategoryConfig`](#projectcategoryconfig)`[]` | Categories to include in this project
`jobOptions` | [`JobOptions`](#joboptions) | Default settings for executing jobs
`streams` | [`StreamConfig`](Streams.md)`[]` | List of streams
`acl` | [`AclConfig`](#aclconfig) | Acl entries

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

## ProjectCategoryConfig

Information about a category to display for a stream

Name | Type | Description
---- | ---- | -----------
`name` | `string` | Name of this category
`row` | `integer` | Index of the row to display this category on
`showOnNavMenu` | `boolean` | Whether to show this category on the nav menu
`includePatterns` | `string[]` | Patterns for stream names to include
`excludePatterns` | `string[]` | Patterns for stream names to exclude

## JobOptions

Options for executing a job

Name | Type | Description
---- | ---- | -----------
`executor` | `string` | Name of the executor to use
`useNewTempStorage` | `boolean` | Whether to use the new temp storage backend
`useWine` | `boolean` | Whether to execute using Wine emulation on Linux
`runInSeparateProcess` | `boolean` | Executes the job lease in a separate process
`workspaceMaterializer` | `string` | What workspace materializer to use in WorkspaceExecutor. Will override any value from workspace config.
`container` | [`JobContainerOptions`](#jobcontaineroptions) | Options for executing a job inside a container
`bundleVersion` | `integer` | Version to use when writing bundles

## JobContainerOptions

Options for executing a job inside a container

Name | Type | Description
---- | ---- | -----------
`enabled` | `boolean` | Whether to execute job inside a container
`imageUrl` | `string` | Image URL to container, such as "quay.io/podman/hello"
`containerEngineExecutable` | `string` | Container engine executable (docker or with full path like /usr/bin/podman)
`extraArguments` | `string` | Additional arguments to pass to container engine

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
