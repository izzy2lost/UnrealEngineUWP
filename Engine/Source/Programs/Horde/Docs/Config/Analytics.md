[Horde](../../README.md) > [Configuration](../Config.md) > Analytics

# Analytics

Horde implements HTTP endpoints to collect telemetry data sent by Unreal Editor. This data can
provide insights into bottlenecks and workflow issues that a team and the Horde dashboard can aggregate and
chart it to highlight improvements and regressions over time.

## Configuring the Editor

To configure the editor to send analytics data to Horde, add the following lines to the
`{{ PROJECT_DIR }}/Config/DefaultEngine.ini` file and submit it to source control.

    [StudioTelemetry.Horde]
    Name=HordeStudioAnalytics
    ProviderType=FAnalyticsProviderET
    UsageType=EditorAndClient
    APIKeyET=HordeAnalytics.Dev
    APIServerET="{{ HORDE_SERVER_URL }}"
    APIEndpointET="api/v1/telemetry"

## Telemetry Sinks

Horde can collect telemetry data in its own database and forward it to other telemetry sinks.

You can configure telemetry sinks through the `Telemetry` property in the server's
[Server.json](../Deployment/ServerSettings.md) file.

## Metrics and Aggregation

To provide efficient aggregation of analytics data over large time periods, Horde aggregates telemetry events into
running metrics for each time interval. This aggregation is performed according to rules specified in the
`Telemetry.Metrics` section of the globals.json file (see [MetricConfig](Schema/Telemetry.md#metricconfig)).

## Charting

The Horde dashboard can render charts showing aggregated metrics collected on the server. These views are configured
using the `Dashboard.Analytics` section of the globals.json file (see [TelemetryViewConfig](Schema/Dashboard.md#telemetryviewconfig)).
