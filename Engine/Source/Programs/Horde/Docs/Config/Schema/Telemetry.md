[Horde](../../../README.md) > [Configuration](../../Config.md) > *.telemetry.json

# *.telemetry.json

Name | Description
---- | -----------
`id` | `string`<br>
`acl` | [`AclConfig`](#aclconfig)<br>
`metrics` | [`MetricConfig`](#metricconfig)`[]`<br>
`include` | [`ConfigInclude`](#configinclude)`[]`<br>
`macros` | [`ConfigMacro`](#configmacro)`[]`<br>

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

Name | Description
---- | -----------
`id` | `string`<br>
`filter` | `string`<br>
`property` | `string`<br>
`groupBy` | `string`<br>
`function` | [`AggregationFunction`](#aggregationfunction-enum)<br>
`percentile` | `integer`<br>
`interval` | `string`<br>

## AggregationFunction (Enum)

Name | Description
---- | -----------
`Count` | 
`Min` | 
`Max` | 
`Sum` | 
`Average` | 
`Percentile` | 

## ConfigInclude

Name | Description
---- | -----------
`path` | `string`<br>

## ConfigMacro

Name | Description
---- | -----------
`name` | `string`<br>
`value` | `string`<br>
