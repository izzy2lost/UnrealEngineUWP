[Horde](../../../README.md) > [Configuration](../../Config.md) > *.telemetry.json

# *.telemetry.json

Config for metrics

Name | Description
---- | -----------
`id` | `string`<br>Identifier for this store
`acl` | [`AclConfig`](#aclconfig)<br>Permissions for this store
`metrics` | [`MetricConfig`](#metricconfig)`[]`<br>Metrics to aggregate on the Horde server
`include` | [`ConfigInclude`](#configinclude)`[]`<br>Includes for other configuration files
`macros` | [`ConfigMacro`](#configmacro)`[]`<br>Macros within this configuration

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

## AclClaimConfig

Name | Description
---- | -----------
`type` | `string`<br>
`value` | `string`<br>

## AclProfileConfig

Name | Description
---- | -----------
`id` | `string`<br>
`actions` | `string[]`<br>
`excludeActions` | `string[]`<br>
`extends` | `string[]`<br>

## MetricConfig

Configures a metric to aggregate on the server

Name | Description
---- | -----------
`id` | `string`<br>Identifier for this metric
`filter` | `string`<br>Filter expression to evaluate to determine which events to include. This query is evaluated against an array.
`property` | `string`<br>Property to aggregate
`groupBy` | `string`<br>Property to group by. Specified as a comma-separated list of JSON path expressions.
`function` | [`AggregationFunction`](#aggregationfunction-enum)<br>How to aggregate samples for this metric
`percentile` | `integer`<br>For the percentile function, specifies the percentile to measure
`interval` | `string`<br>Interval for each metric. Supports times such as "2d", "1h", "1h30m", "20s".

## AggregationFunction (Enum)

Method for aggregating samples into a metric

Name | Description
---- | -----------
`Count` | Count the number of matching elements
`Min` | Take the minimum value of all samples
`Max` | Take the maximum value of all samples
`Sum` | Sum all the reported values
`Average` | Average all the samples
`Percentile` | Estimates the value at a certain percentile

## ConfigInclude

Name | Description
---- | -----------
`path` | `string`<br>

## ConfigMacro

Name | Description
---- | -----------
`name` | `string`<br>
`value` | `string`<br>
