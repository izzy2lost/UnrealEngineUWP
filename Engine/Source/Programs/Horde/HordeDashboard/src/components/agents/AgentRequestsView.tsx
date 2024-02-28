// Copyright Epic Games, Inc. All Rights Reserved.

import { DetailsList, DetailsListLayoutMode, IColumn, SelectionZone, PrimaryButton, Selection, SelectionMode, Stack, Text, DefaultButton, Dialog, DialogFooter, DialogType, Modal, Spinner, SpinnerSize } from "@fluentui/react";
import { observer } from "mobx-react-lite";
import React, { useEffect, useState } from "react";
import backend from "../../backend";
import { GetPendingAgentResponse } from "../../backend/Api";
import { PollBase } from "../../backend/PollBase";
import { useWindowSize } from "../../base/utilities/hooks";
import { getHordeStyling } from "../../styles/Styles";
import { Breadcrumbs } from "../Breadcrumbs";
import { TopNav } from "../TopNav";
import ErrorHandler from "../ErrorHandler";

class AgentRequestsHandler extends PollBase {

   constructor(pollTime = 5000) {

      super(pollTime);

   }

   clear() {
      this.initial = true;
      this.requests = [];
      this.selectedAgents = [];
      this.selection = new Selection({ onSelectionChanged: () => { this.onSelectionChanged(this.selection.getSelection() as any) }, selectionMode: SelectionMode.multiple })
      super.stop();
   }

   async poll(): Promise<void> {

      try {

         const requests = await backend.getAgentRegistrationRequests();
         this.requests = requests.agents;
         this.initial = false;

         this.requests = [{ key: "one", hostName: "one", description: "one" }, { key: "two", hostName: "two", description: "two" }, { key: "three", hostName: "three", description: "three" }]
         this.setUpdated();

      } catch (err) {

      }
   }

   onSelectionChanged(selection: GetPendingAgentResponse[] | undefined) {
      this.selectedAgents = selection ?? [];
      this.setUpdated();
   }

   selection = new Selection({ onSelectionChanged: () => { this.onSelectionChanged(this.selection.getSelection() as any) }, selectionMode: SelectionMode.multiple })

   selectedAgents: GetPendingAgentResponse[] = [];

   requests: GetPendingAgentResponse[] = [];

   initial = true;
}

const handler = new AgentRequestsHandler();

const AgentsPanel: React.FC = observer(() => {

   const [confirmRegister, setConfirmRegister] = useState(false);
   const [submitting, setSubmitting] = useState(false);

   useEffect(() => {

      handler.start();

      return () => {
         handler.clear();
      };

   }, []);

   const { modeColors } = getHordeStyling();

   // subscribe
   if (handler.updated) { };

   const onRegister = async () => {

      const agents = handler.selectedAgents.map(a => { return { key: a.key } })

      try {
         setSubmitting(true);
         await backend.registerAgents({ agents: agents });
         setSubmitting(false);
         handler.stop();
         handler.clear();
         handler.start();
      } catch (reason) {
         console.error(reason);
         
         ErrorHandler.set({
            reason: reason,
            title: `Error Registering Agents`,
            message: `There was an error registering agents, reason: "${reason}"`

         }, true);

         setSubmitting(false);
      }
   }

   const columns = [
      { key: 'column_hostname', name: 'Hostname', fieldName: 'hostName', minWidth: 120, maxWidth: 120, isResizable: false },
      { key: 'column_description', name: 'Description', fieldName: 'description', minWidth: 120, maxWidth: 120, isResizable: false },
   ];

   let requests = [...handler.requests];

   requests = requests.sort((a, b) => a.hostName.localeCompare(b.hostName));

   const renderItem = (item: any, index?: number, column?: IColumn) => {
      if (!column?.fieldName) {
         return null;
      }
      return <Text style={{ color: modeColors.text }}>{item[column?.fieldName]}</Text>
   };

   return (<Stack>
      {submitting && <Modal isOpen={true} isBlocking={true} topOffsetFixed={true} styles={{ main: { padding: 8, width: 400, hasBeenOpened: false, top: "120px", position: "absolute" } }} >
         <Stack style={{ paddingTop: 32 }}>
            <Stack tokens={{ childrenGap: 24 }} styles={{ root: { padding: 8 } }}>
               <Stack horizontalAlign="center">
                  <Text variant="mediumPlus">Please wait...</Text>
               </Stack>
               <Stack verticalAlign="center" style={{ paddingBottom: 32 }}>
                  <Spinner size={SpinnerSize.large} />
               </Stack>
            </Stack>
         </Stack>
      </Modal>}
      {confirmRegister &&
         <Dialog
            hidden={false}
            onDismiss={() => setConfirmRegister(false)}
            minWidth={612}
            dialogContentProps={{
               type: DialogType.normal,
               title: `Register Agents`,
               subText: `Confirm registering agents: ${handler.selectedAgents.map(a => a.hostName).join(", ")}`
            }}
            modalProps={{ isBlocking: true, topOffsetFixed: true, styles: { main: { padding: 8, width: 400, hasBeenOpened: false, top: "120px", position: "absolute" } } }} >            
            <Stack style={{ height: "18px" }} />
            <DialogFooter>
               <PrimaryButton onClick={() => { setConfirmRegister(false); onRegister() }} text="Register" />
               <DefaultButton onClick={() => setConfirmRegister(false)} text="Cancel" />
            </DialogFooter>
         </Dialog>
      }
      <Stack styles={{ root: { paddingTop: 18, paddingLeft: 12, paddingRight: 12, width: "100%" } }} >
         <Stack tokens={{ childrenGap: 12 }}>
            <Stack horizontal horizontalAlign="center">
               <Stack>
                  <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>{requests.length || handler.initial ? "Agent Requests" : "Agent Requests (None)"}</Text>
               </Stack>
               <Stack grow />
               <Stack>
                  <PrimaryButton disabled={!handler.selectedAgents.length} styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }} onClick={() => setConfirmRegister(true)}>Register Agents</PrimaryButton>
               </Stack>
            </Stack>

            <div style={{ overflowY: 'auto', overflowX: 'hidden', maxHeight: "calc(100vh - 312px)" }} data-is-scrollable={true}>
               {!!requests.length && <Stack>
                  <SelectionZone selection={handler.selection}>
                     <DetailsList
                        setKey="set"
                        items={requests}
                        columns={columns}
                        layoutMode={DetailsListLayoutMode.justified}
                        compact={true}
                        selectionMode={SelectionMode.multiple}
                        selection={handler.selection}
                        selectionPreservedOnEmptyClick={true}
                        onRenderItemColumn={renderItem}
                        enableUpdateAnimations={false}
                        onShouldVirtualize={() => false}
                     />
                  </SelectionZone>
               </Stack>}
            </div>
         </Stack>
      </Stack>
   </Stack>);
});


export const AgentRequestsView: React.FC = () => {

   const windowSize = useWindowSize();
   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);

   const { hordeClasses, modeColors } = getHordeStyling();

   return <Stack className={hordeClasses.horde}>
      <TopNav />
      <Breadcrumbs items={[{ text: 'Agents', link: "/agents" }, { text: 'Registration' }]} />
      <Stack horizontal>
         <div key={`windowsize_streamview_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - (1440 / 2), flexShrink: 0, backgroundColor: modeColors.background }} />
         <Stack tokens={{ childrenGap: 0 }} styles={{ root: { backgroundColor: modeColors.background, width: "100%" } }}>
            <Stack style={{ maxWidth: 1440, paddingTop: 6, marginLeft: 4, height: 'calc(100vh - 8px)' }}>
               <Stack horizontal className={hordeClasses.raised}>
                  <Stack style={{ width: "100%", height: 'calc(100vh - 228px)' }} tokens={{ childrenGap: 18 }}>
                     <AgentsPanel />
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   </Stack>
};

