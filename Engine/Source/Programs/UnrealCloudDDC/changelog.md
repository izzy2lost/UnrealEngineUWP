# Unreleased

# 0.5.0
* Fixed issue introduced in `0.4.0` that would cause last access tracking to not work correctly.
* Ability to run with auth enabled but using only the service account scheme - useful for a simpler setup as that does not require setup specific information.
* Improvments to scylla requests to avoid churning the scylla cache when doing GC.
* Fixed bug in the ref memory cache were overwrites would not correctly updated the local cache.
* Tweaks to scylla node connection limits to allow for more requests per connection.

# 0.4.0
* *Breaking* The scylla connection string now needs to include the default keyspace, a example connection string is  `Contact Points=your-scylla-dns.your-domain.com;;Default Keyspace=jupiter;`. The keyspace is `jupiter` is you are migrating from older releases. This allows you to also set the keyspace to something different if you want to run multiple instances of Unreal Cloud DDC against the same scylla cluster.)
* Migration options from `0.3.0` have been updated to assume you have migrated by default.
* Added `prepareNvmeFilesystem` section in Helm chart that creates a initContainer which will format a attached nvme drive.
* Fixed issue when content id remapped due to a new version, its supposed to use the smaller version but was in fact using the larger.
* Bug fixes to make the speculative replication more resilient.
* Ability to use nginx to sanatize http traffic and only use a level 4 load balancer in front as a performance improvment.
* Fixes for handling 2GB+ files.
* Fixes for on-demand replication.
* Support for S3 multipart uploads of large files.
* `Scylla.AvoidSchemaChanges` can now be set to avoid triggering schema modifications - this forces you to manually apply any schema changes required when upgrading but also means you can avoid triggering schema changes while maintinance is running.
* Added `MetricsService` which is disabled by default, will scan all data to determine things like number of objects in each bucket and sizes. Puts a fairly heavy load on your DB so is disabled by default.
* Added GC of non-finalized refs at a more aggresive cadence then normal refs (removed when they are 2 hours old)
  
# 0.3.0
* Azure blob storage now supports storage pools
* Last access table refactoring - Moved the last accessing tracking out of the objects table and into a seperate table. Saves on compation work for Scylla. Set `Scylla.ListObjectsFromLastAccessTable` to migrate GC refs to use this new table (will be default in the next release).
* Blob index table refactoring - Multiple changes to the blob index table, avoiding collections in the scylla rows and instead prefering clustering keys. This reduces amount of work that needs to happen when updating regions or references to blobs and improves performance (and caching) on the DB. We automatically migrate old blob index entiries when `Scylla.MigrateFromOldBlobIndex` is set (it is by default). We will start requring data in these tables by the next release. 
* Bucket table refactoring - Reads data from old and new table for this release. Set `Scylla.ListObjectsFromOldNamespaceTable` to disable reading the old table.
* Generally reduced some of the excessive logging from the worker in favor of open telemetry metrics instead.
* GC Rules - Added ability to disable GC or configure it to use Time-To-Live instead of last access tracking.
* FallbackNamespaces added - allows you to configure a second namespace used for blob operations if blobs are missing from the first. This can be used to store Virtual Assets in a seperate store pool that have different GC rules.
* Added ability to use presigned urls when uploading / downloading blobs
* If using a nginx reverse proxy on the same host we can support redirecting filesystem reads to it (X-Accel).
* Fixed issue with compact binary packages not being serialized correctly

# 0.2.0
* Improved handling of cassandra timeouts during ref GC.
* Added deletion of invalid references during blob GC - this reduces the size for entries in the blob_index table.
* Using OpenTelemetry for tracing - still supports datadog traces but this can now be forwarded to any OpenTelemetry compatible service.
* Fixed issue in Azure Blob storage with namespace containing _
* Optimized content id resolving
* Fixed issue with crash on startup from background services for certain large configuration objects
* Fixed issues with content id when using CosmosDB

# 0.1.0
* First public release of UnrealCloudDDC

# Older releases
No detailed changelog provided for older releases.