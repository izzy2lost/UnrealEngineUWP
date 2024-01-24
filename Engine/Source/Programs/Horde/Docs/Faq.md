[Horde](Home.md) > FAQ

# FAQ

### Why are all these use cases being muddled together?

Most of our target use cases are traditionally handled as distinct components, but bringing them all together
gives us many opportunities for optimization:

* Storage is a key component of any data pipeline for caching. 
* Remote execution needs data close to compute nodes where it can be retrieved quickly.
* Scalable build automation systems can make use of the same scheduling, management tools and auto-scaling
  functionality as a remote execution platform, and require a storage backend for intermediate and final
  artifacts.

We want Unreal Engine to allow developers to focus on making awesome products, and fast, reliable iteration is a 
key component of that. Sharing battle-tested infrastructure that works seamlessly with the engine reduces the 
barrier to entry for other teams.

### Will I need to deploy Horde to use Unreal Engine?

No, Horde will not required to use Unreal Engine. We have been developing with an eye to Epic's needs and believe 
it can provide similar benefits to others.

### Do I have to deploy Horde to the cloud?

No. Horde runs well in local deployments using off-the-shelf hardware, though some applications may benefit from scalable attached storage.

### Do I have to use the CI system / remote execution functionality / test framework / etc...?

No. Each feature is optional, and any disabled features do not incur any costs. It's easier to implement each service using a common framework due to overlapping requirements such as storage and farm management, and it gives 

### Why would I use Horde for build automation, rather than an established build automation system like Jenkins or TeamCity?

Horde is built from the ground up to support development of Unreal Engine projects.

While it is possible to customize a more generic build automation tool, Horde is built to serve the specific needs of Epic and UE developers - supporting heavy throughput, easy parallelism, integration with tools like Unreal Editor and UnrealGameSync, and with richer, more context-aware interface choices. 

Other features, such as Horde's build health and bisection functionality, are fairly unique solutions to working on scrappy, high-velocity development teams.

Horde's CI functionality is not enabled by default. Other functionality in Horde can be used without having to migrate to a new CI system.
