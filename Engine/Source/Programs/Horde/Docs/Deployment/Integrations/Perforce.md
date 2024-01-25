[Horde](../../../Home.md) > [Deployment](../../Deployment.md) > Integrations > Perforce

# Perforce

## General

Horde uses Perforce primarily for CI functionality, but also supports reading configuration data directly from a
Perforce server (see [Configuration > Orientation](../Config/Orientation.md)). Support for other version control
systems may be added in the future.

The Perforce connection to use for reading configuration files is configured alongside the server deployment via the
`perforce` property in the server [appsettings.json](../ServerSettings.md) file.

## Clusters

Epic's Perforce deployment is quite elaborate, and the desire to scale our CI build infrastructure across multiple
regions and datacenters has resulted in bolstering Horde with a lot of custom functionality for interacting with
Peforce edge servers. Horde implements a load balancer for connecting build agents to Perforce servers, for example,
which uses health check data provided by server instances to roll over to new server instances when a server reports
a problem.

Horde supports the use of multiple independent Perforce installations, as well as load balancing across multiple
mirrors _within_ each installation. A collection of Perforce servers which mirror the same data is called a _cluster_.
Each stream in the CI system may be configured to use a different cluster as desired.

Clusters are configured through the `perforceClusters` property in the [globals.json](../Config/Schema/Globals.md)
config file.

There are several configurables for each cluster:

* `Name`: which is used to reference the cluster from a stream in the CI system.
* `Servers`: Each server supports several settings of its own:
  * `ResolveDns`: If true, the given DNS name is resolved to find a concrete list of servers to be used. This allows
  IT/infrastructure teams to add and remove servers to a cluster without having to reconfigure Horde.
  * `Properties`: Specifies properties that the agent must have to select this server.
  * `HealthCheck`: If true, the Horde server will periodically poll the server for its health on a well-known endpoint.
  See [#health-checks] for more information.
* `Credentials`: A list of username/password/tickets for different accounts on this server. CI jobs can request these
  credentials.
* `ServiceAccount`: Sets the username of the account that Horde should use for internal operations, such as querying
  commits from a stream, submitting on behalf of another user, and so on.
* `CanImpersonate`: Indicates whether Horde should attempt to impersonate other users when submitting changes after a
  successful preflight-and-submit operation. Typically requires an administrator account.

## Health Checks

If enabled, health checks for Perforce servers are performed by performing an HTTP `GET` request to
`http://{{ PERFORCE_SERVER_URL }}:5000/healthcheck`. The endpoint is expected to return a JSON document with the
following structure:

    {
        "results": [
            {
                "checker": "edge_traffic_lights"
                "output": "green"
            }
        ]
    }

Where valid values for `output` are:

* `green`: Server is healthy
* `yellow`: Server peformance is degraded.
* `red`: Server is draining existing connections and should not be used.

This functionality is implemented in `PerforceLoadBalancer.GetServerHealthAsync()`.
