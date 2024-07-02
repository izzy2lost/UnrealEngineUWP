[Horde](../../../README.md) > [Configuration](../../Config.md) > Globals.json

# Globals.json

Global configuration

Name | Description
---- | -----------
`version` | `integer`<br>Version number for the server. Values are indicated by the .
`include` | [`ConfigInclude`](#configinclude)`[]`<br>Other paths to include
`macros` | [`ConfigMacro`](#configmacro)`[]`<br>Macros within the global scope
`dashboard` | [`DashboardConfig`](Dashboard.md)<br>Settings for the dashboard
`downtime` | [`ScheduledDowntime`](#scheduleddowntime)`[]`<br>List of scheduled downtime
`plugins` | `object`<br>Plugin config objects
`parameters` | `object`<br>General parameters for other tools. Can be queried through the api/v1/parameters endpoint.
`acl` | [`AclConfig`](#aclconfig)<br>Access control list

## ConfigInclude

Name | Description
---- | -----------
`path` | `string`<br>

## ConfigMacro

Name | Description
---- | -----------
`name` | `string`<br>
`value` | `string`<br>

## ScheduledDowntime

Settings for the maintenance window

Name | Description
---- | -----------
`startTime` | `string`<br>Start time
`finishTime` | `string`<br>Finish time
`frequency` | [`ScheduledDowntimeFrequency`](#scheduleddowntimefrequency-enum)<br>Frequency that the window repeats

## ScheduledDowntimeFrequency (Enum)

How frequently the maintence window repeats

Name | Description
---- | -----------
`Once` | Once
`Daily` | Every day
`Weekly` | Every week

## AnalyticsConfig

Name | Description
---- | -----------
`stores` | [`TelemetryStoreConfig`](Telemetry.md)`[]`<br>

## BuildConfig

Name | Description
---- | -----------
`perforceClusters` | [`PerforceCluster`](#perforcecluster)`[]`<br>
`devices` | [`DeviceConfig`](#deviceconfig)<br>
`maxConformCount` | `integer`<br>
`agentShutdownIfDisabledGracePeriod` | `string`<br>
`artifactTypes` | [`ArtifactTypeConfig`](#artifacttypeconfig)`[]`<br>
`projects` | [`ProjectConfig`](Projects.md)`[]`<br>
`issueFixedTag` | `string`<br>

## PerforceCluster

Name | Description
---- | -----------
`name` | `string`<br>
`serviceAccount` | `string`<br>
`canImpersonate` | `boolean`<br>
`supportsPartitionedWorkspaces` | `boolean`<br>
`servers` | [`PerforceServer`](#perforceserver)`[]`<br>
`credentials` | [`PerforceCredentials`](#perforcecredentials)`[]`<br>
`autoSdk` | [`AutoSdkWorkspace`](#autosdkworkspace)`[]`<br>

## PerforceServer

Name | Description
---- | -----------
`serverAndPort` | `string`<br>
`healthCheck` | `boolean`<br>
`resolveDns` | `boolean`<br>
`maxConformCount` | `integer`<br>
`condition` | `string`<br>
`properties` | `string[]`<br>

## PerforceCredentials

Name | Description
---- | -----------
`userName` | `string`<br>
`password` | `string`<br>
`ticket` | `string`<br>

## AutoSdkWorkspace

Name | Description
---- | -----------
`name` | `string`<br>
`properties` | `string[]`<br>
`userName` | `string`<br>
`stream` | `string`<br>

## DeviceConfig

Name | Description
---- | -----------
`platforms` | [`DevicePlatformConfig`](#deviceplatformconfig)`[]`<br>
`pools` | [`DevicePoolConfig`](#devicepoolconfig)`[]`<br>

## DevicePlatformConfig

Name | Description
---- | -----------
`id` | `string`<br>
`name` | `string`<br>
`models` | `string[]`<br>
`legacyNames` | `string[]`<br>
`legacyPerfSpecHighModel` | `string`<br>

## DevicePoolConfig

Name | Description
---- | -----------
`id` | `string`<br>
`name` | `string`<br>
`poolType` | [`DevicePoolType`](#devicepooltype-enum)<br>
`projectIds` | `string[]`<br>

## DevicePoolType (Enum)

The type of device pool

Name | Description
---- | -----------
`Automation` | Available to CIS jobs
`Shared` | Shared by users with remote checking and checkouts

## ArtifactTypeConfig

Name | Description
---- | -----------
`type` | `string`<br>
`name` | `string`<br>
`keepCount` | `integer`<br>
`keepDays` | `integer`<br>

## ComputeConfig

Name | Description
---- | -----------
`acl` | [`AclConfig`](#aclconfig)<br>
`versionEnum` | [`ConfigVersion`](#configversion-enum)<br>
`rates` | [`AgentRateConfig`](#agentrateconfig)`[]`<br>
`clusters` | [`ComputeClusterConfig`](#computeclusterconfig)`[]`<br>
`pools` | [`PoolConfig`](#poolconfig)`[]`<br>
`software` | [`AgentSoftwareConfig`](#agentsoftwareconfig)`[]`<br>
`networks` | [`NetworkConfig`](#networkconfig)`[]`<br>

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

## ConfigVersion (Enum)

Name | Description
---- | -----------
`None` | 
`Initial` | 
`PoolsInConfigFiles` | 
`Latest` | 
`LatestPlusOne` | 

## AgentRateConfig

Name | Description
---- | -----------
`condition` | `string`<br>
`rate` | `number`<br>

## ComputeClusterConfig

Name | Description
---- | -----------
`id` | `string`<br>
`namespaceId` | `string`<br>
`requestBucketId` | `string`<br>
`responseBucketId` | `string`<br>
`condition` | `string`<br>
`acl` | [`AclConfig`](#aclconfig)<br>

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

## AgentSoftwareConfig

Name | Description
---- | -----------
`toolId` | `string`<br>
`condition` | `string`<br>

## NetworkConfig

Name | Description
---- | -----------
`id` | `string`<br>
`cidrBlock` | `string`<br>
`description` | `string`<br>
`computeId` | `string`<br>

## EmptyPluginConfig


## SecretsConfig

Name | Description
---- | -----------
`secrets` | [`SecretConfig`](#secretconfig)`[]`<br>

## SecretConfig

Name | Description
---- | -----------
`id` | `string`<br>
`data` | `string` `->` `string`<br>
`sources` | [`ExternalSecretConfig`](#externalsecretconfig)`[]`<br>
`acl` | [`AclConfig`](#aclconfig)<br>

## ExternalSecretConfig

Name | Description
---- | -----------
`provider` | `string`<br>
`format` | [`ExternalSecretFormat`](#externalsecretformat-enum)<br>
`key` | `string`<br>
`path` | `string`<br>

## ExternalSecretFormat (Enum)

Name | Description
---- | -----------
`Text` | 
`Json` | 

## StorageConfig

Name | Description
---- | -----------
`enableGc` | `boolean`<br>
`enableGcVerification` | `boolean`<br>
`backends` | [`BackendConfig`](#backendconfig)`[]`<br>
`namespaces` | [`NamespaceConfig`](#namespaceconfig)`[]`<br>

## BackendConfig

Name | Description
---- | -----------
`id` | `string`<br>
`base` | `string`<br>
`type` | [`StorageBackendType`](#storagebackendtype-enum)<br>
`baseDir` | `string`<br>
`awsBucketName` | `string`<br>
`awsBucketPath` | `string`<br>
`awsCredentials` | [`AwsCredentialsType`](#awscredentialstype-enum)<br>
`awsRole` | `string`<br>
`awsProfile` | `string`<br>
`awsRegion` | `string`<br>
`azureConnectionString` | `string`<br>
`azureContainerName` | `string`<br>
`relayServer` | `string`<br>
`relayToken` | `string`<br>

## StorageBackendType (Enum)

Name | Description
---- | -----------
`FileSystem` | 
`Aws` | 
`Azure` | 
`Memory` | 

## AwsCredentialsType (Enum)

Name | Description
---- | -----------
`Default` | 
`Profile` | 
`AssumeRole` | 
`AssumeRoleWebIdentity` | 

## NamespaceConfig

Name | Description
---- | -----------
`id` | `string`<br>
`backend` | `string`<br>
`prefix` | `string`<br>
`gcFrequencyHrs` | `number`<br>
`gcDelayHrs` | `number`<br>
`enableAliases` | `boolean`<br>
`acl` | [`AclConfig`](#aclconfig)<br>

## ToolsConfig

Name | Description
---- | -----------
`tools` | [`ToolConfig`](#toolconfig)`[]`<br>

## ToolConfig

Name | Description
---- | -----------
`id` | `string`<br>
`name` | `string`<br>
`description` | `string`<br>
`category` | `string`<br>
`group` | `string`<br>
`platforms` | `string[]`<br>
`public` | `boolean`<br>
`showInUgs` | `boolean`<br>
`showInDashboard` | `boolean`<br>
`namespaceId` | `string`<br>
`acl` | [`AclConfig`](#aclconfig)<br>
