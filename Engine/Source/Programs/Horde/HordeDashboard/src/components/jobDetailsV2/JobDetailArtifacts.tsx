// Copyright Epic Games, Inc. All Rights Reserved.

import { DetailsList, DetailsListLayoutMode, FontIcon, IColumn, SelectionMode, Stack, Text, mergeStyleSets } from '@fluentui/react';
import { observer } from 'mobx-react-lite';
import React, { useEffect, useState } from 'react';
import { GetJobArtifactResponse, JobStepState } from '../../backend/Api';
import { ISideRailLink } from '../../base/components/SideRail';
import { getStepETA, getStepFinishTime } from '../../base/utilities/timeUtils';
import { getHordeStyling } from '../../styles/Styles';
import { StepStatusIcon } from '../StatusIcon';
import { JobArtifactsModal } from '../artifacts/ArtifactsModal';
import { JobDataView, JobDetailsV2 } from "./JobDetailsViewCommon";

const sideRail: ISideRailLink = { text: "Artifacts", url: "rail_artifacts" };

class JobArtifactsDataView extends JobDataView {

   filterUpdated() {

   }

   clear() {
      this.initial = true;
      this.hasArtifacts = undefined;
      super.clear();
   }

   set() { 

      if (!this.initial) {
         return;
      }

      this.initial = false;

      this.hasArtifacts = !!this.details?.jobData?.artifacts?.length;
      this.initialize(this.hasArtifacts ? [sideRail] : undefined);

      // Test for upating artifacts dynamically
      /*
      setTimeout(() => {
         this.details!.jobData!.artifacts = [
            {artifactId: "abcd", "stepId": "abcd", name: "Test Artifact", type: "test-artifact"}
         ]
         this.detailsUpdated();
      }, 5000)
      */
   }


   detailsUpdated() {

      const hasArtifacts = !!this.details?.jobData?.artifacts?.length;
      if (this.hasArtifacts !== hasArtifacts) {         
         this.hasArtifacts = hasArtifacts;
         this.initialize(hasArtifacts ? [sideRail] : undefined);
         this.details?.setRootUpdated();
      }
         
   }

   initial = true;

   hasArtifacts?: boolean;
   order = 0;

}

JobDetailsV2.registerDataView("JobArtifactsDataView", (details: JobDetailsV2) => new JobArtifactsDataView(details));

let _styles: any;

const getStyles = () => {

   const styles = _styles ?? mergeStyleSets({
      list: {
         selectors: {
            'a': {
               height: "unset !important",
            },
            ".ms-DetailsRow #artifactview": {
               opacity: 0
            },
            ".ms-DetailsRow:hover #artifactview": {
               opacity: 1
            },
         }
      }
   });

   _styles = styles;

   return styles;

}


export const JobArtifactsPanel: React.FC<{ jobDetails: JobDetailsV2 }> = observer(({ jobDetails }) => {

   const [selected, setSelected] = useState<GetJobArtifactResponse | undefined>(undefined);   
   
   const artifactView = jobDetails.getDataView<JobArtifactsDataView>("JobArtifactsDataView");

   jobDetails.subscribe();

   useEffect(() => {
      return () => {
         artifactView.clear();
      }
   }, [artifactView]);

   const jobData = jobDetails.jobData;

   if (!jobData) {
      return null;
   }

   artifactView.set();

   const hasArtifacts = !!jobData.artifacts?.length;

   if (!hasArtifacts) {
      return null;
   }

   const styles = getStyles();
   const { hordeClasses, modeColors } = getHordeStyling();

   const columns: IColumn[] = [
      { key: 'column_desc', name: 'Description', minWidth: 580, isResizable: false, isMultiline: true },
      { key: 'column_time', name: 'Time', minWidth: 120, isResizable: false, isMultiline: true },
      { key: 'column_cloud', name: 'Cloud', minWidth: 64, isResizable: false, isMultiline: true },
   ];

   let artifacts = [...jobData.artifacts!];

   artifacts = artifacts.sort((a, b) => {
      const aname = a.description ?? a.name;
      const bname = b.description ?? b.name;
      return aname.localeCompare(bname)
   });


   const renderItem = (item: GetJobArtifactResponse, index?: number, column?: IColumn) => {

      if (!column) {
         return null;
      }

      let step = jobDetails.stepById(item.stepId);

      let stepFinished = !!step?.finishTime;
      const cursor = stepFinished ? "pointer" : undefined;

      if (column.key === 'column_cloud') {
         return <Stack horizontalAlign="end" verticalAlign="center" verticalFill={true} style={{ cursor: cursor, paddingRight: 8 }} onClick={() => { if (stepFinished) setSelected(item) }}>
            <FontIcon id="artifactview" style={{ fontSize: "14px", color: stepFinished ? "#106EBE" : "#106EBE77" }} iconName="CloudDownload" />            
         </Stack>
      }      

      let eta = {
         display: "",
         server: ""
      };

      if (column.key === 'column_time' && !!step) {

         if (step.state === JobStepState.Skipped) {
            return null;
         }

         let finished = { display: "", server: "" };

         eta = getStepETA(step, jobDetails.jobData!);

         finished = getStepFinishTime(step);

         if (finished.display) {
            eta.display = finished.display;
            eta.server = finished.server;
         }

         return <Stack horizontalAlign={"end"} verticalAlign="center" verticalFill={true} style={{ cursor: cursor }} onClick={() => { if (stepFinished) setSelected(item) }}>
            <Stack horizontal tokens={{ childrenGap: 2 }}>
               {!!eta.display && !step.finishTime && <Text style={{ fontSize: "11px", paddingTop: 2 }}>~</Text>}
               <Text style={{ fontSize: "13px" }}>
                  {eta.display}
               </Text>
            </Stack>
         </Stack>

      }

      if (column.key === 'column_desc') {
         return <Stack horizontal verticalAlign="center" verticalFill={true} style={{cursor: cursor}} onClick={() => { if (stepFinished) setSelected(item) }}>
            {!!step && <StepStatusIcon step={step} style={{paddingTop: 2}} />}
            <Text style={{ color: modeColors.text, fontFamily: "Horde Open Sans SemiBold" }}>{item.description ?? item.name}</Text>
         </Stack>
      }

      return null;
   };


   return (<Stack id={sideRail.url} styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
      {!!selected && <JobArtifactsModal jobId={jobData.id} stepId={selected.stepId} contextType={selected.type} onClose={() => setSelected(undefined)} />}
      <Stack className={hordeClasses.raised} >
         <Stack tokens={{ childrenGap: 12 }} grow>
            <Stack horizontal>
               <Stack>
                  <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Artifacts</Text>
               </Stack>
            </Stack>
            <Stack >
               <Stack style={{ paddingTop: 8 }} tokens={{ childrenGap: 12 }}>
                  <DetailsList
                     styles={{ root: { overflowX: "hidden" } }}
                     isHeaderVisible={false}
                     className={styles.list}
                     items={artifacts}
                     columns={columns}
                     selectionMode={SelectionMode.none}
                     layoutMode={DetailsListLayoutMode.justified}
                     compact
                     onRenderItemColumn={renderItem}
                  />
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   </Stack>);
});
