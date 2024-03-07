// Copyright Epic Games, Inc. All Rights Reserved.

import { DetailsList, DetailsListLayoutMode, IColumn, PrimaryButton, SelectionMode, Stack, Text } from "@fluentui/react";
import { observer } from "mobx-react-lite";
import React, { useEffect } from "react";
import backend from "../backend";
import { GetToolSummaryResponse } from "../backend/Api";
import { PollBase } from "../backend/PollBase";
import { useWindowSize } from "../base/utilities/hooks";
import { Breadcrumbs } from "./Breadcrumbs";
import { TopNav } from "./TopNav";
import { getHordeStyling } from "../styles/Styles";

class ToolHandler extends PollBase {

   constructor(pollTime = 30000) {

      super(pollTime);

   }

   clear() {
      this.loaded = false;
      super.stop();
   }

   async poll(): Promise<void> {

      try {

         this.tools = await backend.getTools();
         this.tools = this.tools.filter(t => t.showInDashboard);
         this.loaded = true;
         this.setUpdated();

      } catch (err) {

      }

   }
   loaded = false;
   tools: GetToolSummaryResponse[] = [];
}

const handler = new ToolHandler();

const ToolPanel: React.FC = observer(() => {

   useEffect(() => {

      handler.start();

      return () => {
         handler.clear();
      };

   }, []);

   const { hordeClasses, modeColors } = getHordeStyling();

   // subscribe
   if (handler.updated) { };

   const columns: IColumn[] = [
      { key: 'column_name', name: 'Name', minWidth: 240, maxWidth: 240, isResizable: false },
      { key: 'column_desc', name: 'Description', fieldName: 'description', minWidth: 580, maxWidth: 580, isResizable: false, isMultiline: true },
      { key: 'column_version', name: 'Version', fieldName: 'version', minWidth: 280, maxWidth: 280, isResizable: false, headerClassName: hordeClasses.detailsHeader },
      { key: 'column_download', name: '', minWidth: 160, maxWidth: 160, isResizable: false }
   ];

   let tools = [...handler.tools];

   tools = tools.sort((a, b) => a.name.localeCompare(b.name));

   const renderItem = (item: any, index?: number, column?: IColumn) => {

      if (!column) {
         return null;
      }

      if (column.key === 'column_name') {
         return <Stack verticalAlign="center" verticalFill={true}>
            <Text style={{ fontFamily: "Horde Open Sans SemiBold", color: modeColors.text }}>{item.name}</Text>
         </Stack>
      }

      if (column.key === 'column_version') {
         if (!item.version) {
            return null;
         }
         return <Stack horizontalAlign="center" verticalAlign="center" verticalFill={true}>
            <Text style={{ color: modeColors.text }}>{item.version}</Text>
         </Stack>
      }


      if (column.key === 'column_download') {
         return <Stack horizontalAlign="center" verticalAlign="center" verticalFill={true}>
            <PrimaryButton style={{ width: 120, color: "#FFFFFF" }} text="Download" href={`/api/v1/tools/${item.id}?action=download`} />
         </Stack>
      }

      if (!column?.fieldName) {
         return null;
      }
      return <Stack verticalAlign="center" verticalFill={true}>
         <Text style={{ color: modeColors.text }}>{item[column?.fieldName]}</Text>
      </Stack>
   };

   return <Stack>
      {!tools.length && handler.loaded && <Stack style={{ paddingBottom: 12 }}>
         <Stack verticalAlign="center">
            <Stack horizontalAlign="center">
               <Text variant="mediumPlus">No Tools Found</Text>
            </Stack>
         </Stack>
      </Stack>}

      {!!tools.length && <Stack className={hordeClasses.raised} >
         <Stack styles={{ root: { paddingLeft: 12, paddingRight: 12, paddingBottom: 12, width: "100%" } }} >
            <DetailsList
               isHeaderVisible={true}
               items={tools}
               columns={columns}
               selectionMode={SelectionMode.none}
               layoutMode={DetailsListLayoutMode.justified}
               compact={false}
               onRenderItemColumn={renderItem}
            />
         </Stack>
      </Stack>}
   </Stack>
});


export const ToolView: React.FC = () => {

   const windowSize = useWindowSize();
   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);
   const centerAlign = vw / 2 - 720;

   const { hordeClasses, modeColors } = getHordeStyling();

   const key = `windowsize_view_${windowSize.width}_${windowSize.height}`;

   return <Stack className={hordeClasses.horde}>
      <TopNav />
      <Breadcrumbs items={[{ text: 'Tools' }]} />
      <Stack styles={{ root: { width: "100%", backgroundColor: modeColors.background } }}>
         <Stack style={{ width: "100%", backgroundColor: modeColors.background }}>
            <Stack style={{ position: "relative", width: "100%", height: 'calc(100vh - 148px)' }}>
               <div style={{ overflowX: "auto", overflowY: "visible" }}>
                  <Stack horizontal style={{ paddingTop: 30, paddingBottom: 48 }}>
                     <Stack key={`${key}`} style={{ paddingLeft: centerAlign }} />
                     <Stack style={{ width: 1440 }}>
                        <ToolPanel />
                     </Stack>
                  </Stack>
               </div>
            </Stack>
         </Stack>
      </Stack>
   </Stack>
};

