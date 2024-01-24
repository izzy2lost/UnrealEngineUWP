import { ComboBox, DefaultButton, FontIcon, IComboBox, IComboBoxOption, IComboBoxStyles, Icon, Pivot, PivotItem, SelectableOptionMenuItemType, Spinner, SpinnerSize, Stack, Text, mergeStyleSets, mergeStyles } from "@fluentui/react";
import { useConst } from '@fluentui/react-hooks';
import { action, makeObservable, observable } from "mobx";
import { observer } from "mobx-react-lite";
import React, { useEffect, useState } from "react";
import { useSearchParams } from "react-router-dom";
import { GetTelemetryChartResponse, GetTelemetryMetricsResponse, GetTelemetryVariableResponse, GetTelemetryViewResponse } from "../../backend/Api";
import dashboard, { StatusColor } from "../../backend/Dashboard";
import { useWindowSize } from "../../base/utilities/hooks";
import { msecToElapsed } from "../../base/utilities/timeUtils";
import { getHordeStyling } from "../../styles/Styles";
import { Breadcrumbs } from "../Breadcrumbs";
import { TopNav } from "../TopNav";
import { TelemetryViewData, clearTelemetryViewMetrics, getTelemetryViewData, graphColors } from "./TelemetryData";
import { TelemetryLineRenderer } from "./TelemetryLineGraph";

const timeSelections: TimeSelection[] = [
   {
      text: "Past 1 Hour", key: "time_1_hour", minutes: 60
   },
   {
      text: "Past 2 Hours", key: "time_2_hours", minutes: 60 * 2
   },
   {
      text: "Past 4 Hours", key: "time_4_hours", minutes: 60 * 4
   },
   {
      text: "Past 1 Day", key: "time_1_day", minutes: 60 * 24
   },
   {
      text: "Past 2 Days", key: "time_2_days", minutes: 60 * 24 * 2
   },
   {
      text: "Past 1 Week", key: "time_1_week", minutes: 60 * 24 * 7
   },
   {
      text: "Past 2 Weeks", key: "time_2_weeks", minutes: 60 * 24 * 7 * 2
   }
]


type SearchState = {
   category?: string;
   variables?: string[];
   minutes?: number;
}

class MetricsHandler {
   constructor() {
      makeObservable(this);
   }

   subscribe() {
      if (this.updated) { }
   }

   subscribeToSearch() {
      if (this.searchUpdated) { }
   }

   async set(category: string) {

      if (!this.view || category === this._category) {
         return;
      }

      this._category = category;
      this.querying = true;
      this.setUpdated();

      this.metrics = await getTelemetryViewData(this.view, category, this.minDate, this.maxDate);

      this.querying = false;
      this.setUpdated();
   }

   getCharts() {
      if (!this.searchState.category) {
         return [];
      }

      const category = this.view?.categories.find(c => c.name === this.searchState.category);
      if (!category) {
         return [];
      }

      return category.charts;
   }

   getChartVariables(): GetTelemetryVariableResponse[] {

      const variables = this.view?.variables;

      if (!variables?.length || !this.metrics) {
         return [];
      }

      const charts = this.getCharts();
      const allMetrics = charts.map(c => this.getAllChartMetrics(c.name).map(m => m.metrics).flat()).flat();

      const vars = variables.filter(v => {

         if (!v.values.length) {
            return false;
         }

         return !!allMetrics.find(m => m.groupValues && m.groupValues[v.group] !== undefined);
      });

      return vars;
   }

   getChart(chartName: string): GetTelemetryChartResponse | undefined {
      const category = this.view?.categories.find(c => !!c.charts.find(ch => ch.name === chartName));
      return category?.charts.find(ch => ch.name === chartName);
   }

   getChartLegend(chartName: string): string[] {

      const chart = this.getChart(chartName);
      if (!chart) {
         return [];
      }

      const metrics = this.getFilteredChartMetrics(chartName);

      const legendSet = new Set<string>();

      metrics.forEach(metric => {

         metric.metrics.forEach(m => {
            legendSet.add(m.key)
         })
      })

      const legend = Array.from(legendSet).sort((a, b) => a.localeCompare(b));

      return legend;

   }

   private getAllChartMetrics(chartName: string): GetTelemetryMetricsResponse[] {

      if (!this.metrics) {
         return [];
      }

      const chart = this.getChart(chartName);
      if (!chart) {
         return [];
      }

      const cmetrics = new Set<string>(chart.metrics.map(cm => cm.metricId));

      let metrics = this.metrics.metrics.filter(m => {
         return cmetrics.has(m.metricId);
      })

      return metrics;

   }

   getFilteredChartMetrics(chartName: string, latest = false): GetTelemetryMetricsResponse[] {

      if (!this.metrics) {
         return [];
      }

      const chart = this.getChart(chartName);
      if (!chart) {
         return [];
      }

      const cmetrics = new Set<string>(chart.metrics.map(cm => cm.metricId));

      let metrics = this.metrics.metrics.filter(m => {
         return cmetrics.has(m.metricId);
      }).map(m => { return { ...m } as GetTelemetryMetricsResponse });

      if (latest) {
         const found = new Set<string>();

         metrics.forEach(metric => {
            metric.metrics = metric.metrics.filter(m => {
               if (found.has(m.key)) {
                  return false;
               }
               found.add(m.key);
               return true;
            })
         })

      }

      this.searchState.variables?.forEach(v => {

         const values = v.split(",");
         const group = values.shift()!;

         metrics.forEach(metric => {
            metric.metrics = metric.metrics.filter(m => {

               if (m.groupValues && m.groupValues[group]) {
                  if (values.indexOf(m.groupValues[group]) === -1) {
                     return false;
                  }
               }

               return true;
            })
         })

      })

      return metrics;

   }

   clear() {

      this._category = undefined;
      this.search = new URLSearchParams();
      this.searchState = {};
      this.view = undefined;
      this.initialized = false;
      clearTelemetryViewMetrics();
   }

   async initialize() {

      if (this.view || this.initialized) {
         return;
      }

      this.initialized = true;

      if (!this.allViews.length) {
         console.log("No telemetry views configured");
         return;
      }

      this.search = new URLSearchParams(window.location.search);
      this.searchState = this.stateFromSearch();

      this.view = this.allViews[0]

      if (!this.view || !this.view.categories.length) {
         console.error("No view or no categories");
         return;
      }

      const category = this.searchState.category ?? this.view.categories[0].name;

      await this.setCategory(category)

      if (!this.searchState?.variables?.length) {

         const vars = this.getChartVariables();
         vars.forEach(v => {
            if (v.defaults?.length) {
               this.setVariables(v.group, v.defaults);
            }
         })
      }

      this.setUpdated();
   }

   async setCategory(category?: string) {

      if (category?.length) {
         if (this.searchState.category !== category) {
            this.searchState.category = category;
            this.updateSearch();
         }

         await this.set(category)
      }
   }

   setVariables(group: string, values: string[]) {
      let newVars = this.searchState.variables?.filter(v => {
         const [vgroup,] = v.split(",");
         if (group === vgroup) {
            return false;
         }
         return true;
      })

      if (!newVars) {
         newVars = [];
      }

      if (values.length) {
         newVars.push(`${group},` + values.join(","));
      }

      newVars = newVars.sort((a, b) => a.localeCompare(b));
      this.searchState.variables = newVars.length ? newVars : undefined;

      this.updateSearch();
      this.setUpdated();

   }

   @action
   setUpdated() {
      this.updated++;
   }

   @action
   setSearchUpdated() {
      this.searchUpdated++;
   }

   updateSearch(): boolean {

      const state = { ...this.searchState } as SearchState;

      const search = new URLSearchParams();
      const csearch = this.search.toString();

      if (state.category?.length) {
         search.append("category", state.category);
      }

      if (state.minutes) {
         search.append("minutes", state.minutes.toString());
      }

      state.variables?.forEach(v => {
         search.append("v", v);
      });


      if (search.toString() !== csearch) {
         this.search = search;
         this.setSearchUpdated();
         return true;
      }

      return false;
   }

   stateFromSearch() {

      const state: SearchState = {};

      state.category = this.search.get("category") ?? undefined;
      state.variables = this.search.getAll("v") ?? undefined;
      let minutes = Number.parseInt(this.search.get("minutes") ?? "0")
      if (!minutes) {
         minutes = 240;
      }
      state.minutes = minutes;

      this.updateTime(minutes);

      return state;
   }

   updateTime(minutes: number) {


      this.minDate = new Date(new Date().valueOf() - (minutes * 60000));
      this.maxDate = new Date();
   }

   reload() {

      // force an update
      const category = this._category;
      this._category = undefined;
      this.searchState.category = undefined;
      clearTelemetryViewMetrics();
      this.setCategory(category);

   }

   setTimeSelection(time: TimeSelection) {

      if (this.searchState.minutes === time.minutes) {
         return;
      }

      this.searchState.minutes = time.minutes;
      this.updateTime(time.minutes);

      this.reload();
   }

   @observable
   private updated = 0;

   @observable
   private searchUpdated: number = 0;

   initialized = false;

   view?: GetTelemetryViewResponse;

   minDate: Date = new Date();
   maxDate: Date = new Date();

   querying = false;

   searchState: SearchState = {}

   search: URLSearchParams = new URLSearchParams();

   private get allViews() { return dashboard.telemetryViews }

   private metrics?: TelemetryViewData;

   private _category?: string;
}

const handler = new MetricsHandler();

const pivotClasses = mergeStyleSets({
   pivot: {
      selectors: {
         ".ms-Pivot-link": {
            lineHeight: "36px",
            height: "36px",
            paddingTop: 0,
            paddingBottom: 0
         }
      }
   }
});

const TelemetryPivot: React.FC = observer(() => {

   handler.subscribe();

   const view = handler.view;

   if (!view) {
      return null;
   }

   const links = view.categories.map(cat => {
      const key = `item_key_${cat.name}`;
      return <PivotItem headerText={cat.name} key={key} itemKey={key} />
   });

   return <Stack horizontal tokens={{ childrenGap: 18 }} verticalAlign="center" verticalFill={false}>
      <Pivot className={pivotClasses.pivot} defaultSelectedKey={`item_key_${handler.searchState.category}`} linkSize="normal" linkFormat="links" onLinkClick={(item, ev) => {
         if (item) {

            const catname = item.props.itemKey!.replace("item_key_", "");
            handler.setCategory(catname);
         }
      }}>
         {links}
      </Pivot>
   </Stack>;
})

let multiComboBoxId = 0;

const VariableChooser: React.FC<{ group: string, label?: string, multiSelect: boolean, optionsIn: IComboBoxOption[], initialKeysIn: string[], updateKeys: (group: string, selectedKeys: string[]) => void }> = ({ group, label, multiSelect, optionsIn, initialKeysIn, updateKeys }) => {

   const comboBoxRef = React.useRef<IComboBox>(null);

   let initialKeys = [...initialKeysIn];
   let options = [...optionsIn];

   if (options.length === initialKeys.length) {
      initialKeys.push('selectAll');
   }

   if (multiSelect && options.length > 3) {
      options.unshift({ key: 'selectAll', text: 'Select All', itemType: SelectableOptionMenuItemType.SelectAll });
   }

   const comboBoxStyles: Partial<IComboBoxStyles> = { root: { width: 270 } };

   return <ComboBox componentRef={comboBoxRef} key={`multi_option_${group}_${multiComboBoxId++}`} label={label} placeholder="None" defaultSelectedKey={initialKeys} multiSelect={multiSelect} options={options} onResolveOptions={() => options}
      onChange={multiSelect ? undefined : (event, option, index, value) => {
         setTimeout(() => { multiComboBoxId++; updateKeys(group, [value ?? ""]) }, 250);
      }}
      onMenuDismiss={!multiSelect ? undefined : () => {
         if (comboBoxRef?.current?.selectedOptions) {
            const selectedKeys = comboBoxRef.current.selectedOptions.map(o => o.key as string).filter(k => k !== 'selectAll');
            setTimeout(() => { multiComboBoxId++; updateKeys(group, selectedKeys) }, 250);
         }
      }} styles={comboBoxStyles} />
};


const TelemetryChooser: React.FC = observer(() => {

   handler.subscribe();

   const view = handler.view;

   if (!view) {
      return null;
   }

   const varStacks: JSX.Element[] = [];

   const vars = handler.getChartVariables();

   vars.forEach(v => {

      const options: IComboBoxOption[] = v.values.map(name => {
         return {
            key: name,
            text: name
         }
      })

      if (!options.length) {
         return;
      }

      let defaultKeys: string[] = options.map(o => o.key as string);

      const state = handler.searchState;
      if (state.variables?.length) {
         const sv = state.variables?.find(sv => sv.startsWith(`${v.group},`))
         if (sv) {
            defaultKeys = sv.split(",");
            defaultKeys.shift();
         }
      }

      const stack = <Stack key={`key_chooser_${v.group}`}>
         <VariableChooser
            group={v.group}
            label={v.name}
            multiSelect={true}
            optionsIn={options}
            initialKeysIn={defaultKeys}
            updateKeys={(group, keys) => {
               handler.setVariables(group, keys);
            }}
         />
      </Stack>

      varStacks.push(stack);

   })

   if (!varStacks.length) {
      return null;
   }

   return <Stack horizontal tokens={{ childrenGap: 24 }} verticalAlign="center" verticalFill={false}>
      {varStacks}
   </Stack>;
})

type TimeSelection = {
   text: string;
   key: string;
   minutes: number;
}

const TimeChooser: React.FC = observer(() => {

   handler.subscribe();

   let timeComboText: string | undefined;
   let timeComboWidth = 180;

   const key = timeSelections.find(t => t.minutes === handler.searchState.minutes)?.key;
   if (!key) {
      return null;
   }

   return <Stack>
      <ComboBox
         label="Time"
         styles={{ root: { width: timeComboWidth } }}
         options={timeSelections}
         text={timeComboText}
         selectedKey={key}
         onChange={(ev, option, index, value) => {
            const select = option as TimeSelection;
            handler.setTimeSelection(select);
         }}
      />
   </Stack>

})

const Legend: React.FC<{ chart: GetTelemetryChartResponse }> = observer(({ chart }) => {

   handler.subscribe();

   const legend = handler.getChartLegend(chart.name);

   const legendStacks: JSX.Element[] = [];

   legend.forEach((v, index) => {

      const filtered = false;

      const stack = <Stack key={`key_legend_${v}`} horizontal verticalAlign="center" tokens={{ childrenGap: 8 }} onClick={() => {
      }}>
         <Stack>
            <FontIcon style={{ color: filtered ? "#999999" : graphColors[index % graphColors.length], paddingTop: 2 }} iconName="Square" />
         </Stack>
         <Stack>
            <Text style={{ fontSize: "11px", color: filtered ? "#999999" : undefined }}>{v}</Text>
         </Stack>
      </Stack>

      legendStacks.push(stack)

   })

   return <Stack style={{ cursor: "pointer", width: 340, height: 300, overflowY: "auto" }} tokens={{ childrenGap: 4 }}>{legendStacks}</Stack>
})


const indicatorStyles = mergeStyleSets({

   stripes: {
      backgroundImage: 'repeating-linear-gradient(-45deg, rgba(255, 255, 255, .2) 25%, transparent 25%, transparent 50%, rgba(255, 255, 255, .2) 50%, rgba(255, 255, 255, .2) 75%, transparent 75%, transparent)',
   }

});

export type IndicatorBarStack = {
   value: number,
   title?: string,
   titleValue?: number,
   color?: string,
   onClick?: () => void,
   stripes?: boolean,
   brightness?: number
}

export const IndicatorBar: React.FC<{ stack: IndicatorBarStack[], width: number, height: number, basecolor?: string, style?: any }> = ({ stack, width, height, basecolor, style }) => {
   stack = stack.filter(s => s.value > 0);

   const mainTitle = stack.map((item) => {
      return item.titleValue === undefined ? `${item.value}% ${item.title}` : `${item.titleValue} ${item.title}`
   }).join(' ');


   return (
      <div className={mergeStyles({ backgroundColor: basecolor, width: width, height: height, verticalAlign: 'middle', display: "flex" }, style)} title={mainTitle}>
         {stack.map((item) => {
            const iwidth = width * (item.value / 100);
            return <span key={item.title!}
               onClick={item.onClick}
               className={item.stripes ? indicatorStyles.stripes : undefined}
               style={{
                  width: `${iwidth}px`, height: '100%',
                  backgroundColor: item.color,
                  margin: "1px",
                  display: 'block',
                  borderRadius: "2px",
                  cursor: item.onClick ? 'pointer' : 'inherit',
                  backgroundSize: `${height * 2}px ${height * 2}px`,
                  boxShadow: !item.brightness ? `0 0 3px ${item.color}` : undefined,
                  filter: item.brightness ? `brightness(${item.brightness})` : undefined
               }} />
         })}
      </div>
   );
}

const IndicatorTile: React.FC<{ chart: GetTelemetryChartResponse }> = observer(({ chart }) => {

   const { modeColors } = getHordeStyling();

   handler.subscribe();

   const metrics = handler.getFilteredChartMetrics(chart.name, true);

   if (!metrics?.length) {
      return <Text>No Data</Text>;
   }

   const colors = dashboard.getStatusColors();

   const allMetrics = metrics.map(m => m.metrics).flat().sort((a, b) => a.key.localeCompare(b.key));

   const elements: JSX.Element[] = [];

   allMetrics.forEach(m => {

      const barStack: IndicatorBarStack[] = [];

      const max = chart.max ?? 100;
      const threshold = m.threshold ?? max;


      const v = m.value / max;
      const t = threshold / max;
      for (let i = 0.0; i < 1; i += .1) {

         const color = i <= t ? colors.get(StatusColor.Success)! : colors.get(StatusColor.Failure)!;

         let brightness: number | undefined;
         if (i > v) {
            brightness = 0.4
         }
         barStack.push({ value: 10, color: color, brightness: brightness });
      }

      const element = <Stack horizontal verticalAlign="center">
         <Stack style={{ width: 340 }}>
            <Text variant="small">{m.key}</Text>
         </Stack>
         <Stack>
            <IndicatorBar stack={barStack} width={160} height={14} />
         </Stack>
         <Stack style={{ width: 90 }} horizontalAlign="end">
            <Text variant="small">{msecToElapsed(m.value * 1000)}</Text>
         </Stack>
      </Stack>

      elements.push(element)
   })

   return <Stack style={{ paddingTop: 12 }}>
      <Stack tokens={{ childrenGap: 6 }} style={{ backgroundColor: modeColors.background, padding: 18, height: 300, overflowY: "auto" }}>
         {elements}
      </Stack>
   </Stack>
})

const LineGraphTile: React.FC<{ chart: GetTelemetryChartResponse }> = observer(({ chart }) => {

   const [scale] = useState(1);
   const [container, setContainer] = useState<HTMLDivElement | null>(null);
   const renderer = useConst(chart.graph === "Line" ? new TelemetryLineRenderer() : new TelemetryLineRenderer());

   const { hordeClasses, modeColors } = getHordeStyling();

   handler.subscribe();

   const graph_container_id = `metric_graph_container_${chart.name}}`;

   const metrics = handler.getFilteredChartMetrics(chart.name);

   if (!metrics?.length) {
      return null;
   }

   const legend = handler.getChartLegend(chart.name);


   if (container) {
      try {
         renderer.render(chart, metrics, legend, handler.minDate!, handler.maxDate!, container, scale);

      } catch (err) {
         console.error(err);
      }
   }
   const width = 1024;

   return <Stack className={hordeClasses.horde} key={`metric_graph_stack_${chart.name}`}>
      <Stack style={{ width: "100%", paddingTop: 16, paddingBottom: 16, paddingLeft: 16, backgroundColor: modeColors.background }} horizontal tokens={{ childrenGap: 12 }}>
         <Stack style={{ width: width }}>
            <div id={graph_container_id} style={{ shapeRendering: "geometricPrecision", userSelect: "none" }} ref={(ref: HTMLDivElement) => setContainer(ref)} onMouseEnter={() => { }} onMouseLeave={() => { }} />
         </Stack>
         <Stack>
            <Legend chart={chart} />
         </Stack>
      </Stack>

   </Stack>;
})

const TelemetryViewInternal: React.FC = observer(() => {

   const { hordeClasses } = getHordeStyling();

   handler.subscribe();

   if (handler.querying) {
      return <Stack>
         <Text>Loading Data</Text>
         <Spinner size={SpinnerSize.large} />
      </Stack>
   }

   const charts = handler.getCharts();

   const indicators = charts.filter(c => c.graph === "Indicator");
   const lines = charts.filter(c => c.graph === "Line");

   const ipanels = indicators.map(chart => {
      return <Stack key={`telemetry_view_panel_${chart.name}`} styles={{ root: { paddingTop: 0, paddingRight: 12, width: 720 } }}>
         <Stack className={hordeClasses.raised} >
            <Stack tokens={{ childrenGap: 12 }} grow>
               <Stack >
                  <Stack >
                     <Text>{chart.name}</Text>
                  </Stack>
                  <Stack>
                     <IndicatorTile chart={chart} />
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   })

   const linepanels = lines.map(chart => {
      return <Stack key={`telemetry_view_panel_${chart.name}`} styles={{ root: { paddingTop: 0, paddingRight: 12 } }}>
         <Stack className={hordeClasses.raised} >
            <Stack tokens={{ childrenGap: 12 }} grow>
               <Stack >
                  <Stack >
                     <Text>{chart.name}</Text>
                  </Stack>
                  <Stack>
                     <LineGraphTile chart={chart} />
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   })

   return <Stack tokens={{ childrenGap: 12 }}>
      <Stack horizontal>
         {ipanels}
      </Stack>
      {linepanels}
   </Stack>
})

export const SearchUpdate: React.FC = observer(() => {

   const [, setSearchParams] = useSearchParams();

   const csearch = handler.search.toString();

   useEffect(() => {
      setSearchParams(csearch, { replace: true });
   }, [csearch, setSearchParams])

   // subscribe
   handler.subscribeToSearch();

   return null;
});

export const TelemetryView: React.FC = () => {

   handler.initialize();

   useEffect(() => {
      return () => {
         handler.clear();
      };
   }, []);

   const windowSize = useWindowSize();

   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);

   const { hordeClasses, modeColors } = getHordeStyling();

   const rootWidth = 1440;
   const centerAlign = vw / 2 - 720 /*890*/;
   const key = `windowsize_metrics_view_${windowSize.width}_${windowSize.height}`;

   return <Stack className={hordeClasses.horde} key="key_metrics_graph_test">
      <SearchUpdate />
      <TopNav />
      <Breadcrumbs items={[{ text: 'Telemetry' }]} />
      <Stack horizontal styles={{ root: { backgroundColor: modeColors.background } }}>
         <Stack styles={{ root: { width: "100%" } }}>
            <Stack horizontal>
               <Stack key={`${key}_1`} style={{ paddingLeft: centerAlign }} />
               <Stack style={{ width: rootWidth - 8, maxWidth: windowSize.width - 12, paddingLeft: 0, paddingTop: 24, paddingBottom: 24, paddingRight: 0 }} >
                  <Stack>
                     <TelemetryPivot />
                  </Stack>
                  <Stack horizontal>
                     <Stack style={{ paddingLeft: 32, paddingTop: 12 }}>
                        <TelemetryChooser />
                     </Stack>
                     <Stack grow />
                     <Stack horizontal style={{ paddingTop: 12 }} tokens={{ childrenGap: 18 }}>
                        <Stack >
                           <TimeChooser />
                        </Stack>
                        <Stack style={{paddingTop: 27}}>
                           <DefaultButton style={{minWidth: 52, height: 34}} onClick={() => handler.reload()}>
                              <Icon
                                 iconName='Refresh'
                              />
                           </DefaultButton>

                        </Stack>
                     </Stack>
                  </Stack>
               </Stack>
            </Stack>
            <Stack style={{ width: "100%", backgroundColor: modeColors.background }}>
               <Stack horizontal>
                  <Stack /*className={classNames.pointerSuppress} */ style={{ position: "relative", width: "100%", height: 'calc(100vh - 228px)' }}>
                     <div id="hordeContentArea" style={{ overflowX: "auto", overflowY: "visible" }}>
                        <Stack horizontal style={{ paddingBottom: 48 }}>
                           <Stack key={`${key}_2`} style={{ paddingLeft: centerAlign }} />
                           <Stack style={{ width: rootWidth }} tokens={{ childrenGap: 12 }}>
                              <TelemetryViewInternal />
                           </Stack>
                        </Stack>
                     </div>
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   </Stack>
}