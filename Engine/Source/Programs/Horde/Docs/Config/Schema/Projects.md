[Horde](../../../README.md) > [Configuration](../../Config.md) > *.project.json

# *.project.json

Name | Description
---- | -----------
`id` | `string`<br>
`name` | `string`<br>
`path` | `string`<br>
`include` | [`ConfigInclude`](#configinclude)`[]`<br>
`macros` | [`ConfigMacro`](#configmacro)`[]`<br>
`order` | `integer`<br>
`logo` | `string`<br>
`logoDarkTheme` | `string`<br>
`pools` | [`PoolConfig`](#poolconfig)`[]`<br>
`categories` | [`ProjectCategoryConfig`](#projectcategoryconfig)`[]`<br>
`jobOptions` | [`JobOptions`](#joboptions)<br>
`workspaceTypes` | `string` `->` [`WorkspaceConfig`](#workspaceconfig)<br>
`telemetryStoreId` | `string`<br>
`streams` | [`StreamConfig`](Streams.md)`[]`<br>
`artifactTypes` | [`ArtifactTypeAclConfig`](#artifacttypeaclconfig)`[]`<br>
`acl` | [`AclConfig`](#aclconfig)<br>

## ConfigInclude

Name | Description
---- | -----------
`path` | `string`<br>

## ConfigMacro

Name | Description
---- | -----------
`name` | `string`<br>
`value` | `string`<br>

## PoolConfig

Name | Description
---- | -----------
`id` | `string`<br>
`base` | `string`<br>
`name` | `string`<br>
`condition` | `string`<br>
`properties` | `string` `->` `string`<br>
`color` | [`PoolColor`](#poolcolor-enum)<br>
`enableAutoscaling` | `boolean`<br>
`minAgents` | `integer`<br>
`numReserveAgents` | `integer`<br>
`conformInterval` | `string`<br>
`scaleOutCooldown` | `string`<br>
`scaleInCooldown` | `string`<br>
`shutdownIfDisabledGracePeriod` | `string`<br>
`sizeStrategy` | [`PoolSizeStrategy`](#poolsizestrategy-enum)<br>
`sizeStrategies` | [`PoolSizeStrategyInfo`](#poolsizestrategyinfo)`[]`<br>
`fleetManagers` | [`FleetManagerInfo`](#fleetmanagerinfo)`[]`<br>
`leaseUtilizationSettings` | [`LeaseUtilizationSettings`](#leaseutilizationsettings)<br>
`jobQueueSettings` | [`JobQueueSettings`](#jobqueuesettings)<br>
`computeQueueAwsMetricSettings` | [`ComputeQueueAwsMetricSettings`](#computequeueawsmetricsettings)<br>

## PoolColor (Enum)

Name | Description
---- | -----------
`Default` | 
`Blue` | 
`Orange` | 
`Green` | 
`Gray` | 

## PoolSizeStrategy (Enum)

Name | Description
---- | -----------
`LeaseUtilization` | 
`JobQueue` | 
`NoOp` | 
`ComputeQueueAwsMetric` | 
`LeaseUtilizationAwsMetric` | 

## PoolSizeStrategyInfo

Name | Description
---- | -----------
`type` | [`PoolSizeStrategy`](#poolsizestrategy-enum)<br>
`condition` | `string`<br>
`config` | `string`<br>
`extraAgentCount` | `integer`<br>

## FleetManagerInfo

Name | Description
---- | -----------
`type` | [`FleetManagerType`](#fleetmanagertype-enum)<br>
`condition` | `string`<br>
`config` | `string`<br>

## FleetManagerType (Enum)

Name | Description
---- | -----------
`Default` | 
`NoOp` | 
`Aws` | 
`AwsReuse` | 
`AwsRecycle` | 
`AwsAsg` | 

## LeaseUtilizationSettings

Name | Description
---- | -----------
`sampleTimeSec` | `integer`<br>
`numSamples` | `integer`<br>
`numSamplesForResult` | `integer`<br>
`minAgents` | `integer`<br>
`numReserveAgents` | `integer`<br>

## JobQueueSettings

Name | Description
---- | -----------
`scaleOutFactor` | `number`<br>
`scaleInFactor` | `number`<br>
`samplePeriodMin` | `integer`<br>
`readyTimeThresholdSec` | `integer`<br>

## ComputeQueueAwsMetricSettings

Name | Description
---- | -----------
`computeClusterId` | `string`<br>
`namespace` | `string`<br>

## ProjectCategoryConfig

Name | Description
---- | -----------
`name` | `string`<br>
`row` | `integer`<br>
`showOnNavMenu` | `boolean`<br>
`includePatterns` | `string[]`<br>
`excludePatterns` | `string[]`<br>

## JobOptions

Options for executing a job

Name | Description
---- | -----------
`executor` | `string`<br>Name of the executor to use
`useWine` | `boolean`<br>Whether to execute using Wine emulation on Linux
`runInSeparateProcess` | `boolean`<br>Executes the job lease in a separate process
`workspaceMaterializer` | `string`<br>What workspace materializer to use in WorkspaceExecutor. Will override any value from workspace config.
`container` | [`JobContainerOptions`](#jobcontaineroptions)<br>Options for executing a job inside a container
`expireAfterDays` | `integer`<br>Number of days after which to expire jobs
`driver` | `string`<br>Name of the driver to use

## JobContainerOptions

Options for executing a job inside a container

Name | Description
---- | -----------
`enabled` | `boolean`<br>Whether to execute job inside a container
`imageUrl` | `string`<br>Image URL to container, such as "quay.io/podman/hello"
`containerEngineExecutable` | `string`<br>Container engine executable (docker or with full path like /usr/bin/podman)
`extraArguments` | `string`<br>Additional arguments to pass to container engine

## WorkspaceConfig

Name | Description
---- | -----------
`base` | `string`<br>
`cluster` | `string`<br>
`serverAndPort` | `string`<br>
`userName` | `string`<br>
`password` | `string`<br>
`identifier` | `string`<br>
`stream` | `string`<br>
`view` | `string[]`<br>
`incremental` | `boolean`<br>
`useAutoSdk` | `boolean`<br>
`autoSdkView` | `string[]`<br>
`method` | `string`<br>
`minScratchSpace` | `integer`<br>
`conformDiskFreeSpace` | `integer`<br>

## ArtifactTypeAclConfig

Name | Description
---- | -----------
`type` | `string`<br>
`acl` | [`AclConfig`](#aclconfig)<br>

## AclConfig

Name | Description
---- | -----------
`entries` | [`AclEntryConfig`](#aclentryconfig)`[]`<br>
`profiles` | [`AclProfileConfig`](#aclprofileconfig)`[]`<br>
`inherit` | `boolean`<br>
`exceptions` | `string[]`<br>

## AclEntryConfig

Name | Description
---- | -----------
`claim` | [`AclClaimConfig`](#aclclaimconfig)<br>
`actions` | `string[]`<br>
`profiles` | `string[]`<br>

## AclClaimConfig

Name | Description
---- | -----------
`type` | `string`<br>
`value` | `string`<br>

## AclProfileConfig

Name | Description
---- | -----------
`id` | `string`<br>
`actions` | `string[]`<br>
`excludeActions` | `string[]`<br>
`extends` | `string[]`<br>
