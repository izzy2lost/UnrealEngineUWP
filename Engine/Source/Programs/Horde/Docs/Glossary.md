[Horde](Home.md) > [Glossary](Glossary.md)

## General

* `Agent` - A service on a remote machine that connects to the Horde server and can be sent work to execute.
* `Lease` - A unit of work that an agent is given to execute.

## Storage

* `Namespace` - A logical partition of the storage system that can have custom permissions, behaviors, and garbage collection policies.
* `Backend` - The underlying storage provider for a particular namespace. Horde supports many types backends, from local disks to cloud object stores.
* `Blob` - An opaque byte stream and set of outward references to other blobs.
* `Ref` - A named reference to a blob. Refs are typically the entry point into the storage system for client applications. Blobs that are not directly or indirectly referenced through a ref are garbage collected.
* `Bundle` - Container for a set of blobs that is written to the underlying storage system. Bundles support compression and reduce the overhead of storing small objects.

## BuildGraph

* `BuildGraph` - Epic's scripting language for large-scale build pipelines (eg. compiling, cooking, and packaging a game to run on multiple platforms). Describes a parameterized dependency graph between nodes that produce artifacts. Epic uses BuildGraph internally to make Unreal Engine and Fortnite builds, and run all the automation around them. Horde executes BuildGraph scripts in jobs, which are created from templates.
* `Node` - A unit of work within a BuildGraph script. Each node may have dependencies on any other nodes or build outputs and executes a sequence of operations to produce its outputs. A node is executed as a job step.
* `Aggregate` - A name given to a set of nodes within a graph as a shorthand.
* `Target` - Specifies the nodes and aggregates within a BuildGraph script a user wishes to execute.

## Build Automation

* `Continuous Integration (CI)` - The process of continually validating a stream of changes submitted to a codebase through build automation.
* `Continuous Delivery (CD)` - Abbreviation for 'Continuous Delivery'; the process of continually producing new builds of a product through build automation.
* `Template` - Describes the options for running a particular BuildGraph script and specifies parameters for how to execute it.
* `Job` - An instance of a template run on a particular changelist with certain parameters.
* `Job Batch` - A set of steps within a job that are run sequentially on a single machine (in a lease) using a synced workspace. Steps within the batch may or may not have dependencies on each other.
* `Job Step` - A unit of work that can pass or fail, tracking the execution of a node.
* `Label` - Annotates a set of nodes whose outcomes can be monitored as a single unit. Labels are displayed prominently on the Horde dashboard, showing information on which parts of a build have succeeded or failed.
* `Preflight` - A build run to test the contents of changes before submitting via a Perforce shelf.
* `Presubmit` - A suite of tests designed to run before users submit changes.