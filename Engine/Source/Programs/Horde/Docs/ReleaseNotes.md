# Release Notes

## 2024-05-30

* Update test projects to NET 8. (34002443)
* Refactor WorkspaceInfo to avoid keeping a long-lived connection. Keeping idle connections to the Perforce server after the initial workspace setup is unnecessary and wastes server resources. (33949575)
* Log in to the server when querying for config files from Perforce. (33945114)
* Fix http auth handler not using correct configuration for server when run locally. (33940162)
* Skip cleaning files for AutoSDK workspace. (33858727)
* Only enable debug controllers if enabled in settings (33857362)
* Add a fingerprint description as part of issue response (33833567)
* EpicGames.Perforce: Fix parsing of array fields from Perforce records. (33807413)
* EpicGames.Perforce: Fix parsing of records which have duplicate field names in child records. (33804970)
* Allow overriding the targets to execute for a job. The optional 'Targets' property can be specified when creating a job, and will replace any -Target= arguments in the command line if set. (33799426)
* Pass -Target arguments to individual steps to ensure that BuildGraph can correctly filter out which blocks to embed in temp storage manifests. (33799034)
* Remove server OS version info from anonymous endpoint responses. (#11909) (33793366)
* Allow overriding the driver for jobs through the JobOptions object. (33791494)
* Prevent AWS status queries from causing session update failures. (33782757)
* Move job driver into a separate executable. (33775640, 33782224)

## 2024-05-20

* Add grouping keys and platforms to tools. Intent is for the dashboard to only show one tool for each grouping key by default (the one with the closest matching platform based on the browser's user-agent), but users can manually expand the list of tools to show other platforms if desired.
  Platforms should be NET RIDs, eg. win-x64, osx-arm64, etc... (https://learn.microsoft.com/en-us/dotnet/core/rid-catalog). (33698418)
* Use native GrpcChannel instances for agent connection management, rather than IRpcConnection. (33660725, 33638856)
* Add an AdditionalArguments property to jobs, which can be used to append arbitrary arguments to those derived from a job's parameters. This field is preserved - but not appended - to the arguments list if arguments are specified explicitly. Needs hooking up to the dashboard. (33630321)
* Prevent default parameters being appended to jobs when an explicit argument list is specified. (33628649)
* Move Perforce/BuildGraph functionality into a new Horde.Agent.Driver application. Intent is to separate this from the core Agent application over time, making it easier to iterate on Job/BuildGraph related functionality outside of the core agent deployment. (33606684)
* Move settings for different executors into their own files. (33605105)
* Trap exceptions when parsing invalid workflow ids from node annotations. (33602812)
* Enable HTTP compression for server-sent responses (33600392)
* Add support for agent queries via replica read (33575058)
* Optimize agent assignment for compute tasks by filtering by pool (33488689)
* Add command-based tracer for MongoDB. Old tracer operates at the collection level, this instead listens for events emitted by the Mongo client. (33479889)
* Keep a cached list of current agents in AgentService (33454500)
* Add an explicit error when the server URL does not have a valid scheme or host name. (33427628)
* Move expired and ephemeral agent clean up to shared ticker (33424708)

## 2024-05-01

* Explicitly check that the expiry time is set when searching for refs to expire, so that DocDB will use an indexed query. (33367777)
* Add CheckConnectionAsync to HordeHttpClient. (33353008)
* Add an index to the refs collection to prevent collection scans when finding refs to expire. (33338629)
* Reduce how long ephemeral and deleted agents are kept in database. Heavy use of auto-scaling can lead to excessive history of ephemeral agents being kept. In the case of AWS, each new instance has a unique agent ID. Many of the queries for agents (and in turn indices) are not optimized for a large collection. (33317167)
* Disable file modified errors in job executor. (33299280)
* Remove legacy job methods from horde_rpc interface. (33298879)
* Remove support for legacy artifact uploads. (33298589)
* Enable agents sending CPU/RAM usage metrics to the server. (33290135, 33289584)

## 2024-04-25

* Allow setting project-wide workspace types. Will automatically be inserted to the workspace type list of each stream belonging to the project. This also allows defining base types which can be inherited from. (33213109)
* EpicGames.Perforce: Fix errors parsing change views from labels. (33207649)
* Delete step logs when deleting a job. (33206294)
* Use the shared registry location to configure the Horde command line tool on Windows, and use a .horde.json in the user folder on Mac and Linux. (33205861)
* Rename step -> job step, report -> job report, batch -> job batch in job responses. (33191560)
* Include graph information in job responses. (33189353)
* Add a service to expire jobs after a period of time. Jobs are kept forever by default, though this may be modified through the ExpireAfterDays parameter in the JobOptions object in streams or projects. (33109364)
* Add auto conform for agents with workspaces below a certain free disk space threshold (33102671)
* Use a standard message for server subsystems that are operating normally. (33100047)
* Add min scratch space to workspace config (33098369)
* Recognize tags in Perforce changelists of the form '#horde 123' as indication that a change fixes a particular issue. The specific tag can be configured in the globals.json file to support multiple deployments with the same Perforce server. (33089498)
* Clear the default ASPNETCORE_HTTP_PORTS environment variable in Docker images to prevent warnings about port overrides on server startup. We manage HTTP port configuration through Horde settings, and don't want the NET runtime image defaults. (33072683)

## 2024-04-18

* Add dashboard's time since last user activity to HTTP log (33068679)
* Fix agent registration not being invalidated when server is reinstalled. Server returns "unauthenticated" (401) response, not "forbidden" (403). Forbidden is only returned when the server deliberately invalidates an agent. (33060431)
* Send DMs to individual users rather than the notification channel when config updates succeed. (33060054)
* Prevent hypens before a period in a sanitized string id. (33044611)
* EpicGames.Perforce: Fix parsing of records with multiple arrays. (33038480)
* Do not allow multiple agent services to run at once. (33037301)
* Handle zero byte reads due to socket shutdown when reading messages into a compute buffer. (33034396)

## 2024-04-12

* Ensure blobs uploaded to the tools endpoint have the tool id as a prefix to the blob locator (32907981)
* Allow specifying a unique id for each parameter in a job template (32906466)
* Send Slack notifications whenever config changes are applied (32897388)
* Make log output directory configurable in agent (32886152)
* Use strongly typed identifiers for artifact log messages in agent. (32861971)
* Fix memory leak and incorrect usage of native P4API library (32798937, 32794622)
* Add debug endpoint for capturing a dotMemory snapshot (32790300)
* Refactor API message names, project locations and reduce gRPC dependency (32785690, 32783652)
* Allow clients to upload the list of references for each blob, removing the need for the server to do it later. (32768299)

## 2024-04-05

* Handle socket shutdown errors gracefully rather than throwing exceptions. (32756972)
* Add log message whenever native P4 connection buffer increases in size above 16mb. (32751407)
* Prevent log appearing empty if tailing is enabled but no new data has been received before the existing tail data is expired. Rpc log sink was waiting forever for new log tail data, so server was discarding the data already received. (32749401)
* Fix default settings for the analytics dashboard, and update documentation to specify the correct telemetry store name. (32741193)
* Update derived data for issues when they are closed by timeout. (32733991)
* Explicitly set parent context for telemetry spans in Perforce updates, to work around async context mismatches. (32732888)
* Rework commit metadata replication to run a single task for each replicated cluster, for a simpler code flow and better tracing data. (32730286)
* Instrument JobTaskSource ticker. We see occasional slow downs in agent assignment for jobs. Adding some additional tracing should help see what's causing it. (32727003)
* Do not assign compute resource to same machine as requester (32723450)

## 2024-04-01

* Fix decoding of UTF8 characters as part of unescaping Json strings. (32638382)
* Increase Jira timeout to 30 seconds, and improve cancellation handling. (32636702)
* Add a defaults.json file for backwards compatibility with older server installers. Just includes default.global.json. (32619739)
* Move tool update packages onto a separate "Internal" tab on the dashboard. (32619239)
* Only export nodes to Horde that are referenced by the initial job parameters, unless -AllowTargetChanges is specified. Prevents data being copied to temp storage that will never be used unless the target list changes. (32616197)
* Add notes for setting up a self-signed cert for testing. (32613289)
* Doc updates. (32595767)
* Comment out the placeholder ticket value in the default config file. (32585713)
* Improve ordering of nodes when writing large file trees to storage, such that the nodes which are read after each other are adjacent in the storage blobs. Nodes are read in the reverse order to which they are written, depth-first. Arranging nodes in the blob in this order prevents thrashing of the cache and improves performance. (32585474)

## 2024-03-28

* Remove /userinfo call when authenticating via JWT. If an access token is passed, it's not guaranteed it has permission to access /userinfo from OIDC. ID tokens during normal web-based login does on the other hand. (32539012)
* Fix parsing of true/false literals in condition expressions. (32518640)
* Add a default pool to include interactive agents, and map it to the TestWin64 agent type. (32514202)
* Add logging to trace when blobs are scanned for imports, to help debug some logs being expired while still referenced. (32506875)
* Normalize keys for artifacts to allow case-insensitive searching. (32495123)
* Support setting arbitrary artifact metadata. This cannot be queried, but will be returned in artifact responses. (32464180)
* Run a background task to monitor for hangs when executing Perforce commands. (32461961)
* Allow configuring different telemetry stores for each project and stream. (32460547)
* Fix slow queries in storage service garbage collection. (32460451)
* Add a [TryAuthorize] attribute which attempts authorization, but which does not return a challenge or forbid result on failure. Should fix tool download requests using service account tokens. (32447487)
* Fix OIDC issues when using Horde auth with UBA. (32406994)

## 2024-03-21

* Lower log level of failed AWS instance start (32397273)
* Assume any exceptions while checking for config files being out of date as meaning they need to be read again. (32388229)
* Use the first specified credentials for server P4 commands if the serviceAccount property is not set. (32383940)
* Update docs for deploying server on Mac. (32370724)
* Log a warning whenever an obsolete endpoint is used. (32329528)
* Fix separate RAM slots being reported separately in agent properties. (32328916)
* Fix use of auth tokens from environment variables when the server URL is also derived from environment variables. (32305895)
* Fix StorageService not being registered as a hosted service, preventing GC from running. (32291615)
* Fix artifacts not being added to graphs. (32286255)
* Add output artifacts to the graph from the exported BuildGraph definition. (32281784)

## 2024-03-15

* Get the userinfo endpoint from the OIDC discovery document rather than assuming it's a fixed path. (32250458)
* Fix serialization issue with global config path on Linux (32240996)
* Improve error reporting when exceptions are thrown during leases. (32233035)
* Prevent DeadlineExceeded exceptions in log rpc task from being logged as errors. (32232976)
* Update EpicGames.Perforce.Native to use the 2023.2 Perforce libraries. (32232242)
* Enable notification service on all server types, otherwise we keep queuing up new tasks without ever removing them. (32220252)
* Invalidate the current session when deleting an agent from the farm. (32212590)
* Invalidate registrations with a server if creating a session fails with a permission denied error. (32212298)
* Fix job costs displaying for all users. (32154404)
* Expose the bundle page size as a configurable option. Still defaults to 1mb. (32153263)
* Use BitFaster.Caching for the LRU bundle cache. (32153088)
* Pool connections in ManagedWorkspace rather than creating a new instance each time. (32144653)
* Exclude appsettings.Local.json files from publish output folders. (32140083)
* Add a UGS config file to command line tool. (32120883)
* Add endpoints to check permissions for certain ACL actions in different scopes. (32118158)
* Immediately cancel running leases if agent status gets set to busy (paused) (32102044)
* Include a separate file containing default values for Horde configuration from the a file copy into C:\ProgramData. (32098391)
* Add OAuth/OIDC support for internal Horde accounts. (32097065)

## 2024-03-07

* Store the URL of the default Horde server in the registry on Windows (31994865)
* Download UGS deployment settings from Horde parameters endpoint (32001513)
* Allow using partitioned workspaces for P4 replication (32041630)
* Allow only keeping a fixed number of artifacts rather than expiring by time (32043455)
* Include dashboard web UI files in public Docker image for Horde server (32054568)
* Turn off native Perforce client on non-Windows agents (31991203)

## 2024-02-29

* Add a back-off delay when RPC client gets interrupted in JsonRpcLogSink (31881762)
* Add a manual approval process for agents joining the farm. New EnrollmentRpc endpoint allows agents to connect and wait for approval without being assigned an agent id or being able to take on work. (31869942)
* Fix bug with tray app not setting agent in paused state during idle (31837900)
* Copy the tray app to a folder under C:\Users\xxx\AppData\Local before running, so the agent can update without having to restart the tray app (which may be running under a different user account). (31815878)
* Use the agent's reported UpdateChannel property to choose which tool to use for updating the installation. ()
* Write the name of the channel to use for agent updates into the agent config file, and report it to the server through the agent properties. (31806993, 31810088)
* Separate implementations for regular user accounts from service accounts. (31802961)
* Added endpoints for querying entitlements of the current user (or any other account). /account/entitlements will return entitlements for the logged in user, and /api/v1/accounts/xxx/entitlements will return entitlements for a Horde account. (31777667)
* Accounts now use the user's full name for the standard OIDC name claim. (https://openid.net/specs/openid-connect-core-1_0.html#StandardClaims) (31773722)

## 2024-02-23

* Fix regression in UGS metadata filtering, where metadata entries with an empty project string should be returned for any project. (31741737)
* Artifact expiry times are now retroactively updated whenever the configured expiry time changes. (31736576)
* Allow specifying a description string for artifacts. (31729492)
* Disable internal Horde account login by default. (31724108)
* Fix bundled tools not being handled correctly in installed builds. (31721936)
* Read registry config first so files and env vars can override (31720705)

## 2024-02-22

* Fixed bundled tools not being handled correctly in installed builds. (31721936)
* Read registry config first so files and env vars can override (31720705)
* Fix leases not being cancelled when a batch moves into the NoLongerNeeded state. (31709570)
* Prevent Windows service startup from completing until all Horde services have started correctly. (31691578)
* Add firewall exception in installer (31678384, 31677487)
* Always return a mime type of text/plain for .log files, allowing them to be viewed in-browser (31648543)
* Configure HTTP/2 port in server installer (31647511)
* Enable new temp storage backend by default. (31637506)
* Support for defining ACL profiles which can be shared between different ACL entries. Intent is to allow configuring some standard permission sets which can be inherited and modified by users. Two default profiles exist - 'generic-read' and 'generic-run' - which match how we have roles configured internally. New profiles can be created in the same scopes that ACL entries can be defined. (31618249)
* Add a new batch error code for agents that fail during syncing. (31576140)
