[Horde](../../../README.md) > [Configuration](../../Config.md) > *.stream.json

# *.stream.json

Name | Description
---- | -----------
`id` | `string`<br>
`path` | `string`<br>
`include` | [`ConfigInclude`](#configinclude)`[]`<br>
`macros` | [`ConfigMacro`](#configmacro)`[]`<br>
`name` | `string`<br>
`clusterName` | `string`<br>
`order` | `integer`<br>
`initialAgentType` | `string`<br>
`notificationChannel` | `string`<br>
`notificationChannelFilter` | `string`<br>
`triageChannel` | `string`<br>
`jobOptions` | [`JobOptions`](#joboptions)<br>
`telemetryStoreId` | `string`<br>
`autoSdkView` | `string[]`<br>
`defaultPreflightTemplate` | `string`<br>
`defaultPreflight` | [`DefaultPreflightConfig`](#defaultpreflightconfig)<br>
`commitTags` | [`CommitTagConfig`](#committagconfig)`[]`<br>
`tabs` | [`TabConfig`](#tabconfig)`[]`<br>
`environment` | `string` `->` `string`<br>
`agentTypes` | `string` `->` [`AgentConfig`](#agentconfig)<br>
`workspaceTypes` | `string` `->` [`WorkspaceConfig`](#workspaceconfig)<br>
`templates` | [`TemplateRefConfig`](#templaterefconfig)`[]`<br>
`acl` | [`AclConfig`](#aclconfig)<br>
`pausedUntil` | `string`<br>
`pauseComment` | `string`<br>
`replicators` | [`ReplicatorConfig`](#replicatorconfig)`[]`<br>
`workflows` | [`WorkflowConfig`](#workflowconfig)`[]`<br>
`tokens` | [`TokenConfig`](#tokenconfig)`[]`<br>
`artifactTypes` | [`ArtifactTypeAclConfig`](#artifacttypeaclconfig)`[]`<br>

## ConfigInclude

Name | Description
---- | -----------
`path` | `string`<br>

## ConfigMacro

Name | Description
---- | -----------
`name` | `string`<br>
`value` | `string`<br>

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

## DefaultPreflightConfig

Name | Description
---- | -----------
`templateId` | `string`<br>
`change` | [`ChangeQueryConfig`](#changequeryconfig)<br>

## ChangeQueryConfig

Query selecting the base changelist to use

Name | Description
---- | -----------
`name` | `string`<br>Name of this query, for display on the dashboard.
`condition` | `string`<br>Condition to evaluate before deciding to use this query. May query tags in a preflight.
`templateId` | `string`<br>The template id to query
`target` | `string`<br>The target to query
`outcomes` | [`JobStepOutcome`](#jobstepoutcome-enum)`[]`<br>Whether to match a job that produced warnings
`commitTag` | `string`<br>Finds the last commit with this tag

## JobStepOutcome (Enum)

Outcome for a jobstep

Name | Description
---- | -----------
`Unspecified` | Outcome is not known
`Failure` | Step failed
`Warnings` | Step completed with warnings
`Success` | Step succeeded

## CommitTagConfig

Name | Description
---- | -----------
`name` | `string`<br>
`base` | `string`<br>
`filter` | `string[]`<br>

## TabConfig

Name | Description
---- | -----------
`title` | `string`<br>
`type` | `string`<br>
`style` | [`TabStyle`](#tabstyle-enum)<br>
`showNames` | `boolean`<br>
`showPreflights` | `boolean`<br>
`jobNames` | `string[]`<br>
`templates` | `string[]`<br>
`columns` | [`TabColumnConfig`](#tabcolumnconfig)`[]`<br>

## TabStyle (Enum)

Name | Description
---- | -----------
`Normal` | 
`Compact` | 

## TabColumnConfig

Name | Description
---- | -----------
`type` | [`TabColumnType`](#tabcolumntype-enum)<br>
`heading` | `string`<br>
`category` | `string`<br>
`parameter` | `string`<br>
`relativeWidth` | `integer`<br>

## TabColumnType (Enum)

Name | Description
---- | -----------
`Labels` | 
`Parameter` | 

## AgentConfig

Name | Description
---- | -----------
`base` | `string`<br>
`pool` | `string`<br>
`workspace` | `string`<br>
`tempStorageDir` | `string`<br>
`environment` | `string` `->` `string`<br>
`tokens` | [`TokenConfig`](#tokenconfig)`[]`<br>

## TokenConfig

Name | Description
---- | -----------
`url` | `string`<br>
`clientId` | `string`<br>
`clientSecret` | `string`<br>
`envVar` | `string`<br>

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

## TemplateRefConfig

Name | Description
---- | -----------
`id` | `string`<br>
`base` | `string`<br>
`showUgsBadges` | `boolean`<br>
`showUgsAlerts` | `boolean`<br>
`notificationChannel` | `string`<br>
`notificationChannelFilter` | `string`<br>
`triageChannel` | `string`<br>
`workflowId` | `string`<br>
`annotations` | `string` `->` `string`<br>
`schedule` | [`ScheduleConfig`](#scheduleconfig)<br>
`chainedJobs` | [`ChainedJobTemplateConfig`](#chainedjobtemplateconfig)`[]`<br>
`acl` | [`AclConfig`](#aclconfig)<br>
`name` | `string`<br>
`description` | `string`<br>
`priority` | [`Priority`](#priority-enum)<br>
`allowPreflights` | `boolean`<br>
`updateIssues` | `boolean`<br>
`promoteIssuesByDefault` | `boolean`<br>
`initialAgentType` | `string`<br>
`submitNewChange` | `string`<br>
`submitDescription` | `string`<br>
`defaultChange` | [`ChangeQueryConfig`](#changequeryconfig)`[]`<br>
`arguments` | `string[]`<br>
`parameters` | [`TextParameterData`](#textparameterdata)/[`ListParameterData`](#listparameterdata)/[`BoolParameterData`](#boolparameterdata)`[]`<br>
`jobOptions` | [`JobOptions`](#joboptions)<br>

## ScheduleConfig

Name | Description
---- | -----------
`claims` | [`AclClaimConfig`](#aclclaimconfig)`[]`<br>
`enabled` | `boolean`<br>
`maxActive` | `integer`<br>
`maxChanges` | `integer`<br>
`requireSubmittedChange` | `boolean`<br>
`gate` | [`ScheduleGateConfig`](#schedulegateconfig)<br>
`commits` | `string[]`<br>
`filter` | [`ChangeContentFlags`](#changecontentflags-enum)`[]`<br>
`files` | `string[]`<br>
`templateParameters` | `string` `->` `string`<br>
`patterns` | [`SchedulePatternConfig`](#schedulepatternconfig)`[]`<br>

## AclClaimConfig

Name | Description
---- | -----------
`type` | `string`<br>
`value` | `string`<br>

## ScheduleGateConfig

Name | Description
---- | -----------
`templateId` | `string`<br>
`target` | `string`<br>

## ChangeContentFlags (Enum)

Name | Description
---- | -----------
`ContainsCode` | 
`ContainsContent` | 

## SchedulePatternConfig

Name | Description
---- | -----------
`daysOfWeek` | [`DayOfWeek`](#dayofweek-enum)`[]`<br>
`minTime` | `string`<br>
`maxTime` | `string`<br>
`interval` | `string`<br>

## DayOfWeek (Enum)

Name | Description
---- | -----------
`Sunday` | 
`Monday` | 
`Tuesday` | 
`Wednesday` | 
`Thursday` | 
`Friday` | 
`Saturday` | 

## ChainedJobTemplateConfig

Name | Description
---- | -----------
`trigger` | `string`<br>
`templateId` | `string`<br>
`useDefaultChangeForTemplate` | `boolean`<br>

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

## AclProfileConfig

Name | Description
---- | -----------
`id` | `string`<br>
`actions` | `string[]`<br>
`excludeActions` | `string[]`<br>
`extends` | `string[]`<br>

## Priority (Enum)

Priority of a job or step

Name | Description
---- | -----------
`Unspecified` | Not specified
`Lowest` | Lowest priority
`BelowNormal` | Below normal priority
`Normal` | Normal priority
`AboveNormal` | Above normal priority
`High` | High priority
`Highest` | Highest priority

## TextParameterData

Name | Description
---- | -----------
`type` | Text<br>Type discriminator
`id` | `string`<br>
`label` | `string`<br>
`argument` | `string`<br>
`default` | `string`<br>
`scheduleOverride` | `string`<br>
`hint` | `string`<br>
`validation` | `string`<br>
`validationError` | `string`<br>
`toolTip` | `string`<br>

## ListParameterData

Name | Description
---- | -----------
`type` | List<br>Type discriminator
`label` | `string`<br>
`style` | [`ListParameterStyle`](#listparameterstyle-enum)<br>
`items` | [`ListParameterItemData`](#listparameteritemdata)`[]`<br>
`toolTip` | `string`<br>

## ListParameterStyle (Enum)

Name | Description
---- | -----------
`List` | 
`MultiList` | 
`TagPicker` | 

## ListParameterItemData

Name | Description
---- | -----------
`id` | `string`<br>
`group` | `string`<br>
`text` | `string`<br>
`argumentIfEnabled` | `string`<br>
`argumentsIfEnabled` | `string[]`<br>
`argumentIfDisabled` | `string`<br>
`argumentsIfDisabled` | `string[]`<br>
`default` | `boolean`<br>
`scheduleOverride` | `boolean`<br>

## BoolParameterData

Name | Description
---- | -----------
`type` | Bool<br>Type discriminator
`id` | `string`<br>
`label` | `string`<br>
`argumentIfEnabled` | `string`<br>
`argumentsIfEnabled` | `string[]`<br>
`argumentIfDisabled` | `string`<br>
`argumentsIfDisabled` | `string[]`<br>
`default` | `boolean`<br>
`scheduleOverride` | `boolean`<br>
`toolTip` | `string`<br>

## ReplicatorConfig

Name | Description
---- | -----------
`id` | `string`<br>
`enabled` | `boolean`<br>
`minChange` | `integer`<br>
`maxChange` | `integer`<br>

## WorkflowConfig

Name | Description
---- | -----------
`id` | `string`<br>
`reportTimes` | `string[]`<br>
`summaryTab` | `string`<br>
`reportChannel` | `string`<br>
`reportWarnings` | `boolean`<br>
`groupIssuesByTemplate` | `boolean`<br>
`triageChannel` | `string`<br>
`triagePrefix` | `string`<br>
`triageSuffix` | `string`<br>
`triageInstructions` | `string`<br>
`triageAlias` | `string`<br>
`triageTypeAliases` | `string` `->` `string`<br>
`escalateAlias` | `string`<br>
`escalateTimes` | `integer[]`<br>
`maxMentions` | `integer`<br>
`allowMentions` | `boolean`<br>
`inviteRestrictedUsers` | `boolean`<br>
`skipWhenEmpty` | `boolean`<br>
`annotations` | `string` `->` `string`<br>
`externalIssues` | [`ExternalIssueConfig`](#externalissueconfig)<br>
`issueHandlers` | `string[]`<br>

## ExternalIssueConfig

Name | Description
---- | -----------
`projectKey` | `string`<br>
`defaultComponentId` | `string`<br>
`defaultIssueTypeId` | `string`<br>

## ArtifactTypeAclConfig

Name | Description
---- | -----------
`type` | `string`<br>
`acl` | [`AclConfig`](#aclconfig)<br>
