[Horde](Home.md) > Internals

# Internals

## Building Horde

### Server

Horde uses the standard [C# coding conventions](https://learn.microsoft.com/en-us/dotnet/csharp/fundamentals/coding-style/coding-conventions) published by Microsoft, though use tabs rather than spaces for legacy reasons.

We enable most static analyzer warnings that ship with the NET SDK, though some may be disabled through `.editorconfig` files.

Horde is configured to support local development by default. 

When debugging a local Horde server against a live deployment setting the `DatabaseReadOnlyMode` property in [`appsettings.json`](Config/Schema/Server.md) will prevent the server from attempting any operation that will modify the server state. Using a read-only DB account in addition is recommended for safety.

### Dashboard

The Horde dashboard is a frontend client developed in [TypesScript](https://www.typescriptlang.org) using [React](https://react.dev/). Steps 

1. Install [Node.js](https://nodejs.org/en/download)
2. From a command line, install Yarn using: `npm install --global yarn`
3. Navigate to the dashboard folder at `Engine\Source\Programs\Horde\HordeDashboard`
4. Run `yarn install` to install the package dependencies
5. Edit package.json setting the proxy property to point at your server url, for example: `http://localhost:5000`
6. Navigate to the admin token endpoint of your server to get an expiring access token, for example: `http://localhost:5000/api/v1/admin/token`
7. Create a file called `.env.development.local` in the root HordeDashboard folder and paste the access token in like so:
   `REACT_APP_HORDE_DEBUG_TOKEN=eyFhbGciziJIUz`
8. Run `yarn start` to run the development web server which should open a tab to the local dashboard at http://localhost:3000 

### Docker

Horde includes a `Dockerfile` for creating Docker images, though its position in the Unreal Engine source tree requires files to be staged beforehand in order to reduce the size of data copied into build images.

A BuildGraph script to perform these operations is included, under `Engine\Source\Programs\Horde\HordeBuild.xml`, which can be run as follows:

    RunUAT.bat Engine/Source/Programs/Horde/HordeBuild.xml -Target="Build HordeServer"

## Topics

* [Leases](Internals/Leases.md)
* [Logs](Internals/Logs.md)
* [Structured Logging](Internals/StructuredLogging.md)
