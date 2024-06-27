import { ComboBox, DefaultButton, DetailsList, DetailsListLayoutMode, IComboBox, IComboBoxOption, IContextualMenuItem, IContextualMenuProps, IDetailsListProps, ITextField, IconButton, Label, Modal, PrimaryButton, ScrollablePane, ScrollbarVisibility, SelectionMode, Spinner, SpinnerSize, Stack, Text, TextField } from "@fluentui/react";
import { getHordeStyling } from "../../styles/Styles";
import { IStreamChooser, StreamChooser } from "../projects/StreamChooser";
import React, { useState } from "react";
import backend from "../../backend";
import { GetArtifactResponse } from "../../backend/Api";
import dashboard from "../../backend/Dashboard";
import { JobArtifactsModal } from "./ArtifactsModal";
import { projectStore } from "../../backend/ProjectStore";


const ArtifactsList: React.FC<{ artifacts?: GetArtifactResponse[] }> = ({ artifacts }) => {

   const [browse, setBrowse] = useState<{ jobId?: string, stepId?: string, artifactId?: string, artifact?: GetArtifactResponse }>({});

   if (!artifacts?.length) {
      return null;
   }

   const sorted = artifacts.filter(a => {
      return !!a.keys.find(k => k.startsWith("job:") && k.indexOf("/step:") !== -1)
   }).sort((a, b) => {

      if (a.name !== b.name) {
         return a.name.localeCompare(b.name);
      }

      if (a.streamId !== b.streamId) {
         return a.streamId!.localeCompare(b.streamId!);
      }

      return (a.change ?? 0) - (b.change ?? 0)
   })

   const renderRow: IDetailsListProps['onRenderRow'] = (props) => {

      if (props) {

         const item = props!.item as GetArtifactResponse;

         let background: string | undefined;
         if (props.itemIndex % 2 === 0) {
            background = dashboard.darktheme ? "#1D2021" : "#EAE9E9";
         }

         const stream = projectStore.streamById(item.streamId);

         return <Stack horizontal verticalAlign="center" verticalFill tokens={{ childrenGap: 24 }} styles={{ root: { backgroundColor: background, paddingLeft: 12, paddingRight: 12, paddingTop: 8, paddingBottom: 8 } }}>
            <Stack style={{ width: 420, paddingLeft: 8 }}>
               <Text style={{ fontWeight: 600 }}>{item.name}</Text>
            </Stack>
            <Stack style={{ width: 140 }}>
               <Text>{stream ? (stream.fullname ?? stream.name) : item.streamId}</Text>
            </Stack>
            <Stack style={{ width: 72 }}>
               <Text>{item.change}</Text>
            </Stack>
            <Stack style={{ width: 80 }}>
               <Text>{item.type}</Text>
            </Stack>
            <Stack>
               <DefaultButton style={{ width: 90, paddingRight: 8 }} text="Browse" onClick={() => {

                  const key = item.keys.find(k => k.startsWith("job:") && k.indexOf("/step:") !== -1);
                  if (!key) {
                     return;
                  }

                  setBrowse({ jobId: key.slice(4, 28), stepId: key.slice(-4), artifactId: item.id, artifact: item })

               }} />
            </Stack>
         </Stack>
      }
      return null;
   };

   return <Stack styles={{ root: { position: "relative", height: 590 } }}>
      {!!browse.jobId && <JobArtifactsModal jobId={browse.jobId} stepId={browse.stepId!} artifactId={browse.artifactId} contextType={browse.artifact!.type} onClose={() => setBrowse({})} />}
      <ScrollablePane scrollbarVisibility={ScrollbarVisibility.always}>
         <DetailsList
            isHeaderVisible={false}
            styles={{
               root: {
                  paddingRight: 12
               },
               headerWrapper: {
                  paddingTop: 0
               }
            }}
            items={sorted}
            selectionMode={SelectionMode.single}
            layoutMode={DetailsListLayoutMode.justified}
            compact={false}
            onRenderRow={renderRow}
         />
      </ScrollablePane>
   </Stack>

}

const artifactTypes: IComboBoxOption[] = [
   {
      key: `step-all`,
      text: `All`
   }, {
      key: `step-saved`,
      text: `step-saved`
   }, {
      key: `step-output`,
      text: `step-output`
   }, {
      key: `step-trace`,
      text: `step-trace`
   },
   {
      key: `step-testdata`,
      text: `step-testdata`
   }
]

export const FindArtifactsModal: React.FC<{ streamId?: string, onClose: () => void }> = ({ streamId, onClose }) => {

   const [state, setState] = useState<{ searching?: boolean, artifacts?: GetArtifactResponse[] }>({});
   const streamRef = React.useRef<IStreamChooser>(null);
   const minChangeRef = React.useRef<ITextField>(null);
   const maxChangeRef = React.useRef<ITextField>(null);
   const nameRef = React.useRef<ITextField>(null);
   const typeRef = React.useRef<IComboBox>(null);

   const { hordeClasses } = getHordeStyling();

   const queryArtifacts = async () => {

      setState({ ...state, searching: true });

      try {

         let minChange: number | undefined = parseInt(minChangeRef.current?.value?.trim() ?? "0");
         if (!minChange || isNaN(minChange)) {
            minChange = undefined;
         }

         let maxChange: number | undefined = parseInt(maxChangeRef.current?.value?.trim() ?? "0");
         if (!maxChange || isNaN(maxChange)) {
            maxChange = undefined;
         }

         let name: string | undefined = nameRef.current?.value?.trim();
         if (!name) {
            name = undefined;
         }

         let type: string | undefined;

         if (typeRef.current?.selectedOptions?.length) {

            type = (typeRef.current.selectedOptions[0].key as string)?.trim();
            const text = (typeRef.current.selectedOptions[0].text as string)?.trim();
            if (type === "step-all" || !text) {
               type = undefined;
            } else {
               const existing = artifactTypes.find(t => t.key === type);
               if (!existing) {
                  artifactTypes.push({ key: text, text: type });
               }
            }
         }


         let streamId = streamRef?.current?.streamId?.trim();
         if (!streamId) {
            streamId = undefined;
         }
         
         const mongoId = /^[a-fA-F0-9]{24}$/i;

         let id: string | undefined;

         if (name?.length) {
            if (name.match(mongoId)?.length) {
               id = name;
            }
         }

         let artifacts: GetArtifactResponse[] = [];

         if (id) {
            try {
               const artifact = await backend.getArtifactData(id);
               if (artifact?.id) {
                  artifacts = [artifact];
               }
            } catch (reason) {
               console.error(reason)
            }            

         } else {
            const find = await backend.getArtifacts(streamId, minChange, maxChange, name, type);
            artifacts = find.artifacts;
         }
         

         setState({
            searching: false, artifacts: artifacts
         });

      } catch (reason) {
         console.error(reason);
         setState({ searching: false });
      }
   }

   const Searching = () => {
      return <Modal isOpen={true} isBlocking={true} topOffsetFixed={true} styles={{ main: { padding: 8, width: 320, height: 128, hasBeenOpened: false, top: "128px", position: "absolute" } }} className={hordeClasses.modal}>
         <Stack horizontalAlign="center">
            <Stack styles={{ root: { padding: 8, paddingBottom: 32 } }}>
               <Text variant="mediumPlus">Searching for Artifacts</Text>
            </Stack>
            <Stack horizontalAlign="center">
               <Spinner size={SpinnerSize.large} />
            </Stack>
         </Stack>
      </Modal>
   }

   const filterTypes = [...artifactTypes].filter(t => !!t.text?.trim());

   return <Stack>
      <Modal isOpen={true} isBlocking={true} topOffsetFixed={true} styles={{ main: { padding: 8, width: 1024, height: 820, hasBeenOpened: false, top: "80px", position: "absolute" } }} onDismiss={() => onClose()} className={hordeClasses.modal}>
         {!!state.searching && <Searching />}
         <Stack className="horde-no-darktheme" styles={{ root: { paddingTop: 10, paddingRight: 12 } }}>
            <Stack style={{ paddingLeft: 24, paddingRight: 24 }}>
               <Stack tokens={{ childrenGap: 12 }} style={{ height: 800 }}>
                  <Stack horizontal verticalAlign="start">
                     <Stack style={{ paddingTop: 3 }}>
                        <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Find Artifacts</Text>
                     </Stack>
                     <Stack grow />
                     <Stack horizontalAlign="end">
                        <IconButton
                           iconProps={{ iconName: 'Cancel' }}
                           onClick={() => { onClose() }}
                        />
                     </Stack>
                  </Stack>
                  <Stack horizontal verticalAlign="center" tokens={{ childrenGap: 20 }}>
                     <Stack >
                        <Label>Stream</Label>
                        <StreamChooser defaultStreamId={streamId} ref={streamRef} />
                     </Stack>
                     <Stack >
                        <TextField key="min_change_option" componentRef={minChangeRef} style={{ width: 92 }} label="Min Changelist" />
                     </Stack>
                     <Stack >
                        <TextField key="max_change_option" componentRef={maxChangeRef} style={{ width: 92 }} label="Max Changelist" />
                     </Stack>
                     <Stack >
                        <TextField key="name_option" componentRef={nameRef} style={{ width: 220 }} label="Name / Artifact Id" spellCheck={false} autoComplete="off"/>
                     </Stack>
                     <Stack>
                        <Label>Artifact Type</Label>
                        <ComboBox key="type_option" componentRef={typeRef} allowFreeform={true} autoComplete="off" spellCheck={false} style={{ width: 144, textAlign: "left" }} defaultSelectedKey="step-all" options={filterTypes} calloutProps={{ doNotLayer: true }} />
                     </Stack>
                  </Stack>
                  <Stack horizontal>
                     <Stack grow />
                     <Stack>
                        <PrimaryButton disabled={!!state.searching} text="Find" onClick={() => (queryArtifacts())} />
                     </Stack>
                  </Stack>
                  <Stack styles={{ root: { paddingTop: 12 } }}>
                     <ArtifactsList artifacts={state.artifacts} />
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
      </Modal>
   </Stack>;
};
