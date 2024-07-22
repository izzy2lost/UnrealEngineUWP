[Horde](../../README.md) > [Deployment](../Deployment.md) > [Server](Server.md) > Server.json

# Server.json

All Horde-specific settings are stored in a root object called `Horde`. Other .NET functionality may be configured using properties in the root of this file.

Name | Description
---- | -----------
`runModes` | [RunMode](#runmode-enum)`[]`<br>Modes that the server should run in. Runmodes can be used in a multi-server deployment to limit the operations that a particular instance will try to perform.
`dataDir` | `string`<br>Override the data directory used by Horde. Defaults to C:\ProgramData\HordeServer on Windows, {AppDir}/Data on other platforms.
`installed` | `boolean`<br>Whether the server is running in 'installed' mode. In this mode, on Windows, the default data directory will use the common application data folder (C:\ProgramData\Epic\Horde), and configuration data will be read from here and the registry. This setting is overridden to false for local builds from appsettings.Local.json.
`httpPort` | `integer`<br>Main port for serving HTTP.
`httpsPort` | `integer`<br>Port for serving HTTP with TLS enabled. Disabled by default.
`http2Port` | `integer`<br>Dedicated port for serving only HTTP/2.
`mongoConnectionString` | `string`<br>Connection string for the Mongo database
`databaseConnectionString` | `string`<br>MongoDB connection string
`mongoDatabaseName` | `string`<br>MongoDB database name
`databaseName` | `string`<br>
`mongoPublicCertificate` | `string`<br>Optional certificate to trust in order to access the database (eg. AWS public cert for TLS)
`databasePublicCert` | `string`<br>
`mongoReadOnlyMode` | `boolean`<br>Access the database in read-only mode (avoids creating indices or updating content) Useful for debugging a local instance of HordeServer against a production database.
`databaseReadOnlyMode` | `boolean`<br>
`shutdownMemoryThreshold` | `integer`<br>Shutdown the current server process if memory usage reaches this threshold (specified in MB)<br>Usually set to 80-90% of available memory to avoid CLR heap using all of it. If a memory leak was to occur, it's usually better to restart the process rather than to let the GC work harder and harder trying to recoup memory.<br>Should only be used when multiple server processes are running behind a load balancer and one can be safely restarted automatically by the underlying process handler (Docker, Kubernetes, AWS ECS, Supervisor etc). The shutdown behaves similar to receiving a SIGTERM and will wait for outstanding requests to finish.
`serverPrivateCert` | `string`<br>Optional PFX certificate to use for encrypting agent SSL traffic. This can be a self-signed certificate, as long as it's trusted by agents.
`authMethod` | [AuthMethod](#authmethod-enum)<br>Issuer for tokens from the auth provider
`oidcProfileName` | `string`<br>Optional profile name to report through the /api/v1/server/auth endpoint. Allows sharing auth tokens between providers configured through the same profile name in OidcToken.exe config files.
`oidcAuthority` | `string`<br>Issuer for tokens from the auth provider
`oidcAudience` | `string`<br>Audience for validating externally issued tokens
`oidcClientId` | `string`<br>Client id for the OIDC authority
`oidcClientSecret` | `string`<br>Client secret for the OIDC authority
`oidcSigninRedirect` | `string`<br>Optional redirect url provided to OIDC login
`oidcLocalRedirectUrls` | `string[]`<br>Optional redirect url provided to OIDC login for external tools (typically to a local server)
`oidcRequestedScopes` | `string[]`<br>OpenID Connect scopes to request when signing in
`oidcClaimNameMapping` | `string[]`<br>List of fields in /userinfo endpoint to try map to the standard name claim (see System.Security.Claims.ClaimTypes.Name)
`oidcClaimEmailMapping` | `string[]`<br>List of fields in /userinfo endpoint to try map to the standard email claim (see System.Security.Claims.ClaimTypes.Email)
`oidcClaimHordeUserMapping` | `string[]`<br>List of fields in /userinfo endpoint to try map to the Horde user claim (see HordeClaimTypes.User)
`oidcClaimHordePerforceUserMapping` | `string[]`<br>List of fields in /userinfo endpoint to try map to the Horde Perforce user claim (see HordeClaimTypes.PerforceUser)
`serverUrl` | `string`<br>Name of this machine
`jwtIssuer` | `string`<br>Name of the issuer in bearer tokens from the server
`jwtExpiryTimeHours` | `integer`<br>Length of time before JWT tokens expire, in hours
`adminClaimType` | `string`<br>The claim type for administrators
`adminClaimValue` | `string`<br>Value of the claim type for administrators
`corsEnabled` | `boolean`<br>Whether to enable Cors, generally for development purposes
`corsOrigin` | `string`<br>Allowed Cors origin
`enableDebugEndpoints` | `boolean`<br>Whether to enable debug/administrative REST API endpoints
`enableNewAgentsByDefault` | `boolean`<br>Whether to automatically enable new agents by default. If false, new agents must manually be enabled before they can take on work.
`schedulePollingInterval` | `string`<br>Interval between rebuilding the schedule queue with a DB query.
`noResourceBackOffTime` | `string`<br>Interval between polling for new jobs
`initiateJobBackOffTime` | `string`<br>Interval between attempting to assign agents to take on jobs
`unknownErrorBackOffTime` | `string`<br>Interval between scheduling jobs when an unknown error occurs
`redisConnectionString` | `string`<br>Config for connecting to Redis server(s). Setting it to null will disable Redis use and connection See format at https://stackexchange.github.io/StackExchange.Redis/Configuration.html
`redisConnectionConfig` | `string`<br>
`redisReadOnlyMode` | `boolean`<br>Whether to disable writes to Redis.
`logServiceWriteCacheType` | `string`<br>Overridden settings for storage backends. Useful for running against a production server with custom backends.
`logJsonToStdOut` | `boolean`<br>Whether to log json to stdout
`logSessionRequests` | `boolean`<br>Whether to log requests to the UpdateSession and QueryServerState RPC endpoints
`scheduleTimeZone` | `string`<br>Timezone for evaluating schedules
`dashboardUrl` | `string`<br>The URl to use for generating links back to the dashboard.
`helpEmailAddress` | `string`<br>Help email address that users can contact with issues
`helpSlackChannel` | `string`<br>Help slack channel that users can use for issues
`globalThreadPoolMinSize` | `integer`<br>Set the minimum size of the global thread pool This value has been found in need of tweaking to avoid timeouts with the Redis client during bursts of traffic. Default is 16 for .NET Core CLR. The correct value is dependent on the traffic the Horde Server is receiving. For Epic's internal deployment, this is set to 40.
`withDatadog` | `boolean`<br>Whether to enable Datadog integration for tracing
`configPath` | `string`<br>Path to the root config file. Relative to the server.json file by default.
`forceConfigUpdateOnStartup` | `boolean`<br>Forces configuration data to be read and updated as part of appplication startup, rather than on a schedule. Useful when running locally.
`openBrowser` | `boolean`<br>Whether to open a browser on startup
`featureFlags` | [FeatureFlagSettings](#featureflagsettings)<br>Experimental features to enable on the server.
`openTelemetry` | [OpenTelemetrySettings](#opentelemetrysettings)<br>Options for OpenTelemetry

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

## FeatureFlagSettings

Feature flags to aid rollout of new features.
Once a feature is running in its intended state and is stable, the flag should be removed. A name and date of when the flag was created is noted next to it to help encourage this behavior. Try having them be just a flag, a boolean.


## OpenTelemetrySettings

OpenTelemetry configuration for collection and sending of traces and metrics.

Name | Description
---- | -----------
`enabled` | `boolean`<br>Whether OpenTelemetry exporting is enabled
`serviceName` | `string`<br>Service name
`serviceNamespace` | `string`<br>Service namespace
`serviceVersion` | `string`<br>Service version
`enableDatadogCompatibility` | `boolean`<br>Whether to enrich and format telemetry to fit presentation in Datadog
`attributes` | `string` `->` `string`<br>Extra attributes to set
`enableConsoleExporter` | `boolean`<br>Whether to enable the console exporter (for debugging purposes)
`protocolExporters` | `string` `->` [OpenTelemetryProtocolExporterSettings](#opentelemetryprotocolexportersettings)<br>Protocol exporters (key is a unique and arbitrary name)

## OpenTelemetryProtocolExporterSettings

Configuration for an OpenTelemetry exporter

Name | Description
---- | -----------
`endpoint` | `string`<br>Endpoint URL. Usually differs depending on protocol used.
`protocol` | `string`<br>Protocol for the exporter ('grpc' or 'httpprotobuf')
