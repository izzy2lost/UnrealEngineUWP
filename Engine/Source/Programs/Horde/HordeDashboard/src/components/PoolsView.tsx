import { DefaultButton, DetailsHeader, DetailsList, IColumn, IDetailsHeaderStyles, IDetailsListProps, ITag, ScrollablePane, ScrollbarVisibility, SelectionMode, Spinner, SpinnerSize, Stack, Sticky, StickyPositionType, TagPicker, Text } from "@fluentui/react";
import { observer } from "mobx-react-lite";
import React, { useEffect, useState } from "react";
import { useNavigate, useSearchParams } from "react-router-dom";
import backend from "../backend";
import { GetAgentResponse, GetPoolResponse } from "../backend/Api";
import { PollBase } from "../backend/PollBase";
import { useWindowSize } from "../base/utilities/hooks";
import { getHordeStyling } from "../styles/Styles";
import { BreadcrumbItem, Breadcrumbs } from "./Breadcrumbs";
import { PoolView } from "./PoolView";
import { TopNav } from "./TopNav";

type PoolData = GetPoolResponse & {
   numAgents?: number;
}

class PoolsHandler extends PollBase {

   constructor(pollTime = 30000) {

      super(pollTime);

   }

   clear() {
      this.loaded = false;
      this.pools = [];
      this.agents = [];
      this.poolLookup.clear();
      super.stop();
   }

   setFilter(filter: string) {
      this.filter = filter;
      this.setUpdated();
   }

   countAgents() {

      handler.agents.forEach(a => {
         a.pools?.forEach(p => {                  
            const pool = handler.poolLookup.get(p);
            if (pool) {
               if (pool.numAgents) {
                  pool.numAgents += 1;
               } else {
                  pool.numAgents = 1;
               }
            }
         })
      })
   
   }

   async loadAgents() {

      if (this.agents.length) {
         return;
      }

      this.agents = await backend.getAgents({ filter: "id,name,pools", includeDeleted: false });
      this.countAgents();

      this.setUpdated();

   }

   async poll(): Promise<void> {

      try {

         this.pools = await backend.getPools("id,name,colorValue");
         this.pools.forEach(p => {
            this.poolLookup.set(p.id, p);
         })

         this.countAgents();

         this.loaded = true;
         this.setUpdated();

      } catch (err) {

      }

   }

   filter = "";

   loaded = false;

   agents: GetAgentResponse[] = [];
   pools: PoolData[] = [];
   poolLookup = new Map<string, PoolData>();
}

const handler = new PoolsHandler();

const PoolList: React.FC = observer(() => {

   const [sortState, setSortState] = useState<{ sortBy?: string, sortDescend?: boolean }>({ sortBy: "Pool" });

   const navigate = useNavigate();

   handler.subscribe();

   handler.loadAgents();

   const columns: IColumn[] = [{
      key: 'column1',
      name: 'Pool',
      isSorted: sortState.sortBy === "Pool",
      isSortedDescending: sortState.sortDescend,
      minWidth: 320,
      maxWidth: 320,
      onRender: (pool: PoolData) => {
         const textColor = "white";
         const color = pool.colorValue;
         return <Stack verticalAlign="center" verticalFill>
            <DefaultButton
               text={pool.name}
               primary
               onClick={(ev) => { ev.preventDefault(); ev.stopPropagation(); navigate(`?pool=${pool.id}`); handler.setUpdated() }}
               styles={{
                  root: {
                     height: '26px',
                     width: "max-content",
                     font: '8pt Horde Open Sans SemiBold !important',
                     flexShrink: '0 !important',
                     paddingLeft: 6,
                     paddingRight: 6,
                     border: '0px', backgroundColor: color, color: textColor
                  },
                  rootHovered: { border: '0px', backgroundColor: color, color: textColor, },
                  rootPressed: { border: '0px', backgroundColor: color, color: textColor, }
               }} />
         </Stack>
      }
   },
   {
      key: 'column2',
      name: 'Agents',
      minWidth: 120,
      maxWidth: 120,
      isSorted: sortState.sortBy === "Agents",
      isSortedDescending: sortState.sortDescend,
      onRenderHeader: () => {
         if (!handler.agents.length) {
            return <Stack horizontal tokens={{childrenGap: 12}}><Text style={{ fontWeight: 600 }}>Agents</Text><Spinner size={SpinnerSize.medium} /></Stack>
         } else {
            return <Text style={{ fontWeight: 600 }}>Agents</Text>
         }
      },
      onRender: (pool: PoolData) => {

         return <Stack horizontalAlign="start" verticalAlign="center" verticalFill>
            <Text>{pool.numAgents ?? (handler.agents.length ? "0" : "")}</Text>
         </Stack>;
      }
   },
   {
      key: 'column3',
      name: 'Hidden',
      minWidth: 120,
      onRenderHeader: () => {
         return null;
      },
      onRender: () => {
         return null;
      }
   }];



   const onRenderDetailsHeader: IDetailsListProps['onRenderDetailsHeader'] = (props) => {
      const customStyles: Partial<IDetailsHeaderStyles> = {};
      if (props) {
         return (
            <Sticky stickyPosition={StickyPositionType.Header} isScrollSynced={true}>
               <DetailsHeader {...props} styles={customStyles} onColumnClick={(ev: React.MouseEvent<HTMLElement>, column: IColumn) => {
                  if (column.name === "Agents") {
                     setSortState({ sortBy: "Agents", sortDescend: sortState.sortBy === "Agents" && !sortState.sortDescend })
                  } else if (column.name === "Pool") {
                     setSortState({ sortBy: "Pool", sortDescend: sortState.sortBy === "Pool" && !sortState.sortDescend })
                  }
               }} />
            </Sticky>
         );
      }
      return null;
   };


   const filter = handler.filter.toLowerCase();
   const filtered = handler.pools.filter(p => {
      if (!filter) {
         return true;
      }
      return p.name.toLowerCase().indexOf(filter) !== -1
   })

   let items = filtered;

   if (sortState.sortBy) {

      items = items.sort((a, b) => {

         if (sortState.sortBy === "Pool" || a.numAgents === b.numAgents) {
            return a.name.localeCompare(b.name);
         }

         return (a.numAgents ?? 0) - (b.numAgents ?? 0);

      })

      if (sortState.sortDescend) {
         items = items.reverse();
      }

   }

   return <Stack style={{ height: "calc(100vh - 280px)", position: "relative" }}>
      <ScrollablePane scrollbarVisibility={ScrollbarVisibility.auto}>
         <DetailsList
            compact
            selectionMode={SelectionMode.none}
            items={items}
            columns={columns}
            isHeaderVisible={true}
            onRenderDetailsHeader={onRenderDetailsHeader}
         />
      </ScrollablePane>
   </Stack>
})

const PoolPicker: React.FC = observer(() => {

   const [searchParams, setSearchParams] = useSearchParams();

   const poolId = searchParams.get("pool") ?? "";

   handler.subscribe();

   const poolTags: ITag[] = handler.pools.sort((a, b) => a.name.localeCompare(b.name)).map(p => {
      return { key: p.id, name: p.name }
   });

   let selectedItems: ITag[] = [];
   if (poolId) {
      const d = poolTags.find(t => t.key === poolId);
      if (d) {
         selectedItems = [d];
      }
   }

   const listContainsTagList = (tag: ITag, tagList?: ITag[]) => {
      if (!tagList || !tagList.length || tagList.length === 0) {
         return false;
      }
      return tagList.some(compareTag => compareTag.key === tag.key);
   };

   const filterSuggestedTags = (filterText: string, tagList?: ITag[]): ITag[] => {
      handler.setFilter(filterText);
      return filterText
         ? poolTags.filter(
            tag => tag.name.toLowerCase().indexOf(filterText.toLowerCase()) !== -1 && !listContainsTagList(tag, tagList),
         )
         : poolTags;
   };

   const getTextFromItem = (item: ITag) => item.name;

   return <Stack horizontal style={{ paddingTop: 8, paddingBottom: 12 }}>
      <Stack grow />
      <Stack style={{ width: 320 }}>
         <TagPicker inputProps={{ placeholder: "Filter" }}
            selectedItems={selectedItems}
            onResolveSuggestions={filterSuggestedTags}
            getTextFromItem={getTextFromItem}
            onEmptyResolveSuggestions={(selected) => {
               return poolTags;
            }}

            onChange={(items) => {
               if (!items?.length) {                  
                  setSearchParams("");
                  handler.setFilter("");
               }
            }}

            onDismiss={() =>  false}

            onItemSelected={(item) => {

               if (!item?.key) {
                  return null;
               }

               handler.setFilter("");
               setSearchParams(`?pool=${item.key}`, { replace: true });

               return item;

            }}

            itemLimit={1} />
      </Stack>
   </Stack>
})

export const PoolsView: React.FC = observer(() => {

   const [searchParams] = useSearchParams();

   const windowSize = useWindowSize();

   useEffect(() => {

      handler.start();

      return () => {
         handler.clear();
      };

   }, []);

   handler.subscribe();

   const poolId = searchParams.get("pool") ?? "";

   const { hordeClasses, modeColors } = getHordeStyling();
   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);

   let crumbs: BreadcrumbItem[] = [{
      text: "Pools",
      link: poolId ? "/pools" : undefined
   }];

   if (poolId) {
      const pool = handler.pools.find(p => p.id === poolId);
      if (pool) {
         crumbs.push({
            text: pool.name
         })
      }
   }

   return <Stack className={hordeClasses.horde}>
      <TopNav />
      <Breadcrumbs items={crumbs} />
      <Stack horizontal>
         <div key={`windowsize_streamview_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - (1440 / 2), flexShrink: 0, backgroundColor: modeColors.background }} />
         <Stack tokens={{ childrenGap: 0 }} styles={{ root: { backgroundColor: modeColors.background, width: "100%" } }}>
            <Stack style={{ maxWidth: 1440, paddingTop: 6, marginLeft: 4, height: 'calc(100vh - 8px)' }}>
               <Stack horizontal className={hordeClasses.raised}>
                  <Stack style={{ width: "100%", height: 'calc(100vh - 228px)' }} tokens={{ childrenGap: 18 }}>
                     <Stack>
                        {<Stack horizontal>
                           <Stack grow />
                           <PoolPicker />
                        </Stack>}
                        {!poolId && !handler.loaded && <Stack>
                           <Spinner size={SpinnerSize.large} />
                        </Stack>}
                        {!poolId && handler.loaded && <Stack>
                           <PoolList />
                        </Stack>}
                        {!!poolId && <Stack>
                           <PoolView pools={handler.pools} />
                        </Stack>}
                     </Stack>
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   </Stack>
});