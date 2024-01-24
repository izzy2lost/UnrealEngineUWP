[Horde](../Home.md) > [Deployment](../Deployment.md) > Server

# Server

## Installation

### MSI Installer (Windows)

An installer for Horde is available for download [TODO](???). 

Windows builds of MongoDB and Redis are included in the installer, and launched by Horde at startup (Horde will also close them when it terminates). This installation is fine for small scale installations and testing Horde, though hosting databases separately would be preferred in production scenarios.

### Docker Images (Linux)

Images for hosting Horde through Docker are available through the EpicGames organization on [GitHub](https://www.unrealengine.com/en-US/ue-on-github). Note that you must be signed into GitHub with an account associated with an EpicGames account to follow these links.
  * [Full Image](https://github.com/orgs/EpicGames/packages/container/package/horde)
  * [Server Only](https://github.com/orgs/EpicGames/packages/container/package/horde-server)
  * [Dashboard Only](https://github.com/orgs/EpicGames/packages/container/package/horde-dashboard)

In this form, an external *MongoDB* and *Redis* instance must be configured through a configuration file or environment variable (see below).

Running multiple Horde servers behind a load balancer does not require any explicit configuration, as long as each server points to the same MongoDB and Redis instance.

### Building from Source

Source code for the Horde server is under `Engine/Source/Programs/Horde/Horde.Server/...`. The server can be built and run from Visual Studio using the solution at `Engine/Source/Programs/Horde/Horde.sln`, or from the command line via the `dotnet build` or `dotnet publish` commands.

Docker images can be built through the BuildGraph script at `Engine/Source/Programs/Horde/BuildHorde.xml`, using the Dockerfile in `Engine/Source/Programs/Horde.Server/Dockerfile`. Using the BuildGraph script is recommended over running the Dockerfile directly because it stages the relevant files to a temporary directory before running `docker build`, which prevents the Docker daemon copying the entire UE source tree to the containerized environment before building. The command line for building Docker images using BuildGraph is:

    RunUAT.bat BuildGraph -Script=Engine/Source/Programs/Horde/BuildHorde.xml -Target="Build HordeServer"

The Windows installer can be built from the same BuildGraph script with a similar command line:

    RunUAT.bat BuildGraph -Script=Engine/Source/Programs/Horde/BuildHorde.xml -Target="Build Horde Installer"

## Settings

### General

Server settings are configured through the [`appsettings.json`](ServerSettings.md) file in the server directory. All Horde-specific settings are stored under the `horde` top-level key, with middleware and standard .NET settings under other root keys.

As an ASP.NET application, Horde's application configuration supports the following features:

* Individual properties can be overriden through **environment variables** using standard ASP.NET syntax (see [MSDN](https://learn.microsoft.com/en-us/aspnet/core/fundamentals/configuration/?view=aspnetcore-7.0#naming-of-environment-variables)). The database connection string can be passed in using the `Horde__DatabaseConnectionString` environment variable, for example.
* The deployment environment can be configured using the ASPNETCORE_ENVIRONMENT environment variable. Standard values for Horde are `Production`, `Development` and `Local`.
* A deployment-specific configuration file can be created called `appsettings.{Environment}.json` (eg. `appsettings.Local.json`), which will be merged with other settings.

Note that the _server_ configuration file (`appsettings.json`) is different to the _global_ configuration file (`globals.json`); the server configuration file is deployed alongside the server and contains deployment/infrastructure settings, wheras the global configuration file can be stored in revision control and updated dynamically during the server's lifetime. See [Config > Orientation](../Config/Orientation.md) for more information.

### MongoDB

The MongoDB connection string can be specified via the `DatabaseConnectionString` property in the [appsettings.json](ServerSettings.md) file, or via the `Horde__DatabaseConnectionString` environment variable. The connection string should be in standard [MongoDB syntax](https://www.mongodb.com/docs/manual/reference/connection-string/), eg:

    mongodb://username:password@url:27017?replicaSet=rs0&readPreference=primary

Horde implements a lot of operations as compare-and-swap operations, so it is important that all reads are configured to use the primary database instance using the `readPreference=primary` argument when using a replica set. Using a secondary instance for reads can cause deadlocks due to the server getting out-of-date documents in a read-modify-write cycle.

The MongoDB connection can be configured to use a trusted set of certificates via the `DatabasePublicCert` property. When running on AWS using DocumentDB, for example, this property can be set to use Amazon's [combined certificate bundle](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/UsingWithRDS.SSL.html) by placing the `global-bundle.pem` file into the server's application directory.

### Redis

The Redis server is configured through the RedisConnectionConfig property in the [appsettings.json](ServerSettings.md) file, or via the `Horde__DatabaseConnectionString` environment variable. This string is formatted as a plain server and port, eg:

    redis:6379

### Ports

By default, Horde is configured to serve data over unencrypted HTTP using port 5000. Agents communicate with the Horde server using gRPC over unencrypted HTTP2 on port 5002 by default. 

These settings are echoed to the console during server startup.

A separate port is used for gRPC since Kestrel (the .NET web server) does not support unencrypted HTTP2 traffic over the same port as HTTP1 traffic. If a HTTPS port is configured, all traffic can use that port.

Settings for port usage are defined in [appsettings.json](ServerSettings.md):

* To disable serving data over HTTP, set the `HttpPort` property to zero.
* To configure the secondary HTTP2 port used, set the `Http2Port2` property (or set it to zero to disable it).
* To serve data over HTTPS, set the `HttpsPort` property. This setting can be used independently of the `HttpPort` and `Http2Port2` setting.

### Monitoring

Horde uses [Serilog](https://serilog.net/) for logging, and is configured to generate plain text and JSON log files to the application directory on Linux, and to the `C:\ProgramData\HordeServer` folder on Windows. Plain text output is written to stdout by default, though Json output can be enabled using the `LogJsonToStdOut` property in [appsettings.json](ServerSettings.md).

Profiling and telemetry data for the server is routed through [OpenTelemetry](https://opentelemetry.io/). Settings for telemetry capture are [listed here](ServerSettings.md#OpenTelemetry).

### RunModes

In order to separate lighter request traffic from heavier background operations, the Horde server can be configured to run in different _RunModes_. These are configured via the [RunMode](ServerSettings.md) setting.

### Authentication

Horde supports [OpenID Connect (OIDC)](https://openid.net/developers/how-connect-works/) for authentication using an external identity provider. _OIDC_ is a widely used auth standard, and Okta, Aws, Azure, Google, Facebook, and many others implement identity providers compatible with it.

The following settings in [appsettings.json](ServerSettings.md) are required to configure an OIDC provider:

* `AuthMethod`: Set this to `OpenIdConnect`.
* `OidcAuthority`: URL of the OIDC authority. You can check the URL specified here is correct by navigating to `{{Url}}/.well-known/openid-configuration` in a browser, which should return the OIDC discovery document.
* `OidcClientId`: Identifies the application (Horde) to the OIDC provider. This is generated by the OIDC provider.
* `OidcClientSecret`: Secret value provided by the OIDC provider to identify the application requesting authorization.

In addition, the following settings can be specified:

* `OidcRequestedScopes`: Specifies the scopes requested from the OIDC provider. Scopes determine the access that Horde requests from the OIDC provider, and the claims that will be returned and available for checking against in Horde ACLs. The meaning of these values is specific to your OIDC provider configuration.
* `OidcClaimNameMapping`: Specifies a list of claims to check, in order of preference, when trying to show a user's real name.
* `OidcClaimEmailMapping`: Specifies a list of claims to check, in order of preference, when trying to show a user's email address.
* `OidcClaimHordePerforceUserMapping`: Specifies a list of claims to check, in order of preference, when trying to determine a user's Perforce username.

### Reference

For a full list of valid properties in the server configuration file, see [**appsettings.json (Server)**](ServerSettings.md).
