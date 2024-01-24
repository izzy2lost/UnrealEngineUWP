[Horde](../../Home.md) > [Configuration](../../Config.md) > *.stream.json

# *.stream.json

Config for a stream

Name | Type | Description
---- | ---- | -----------
`id` | `string` | Identifier for the stream
`path` | `string` | Direct include path for the stream config. For backwards compatibility with old config files when including from a ProjectConfig object.
`include` | [`ConfigInclude`](#configinclude)`[]` | Includes for other configuration files
`name` | `string` | Name of the stream
`clusterName` | `string` | The perforce cluster containing the stream
`order` | `integer` | Order for this stream
`initialAgentType` | `string` | Default initial agent type for templates
`notificationChannel` | `string` | Notification channel for all jobs in this stream
`notificationChannelFilter` | `string` | Notification channel filter for this template. Can be Success|Failure|Warnings
`triageChannel` | `string` | Channel to post issue triage notifications
`jobOptions` | [`JobOptions`](#joboptions) | Default settings for executing jobs
`autoSdkView` | `string[]` | View for the AutoSDK paths to sync. If null, the whole thing will be synced.
`defaultPreflightTemplate` | `string` | Legacy name for the default preflight template
`defaultPreflight` | [`DefaultPreflightConfig`](#defaultpreflightconfig) | Default template for running preflights
`commitTags` | [`CommitTagConfig`](#committagconfig)`[]` | List of tags to apply to commits. Allows fast searching and classification of different commit types (eg. code vs content).
`tabs` | [`JobsTabConfig`](#jobstabconfig)`[]` | List of tabs to show for the new stream
`environment` | `string` `->` `string` | Global environment variables for all agents in this stream
`agentTypes` | `string` `->` [`AgentConfig`](#agentconfig) | Map of agent name to type
`workspaceTypes` | `string` `->` [`WorkspaceConfig`](#workspaceconfig) | Map of workspace name to type
`templates` | [`TemplateRefConfig`](#templaterefconfig)`[]` | List of templates to create
`acl` | [`AclConfig`](#aclconfig) | Custom permissions for this object
`pausedUntil` | `string` | Pause stream builds until specified date
`pauseComment` | `string` | Reason for pausing builds of the stream
`replicationMode` | [`ContentReplicationMode`](#contentreplicationmode-enum) | How to replicate data from VCS to Horde Storage.
`replicationFilter` | `string` | Filter for paths to be replicated to storage, as a Perforce wildcard relative to the root of the workspace.
`replicationStream` | `string` | Stream to use for replication, if different to the default.
`workflows` | [`WorkflowConfig`](#workflowconfig)`[]` | Workflows for dealing with new issues
`tokens` | [`TokenConfig`](#tokenconfig)`[]` | Tokens to create for each job step

## ConfigInclude

Directive to merge config data from another source

Name | Type | Description
---- | ---- | -----------
`path` | `string` | Path to the config data to be included. May be relative to the including file's location.

## JobOptions

Options for executing a job

Name | Type | Description
---- | ---- | -----------
`executor` | `string` | Name of the executor to use
`useNewLogStorage` | `boolean` | Whether to use the new log storage backend
`useNewTempStorage` | `boolean` | Whether to use the new temp storage backend
`useWine` | `boolean` | Whether to execute using Wine emulation on Linux
`runInSeparateProcess` | `boolean` | Executes the job lease in a separate process
`workspaceMaterializer` | `string` | What workspace materializer to use in WorkspaceExecutor. Will override any value from workspace config.
`collectIbMonFilesAsArtifacts` | `boolean` | Whether to search for and save any *.ib_mon files from Incredibuild after a job step
`container` | [`JobContainerOptions`](#jobcontaineroptions) | Options for executing a job inside a container

## JobContainerOptions

Options for executing a job inside a container

Name | Type | Description
---- | ---- | -----------
`enabled` | `boolean` | Whether to execute job inside a container
`imageUrl` | `string` | Image URL to container, such as "quay.io/podman/hello"
`containerEngineExecutable` | `string` | Container engine executable (docker or with full path like /usr/bin/podman)
`extraArguments` | `string` | Additional arguments to pass to container engine

## DefaultPreflightConfig

Specifies defaults for running a preflight

Name | Type | Description
---- | ---- | -----------
`templateId` | `string` | The template id to query
`change` | [`ChangeQueryConfig`](#changequeryconfig) | Query for the change to use

## ChangeQueryConfig

Query selecting the base changelist to use

Name | Type | Description
---- | ---- | -----------
`name` | `string` | Name of this query, for display on the dashboard.
`condition` | `string` | Condition to evaluate before deciding to use this query. May query tags in a preflight.
`templateId` | `string` | The template id to query
`target` | `string` | The target to query
`outcomes` | [`JobStepOutcome`](#jobstepoutcome-enum)`[]` | Whether to match a job that produced warnings
`commitTag` | `string` | Finds the last commit with this tag

## JobStepOutcome (Enum)

Name | Description
---- | -----------
`Unspecified` | 
`Failure` | 
`Warnings` | 
`Success` | 

## CommitTagConfig

Configuration for custom commit filters

Name | Type | Description
---- | ---- | -----------
`name` | `string` | Name of the tag
`base` | `string` | Base tag to copy settings from
`filter` | `string[]` | List of files to be included in this filter

## JobsTabConfig

Describes a job page

Name | Type | Description
---- | ---- | -----------
`type` | Jobs | Type discriminator
`showNames` | `boolean` | Whether to show job names on this page
`showPreflights` | `boolean` | Whether to show all user preflights
`jobNames` | `string[]` | Names of jobs to include on this page. If there is only one name specified, the name column does not need to be displayed.
`templates` | `string[]` | List of job template names to show on this page.
`columns` | [`JobsTabColumnConfig`](#jobstabcolumnconfig)`[]` | Columns to display for different types of aggregates
`title` | `string` | Title of this page

## JobsTabColumnConfig

Describes a column to display on the jobs page

Name | Type | Description
---- | ---- | -----------
`type` | [`JobsTabColumnType`](#jobstabcolumntype-enum) | The type of column
`heading` | `string` | Heading for this column
`category` | `string` | Category of aggregates to display in this column. If null, includes any aggregate not matched by another column.
`parameter` | `string` | Parameter to show in this column
`relativeWidth` | `integer` | Relative width of this column.

## JobsTabColumnType (Enum)

Type of a column in a jobs tab

Name | Description
---- | -----------
`Labels` | Contains labels
`Parameter` | Contains parameters

## AgentConfig

Mapping from a BuildGraph agent type to a set of machines on the farm

Name | Type | Description
---- | ---- | -----------
`pool` | `string` | Pool of agents to use for this agent type
`workspace` | `string` | Name of the workspace to sync
`tempStorageDir` | `string` | Path to the temporary storage dir
`environment` | `string` `->` `string` | Environment variables to be set when executing the job
`tokens` | [`TokenConfig`](#tokenconfig)`[]` | Tokens to allocate for this agent type

## TokenConfig

Configuration for allocating access tokens for each job

Name | Type | Description
---- | ---- | -----------
`url` | `string` | URL to request tokens from
`clientId` | `string` | Client id to use to request a new token
`clientSecret` | `string` | Client secret to request a new access token
`envVar` | `string` | Environment variable to set with the access token

## WorkspaceConfig

Information about a workspace type

Name | Type | Description
---- | ---- | -----------
`cluster` | `string` | Name of the Perforce server cluster to use
`serverAndPort` | `string` | The Perforce server and port (eg. perforce:1666)
`userName` | `string` | User to log into Perforce with (defaults to buildmachine)
`password` | `string` | Password to use to log into the workspace
`identifier` | `string` | Identifier to distinguish this workspace from other workspaces. Defaults to the workspace type name.
`stream` | `string` | Override for the stream to sync
`view` | `string[]` | Custom view for the workspace
`incremental` | `boolean` | Whether to use an incrementally synced workspace
`useAutoSdk` | `boolean` | Whether to use the AutoSDK
`autoSdkView` | `string[]` | View for the AutoSDK paths to sync. If null, the whole thing will be synced.
`method` | `string` | Method to use when syncing/materializing data from Perforce

## TemplateRefConfig

Parameters to create a template within a stream

Name | Type | Description
---- | ---- | -----------
`id` | `string` | Optional identifier for this ref. If not specified, an id will be generated from the name.
`showUgsBadges` | `boolean` | Whether to show badges in UGS for these jobs
`showUgsAlerts` | `boolean` | Whether to show alerts in UGS for these jobs
`notificationChannel` | `string` | Notification channel for this template. Overrides the stream channel if set.
`notificationChannelFilter` | `string` | Notification channel filter for this template. Can be Success|Failure|Warnings
`triageChannel` | `string` | Triage channel for this template. Overrides the stream channel if set.
`workflowId` | `string` | Workflow to user for this stream
`annotations` | `string` `->` `string` | Default annotations to apply to nodes in this template
`schedule` | [`ScheduleConfig`](#scheduleconfig) | Schedule to execute this template
`chainedJobs` | [`ChainedJobTemplateConfig`](#chainedjobtemplateconfig)`[]` | List of chained job triggers
`acl` | [`AclConfig`](#aclconfig) | The ACL for this template
`name` | `string` | Name for the new template
`description` | `string` | Description for the template
`priority` | [`Priority`](#priority-enum) | Default priority for this job
`allowPreflights` | `boolean` | Whether to allow preflights of this template
`updateIssues` | `boolean` | Whether issues should be updated for all jobs using this template
`promoteIssuesByDefault` | `boolean` | Whether issues should be promoted by default for this template, promoted issues will generate user notifications
`initialAgentType` | `string` | Initial agent type to parse the buildgraph script on
`submitNewChange` | `string` | Path to a file within the stream to submit to generate a new changelist for jobs
`submitDescription` | `string` | Description for new changelists
`defaultChange` | [`ChangeQueryConfig`](#changequeryconfig)`[]` | Default change to build at. Each object has a condition parameter which can evaluated by the server to determine which change to use.
`arguments` | `string[]` | Fixed arguments for the new job
`parameters` | [`GroupParameterData`](#groupparameterdata)/[`TextParameterData`](#textparameterdata)/[`ListParameterData`](#listparameterdata)/[`BoolParameterData`](#boolparameterdata)`[]` | Parameters for this template
`jobOptions` | [`JobOptions`](#joboptions) | Default settings for jobs

## ScheduleConfig

Parameters to create a new schedule

Name | Type | Description
---- | ---- | -----------
`claims` | [`AclClaimConfig`](#aclclaimconfig)`[]` | Roles to impersonate for this schedule
`enabled` | `boolean` | Whether the schedule should be enabled
`maxActive` | `integer` | Maximum number of builds that can be active at once
`maxChanges` | `integer` | Maximum number of changes the schedule can fall behind head revision. If greater than zero, builds will be triggered for every submitted changelist until the backlog is this size.
`requireSubmittedChange` | `boolean` | Whether the build requires a change to be submitted
`gate` | [`ScheduleGateConfig`](#schedulegateconfig) | Gate allowing the schedule to trigger
`commits` | `string[]` | Commit tags for this schedule
`filter` | [`ChangeContentFlags`](#changecontentflags-enum)`[]` | The types of changes to run for
`files` | `string[]` | Files that should cause the job to trigger
`templateParameters` | `string` `->` `string` | Parameters for the template
`patterns` | [`SchedulePatternConfig`](#schedulepatternconfig)`[]` | New patterns for the schedule

## AclClaimConfig

New claim to create

Name | Type | Description
---- | ---- | -----------
`type` | `string` | The claim type
`value` | `string` | The claim value

## ScheduleGateConfig

Gate allowing a schedule to trigger.

Name | Type | Description
---- | ---- | -----------
`templateId` | `string` | The template containing the dependency
`target` | `string` | Target to wait for

## ChangeContentFlags (Enum)

Flags identifying content of a changelist

Name | Description
---- | -----------
`ContainsCode` | The change contains code
`ContainsContent` | The change contains content

## SchedulePatternConfig

Parameters to create a new schedule

Name | Type | Description
---- | ---- | -----------
`daysOfWeek` | [`DayOfWeek`](#dayofweek-enum)`[]` | Days of the week to run this schedule on. If null, the schedule will run every day.
`minTime` | `integer` | Time during the day for the first schedule to trigger. Measured in minutes from midnight.
`maxTime` | `integer` | Time during the day for the last schedule to trigger. Measured in minutes from midnight.
`interval` | `integer` | Interval between each schedule triggering

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

Trigger for another template

Name | Type | Description
---- | ---- | -----------
`trigger` | `string` | Name of the target that needs to complete before starting the other template
`templateId` | `string` | Id of the template to trigger
`useDefaultChangeForTemplate` | `boolean` | Whether to use the default change for the template rather than the change for the parent job.

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

## Priority (Enum)

Name | Description
---- | -----------
`Unspecified` | Not specified
`Lowest` | Lowest priority
`BelowNormal` | Below normal priority
`Normal` | Normal priority
`AboveNormal` | Above normal priority
`High` | High priority
`Highest` | Highest priority

## GroupParameterData

Used to group a number of other parameters

Name | Type | Description
---- | ---- | -----------
`type` | Group | Type discriminator
`label` | `string` | Label to display next to this parameter
`style` | [`GroupParameterStyle`](#groupparameterstyle-enum) | How to display this group
`children` | [`GroupParameterData`](#groupparameterdata)/[`TextParameterData`](#textparameterdata)/[`ListParameterData`](#listparameterdata)/[`BoolParameterData`](#boolparameterdata)`[]` | List of child parameters

## GroupParameterStyle (Enum)

Describes how to render a group parameter

Name | Description
---- | -----------
`Tab` | Separate tab on the form
`Section` | Section with heading

## TextParameterData

Free-form text entry parameter

Name | Type | Description
---- | ---- | -----------
`type` | Text | Type discriminator
`label` | `string` | Name of the parameter associated with this parameter.
`argument` | `string` | Argument to pass to the executor
`default` | `string` | Default value for this argument
`hint` | `string` | Hint text for this parameter
`validation` | `string` | Regex used to validate this parameter
`validationError` | `string` | Message displayed if validation fails, informing user of valid values.
`toolTip` | `string` | Tool-tip text to display

## ListParameterData

Allows the user to select a value from a constrained list of choices

Name | Type | Description
---- | ---- | -----------
`type` | List | Type discriminator
`label` | `string` | Label to display next to this parameter. Defaults to the parameter name.
`style` | [`ListParameterStyle`](#listparameterstyle-enum) | The type of list parameter
`items` | [`ListParameterItemData`](#listparameteritemdata)`[]` | List of values to display in the list
`toolTip` | `string` | Tool tip text to display

## ListParameterStyle (Enum)

Style of list parameter

Name | Description
---- | -----------
`List` | Regular drop-down list. One item is always selected.
`MultiList` | Drop-down list with checkboxes
`TagPicker` | Tag picker from list of options

## ListParameterItemData

Possible option for a list parameter

Name | Type | Description
---- | ---- | -----------
`group` | `string` | Optional group heading to display this entry under, if the picker style supports it.
`text` | `string` | Name of the parameter associated with this list.
`argumentIfEnabled` | `string` | Argument to pass with this parameter.
`argumentIfDisabled` | `string` | Argument to pass with this parameter.
`default` | `boolean` | Whether this item is selected by default

## BoolParameterData

Allows the user to toggle an option on or off

Name | Type | Description
---- | ---- | -----------
`type` | Bool | Type discriminator
`label` | `string` | Name of the parameter associated with this parameter.
`argumentIfEnabled` | `string` | Value if enabled
`argumentIfDisabled` | `string` | Value if disabled
`default` | `boolean` | Whether this argument is enabled by default
`toolTip` | `string` | Tool tip text to display

## ContentReplicationMode (Enum)

How to replicate data for this stream

Name | Description
---- | -----------
`None` | No content will be replicated for this stream
`RevisionsOnly` | Only replicate depot path and revision data for each file
`Full` | Replicate full stream contents to storage

## WorkflowConfig

Configuration for an issue workflow

Name | Type | Description
---- | ---- | -----------
`id` | `string` | Identifier for this workflow
`reportTimes` | `string[]` | Times of day at which to send a report
`summaryTab` | `string` | Name of the tab to post summary data to
`reportChannel` | `string` | Channel to post summary information for these templates.
`groupIssuesByTemplate` | `boolean` | Whether to group issues by template in the report
`triageChannel` | `string` | Channel to post threads for triaging new issues
`triagePrefix` | `string` | Prefix for all triage messages
`triageSuffix` | `string` | Suffix for all triage messages
`triageInstructions` | `string` | Instructions posted to triage threads
`triageAlias` | `string` | User id of a Slack user/alias to ping if there is nobody assigned to an issue by default.
`triageTypeAliases` | `string` `->` `string` | Slack user/alias to ping for specific issue types (such as Systemic), if there is nobody assigned to an issue by default.
`escalateAlias` | `string` | Alias to ping if an issue has not been resolved for a certain amount of time
`escalateTimes` | `integer[]` | Times after an issue has been opened to escalate to the alias above, in minutes. Continues to notify on the last interval once reaching the end of the list.
`maxMentions` | `integer` | Maximum number of people to mention on a triage thread
`allowMentions` | `boolean` | Whether to mention people on this thread. Useful to disable for testing.
`inviteRestrictedUsers` | `boolean` | Uses the admin.conversations.invite API to invite users to the channel
`annotations` | `string` `->` `string` | Additional node annotations implicit in this workflow
`externalIssues` | [`ExternalIssueConfig`](#externalissueconfig) | External issue tracking configuration for this workflow

## ExternalIssueConfig

External issue tracking configuration for a workflow

Name | Type | Description
---- | ---- | -----------
`projectKey` | `string` | Project key in external issue tracker
`defaultComponentId` | `string` | Default component id for issues using workflow
`defaultIssueTypeId` | `string` | Default issue type id for issues using workflow
