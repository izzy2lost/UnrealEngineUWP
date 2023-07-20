// Copyright Epic Games, Inc. All Rights Reserved.

import { DefaultButton, DetailsList, DetailsListLayoutMode, IColumn, IGroup, IGroupHeaderProps, IconButton, MessageBar, MessageBarType, Modal, PrimaryButton, SelectionMode, Stack, Text } from '@fluentui/react';
import { mergeStyleSets } from '@fluentui/react/lib/Styling';
import { observer } from 'mobx-react-lite';
import moment from 'moment';
import React, { useEffect, useState } from 'react';
import { Link } from 'react-router-dom';
import backend from '../../backend';
import { BisectTaskState, GetBisectTaskResponse, GetJobStepRefResponse } from '../../backend/Api';
import dashboard, { StatusColor } from '../../backend/Dashboard';
import { Markdown } from '../../base/components/Markdown';
import { ISideRailLink } from '../../base/components/SideRail';
import { displayTimeZone, getElapsedString } from '../../base/utilities/timeUtils';
import { hordeClasses } from '../../styles/Styles';
import { ChangeButton } from '../ChangeButton';
import { StepRefStatusIcon } from '../StatusIcon';
import { JobDataView, JobDetailsV2 } from './JobDetailsViewCommon';


const sideRail: ISideRailLink = { text: "Bisection", url: "rail_detail_bisection" };


class StepBisectionView extends JobDataView {

   filterUpdated() {
      // this.updateReady();
   }

   async set(stepId?: string) {

      const details = this.details;
      if (!details) {
         return;
      }

      if ((stepId && this.stepId === stepId) || !details.jobId || !details.jobData?.id) {
         return;
      }

      this.stepId = stepId;

      if (!this.bisections) {

         try {

            let jobId = details.jobData?.startedByBisectTaskId ? details.jobData?.startedByBisectTaskId : details.jobId;

            if (jobId !== details.jobId) {
               const task = await backend.getBisectTask(details.jobData.startedByBisectTaskId!);
               jobId = task.initialJobId;

            }

            this.bisections = await backend.getJobBisectTasks(jobId);
            if (stepId) {
               this.bisections = this.bisections.filter(r => r.nodeName === details.getStepName(stepId));
            }
            

         } catch (error) {
            console.error(error);
         }
         finally {
            this.initialize(this.bisections?.length ? [sideRail] : undefined);
         }
      }
   }

   clear() {
      this.stepId = undefined;
      this.bisections = undefined;
      super.clear();
   }

   detailsUpdated() {

      if (!this.details?.jobData) {
         return;
      }

      this.updateReady();

   }

   stepId?: string;

   bisections?: GetBisectTaskResponse[];

   order = 9;

}

JobDetailsV2.registerDataView("StepBisectionView", (details: JobDetailsV2) => new StepBisectionView(details));

const customStyles = mergeStyleSets({
   details: {
      selectors: {
         '.ms-GroupHeader,.ms-GroupHeader:hover': {
            background: "#DFDEDD",
         },
         '.ms-GroupHeader-title': {
            cursor: "default"
         },
         '.ms-GroupHeader-expand,.ms-GroupHeader-expand:hover': {
            background: "#DFDEDD"
         },
         '.ms-DetailsRow': {
            animation: "none",
            background: "unset"
         },
         '.ms-DetailsRow:hover': {
            background: "#F3F2F1"
         }
      },
   }

});


export const CancelBisectionModal: React.FC<{ jobDetails: JobDetailsV2, dataView: StepBisectionView, bisectTaskId: string, onClose: (canceled:boolean) => void }> = ({ jobDetails, dataView, bisectTaskId, onClose }) => {

   const [state, setState] = useState<{ submitting?: boolean, error?: string }>({});

   const onCancelTask = async () => {

      setState({ ...state, submitting: true });

      try {

         await backend.updateBisectTask(bisectTaskId, {
            cancel: true
         });
         
         setState({ ...state, submitting: false, error: "" });
         onClose(true);
         
      } catch (reason) {

         setState({ ...state, submitting: false, error: reason as string });
      }

   };

   const height = state.error ? 200 : 180;

   return <Modal className={hordeClasses.modal} isOpen={true} topOffsetFixed={true} styles={{ main: { padding: 8, width: 540, height: height, minHeight: height, hasBeenOpened: false, top: "24px", position: "absolute" } }} onDismiss={() => { onClose(false) }}>
      <Stack horizontal styles={{ root: { padding: 8 } }}>
         <Stack.Item grow={2}>
            <Text variant="mediumPlus">Cancel Bisection</Text>
         </Stack.Item>
         <Stack.Item grow={0}>
            <IconButton
               iconProps={{ iconName: 'Cancel' }}
               ariaLabel="Close popup modal"
               onClick={() => { onClose(false); }}
            />
         </Stack.Item>
      </Stack>

      <Stack styles={{ root: { paddingLeft: 8, width: 540, paddingTop: 12 } }}>
         <Stack tokens={{ childrenGap: 12 }}>
            {!!state.error && <MessageBar
               messageBarType={MessageBarType.error}
               isMultiline={false}> {state.error} </MessageBar>}

         </Stack>
      </Stack>

      <Stack tokens={{ childrenGap: 16 }} styles={{ root: { paddingTop: 32, paddingLeft: 8, paddingBottom: 8 } }}>
         <Stack horizontal>
            <Stack>
            </Stack>
            <Stack grow />
            <Stack>
               <Stack horizontal tokens={{ childrenGap: 12 }} style={{ paddingRight: 24 }}>
                  <PrimaryButton text="Cancel Task" disabled={state.submitting ?? false} onClick={() => { onCancelTask() }} />
                  <DefaultButton text="Go Back" disabled={state.submitting} onClick={() => { onClose(false); }} />
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   </Modal>;

};


const StepBisectionList: React.FC<{ jobDetails: JobDetailsV2, stepId?: string, dataView: StepBisectionView }> = ({ jobDetails, stepId, dataView }) => {

   const [showCancelModal, setShowCancelModal] = useState("");

   type BisectionItem = {
      bisection: GetBisectTaskResponse;
      step?: GetJobStepRefResponse;
   }

   const bisections = dataView.bisections;
   if (!bisections?.length) {
      return null;
   }

   const items: BisectionItem[] = [];

   const groups: IGroup[] = [];

   bisections.forEach((b, cindex) => {

      const key = `group_${b.id}`;
      groups.push({
         key: key,
         name: `Bisection ${b.id} - ${b.state}`,
         data: b,
         startIndex: items.length,
         count: (b.steps?.length ?? 0),
         level: 0,
         isCollapsed: false //bisections.length > 0 && cindex !== 0
      });

      b.steps?.sort((a, b) => a.change - b.change).forEach(s => {
         items.push({
            bisection: b,
            step: s
         })
      })
   })

   const columns = [
      { key: 'column1', name: 'Name', minWidth: 620, maxWidth: 620, isResizable: false },
      { key: 'column2', name: 'Change', minWidth: 80, maxWidth: 80, isResizable: false },
      { key: 'column3', name: 'Started', minWidth: 180, maxWidth: 180, isResizable: false },
      { key: 'column4', name: 'Duration', minWidth: 110, maxWidth: 110, isResizable: false },
   ];

   const renderItem = (item: BisectionItem, index?: number, column?: IColumn) => {

      const step = item?.step;

      if (!column || !step) {
         return null;
      }

      if (column.name === "Name") {
         return <Link to={`/job/${step.jobId}?step=${step.stepId}`}><Stack horizontal>{<StepRefStatusIcon stepRef={step} />}<Text>{item.bisection.nodeName}</Text></Stack></Link>;
      }

      if (column.name === "Change") {


         return <ChangeButton job={jobDetails.jobData!} stepRef={step} hideAborted={true} />;

      }


      if (column.name === "Started") {

         if (step.startTime) {

            const displayTime = moment(step.startTime).tz(displayTimeZone());
            const format = dashboard.display24HourClock ? "HH:mm:ss z" : "LT z";

            let displayTimeStr = displayTime.format('MMM Do') + ` at ${displayTime.format(format)}`;


            return <Stack horizontal horizontalAlign={"start"}>{displayTimeStr}</Stack>;

         } else {
            return "???";
         }
      }

      if (column.name === "Duration") {

         const start = moment(step.startTime);
         let end = moment(Date.now());

         if (step.finishTime) {
            end = moment(step.finishTime);
         }
         if (step.startTime) {
            const time = getElapsedString(start, end);
            return <Stack horizontal horizontalAlign={"end"} style={{ paddingRight: 8 }}><Text>{time}</Text></Stack>;
         } else {
            return "???";
         }
      }


      return null;

   }

   const onRenderGroupHeader = (props?: IGroupHeaderProps): JSX.Element | null => {

      if (!props) {
         return null;
      }

      const bisection = props.group?.data as GetBisectTaskResponse;

      let markdown = `**Bisection ${bisection.id}** -  **State:** ${bisection.state} / **Initial Job:** [${bisection.initialJobId}](/job/${bisection.initialJobId}) - ${bisection.initialChange}`;
      if (bisection.currentJobId) {
         markdown += ` / **Current Job:** [${bisection.currentJobId}](/job/${bisection.currentJobId}) - ${bisection.currentChange}`
      }

      markdown += ` / **Outcome:** ${bisection.outcome}`

      /*
      if (bisection.owner) {
         markdown += ` / **Owner:** ${bisection.owner.name}`
      }
      */

      return <Stack verticalAlign="center" style={{ height: 48, padding: 18, backgroundColor: "rgb(233, 232, 231)" }}>
         <Stack horizontal verticalAlign="center">
            <Stack>
               <Markdown>{markdown}</Markdown>
            </Stack>
            <Stack grow />
            {bisection.state === BisectTaskState.Running && <Stack>
               <DefaultButton style={{ color: "#FFFFFF", backgroundColor: dashboard.getStatusColors().get(StatusColor.Failure) }} text="Cancel" onClick={() => setShowCancelModal(bisection.id)} />
            </Stack>
            }
         </Stack>
      </Stack>
   }

   return <Stack>
      {!!showCancelModal && <CancelBisectionModal jobDetails={jobDetails} dataView={dataView} bisectTaskId={showCancelModal} onClose={(canceled) => { if (canceled) { window.location.reload(); setShowCancelModal("") } }} />}
      <DetailsList
         className={customStyles.details}
         compact={true}
         isHeaderVisible={false}
         indentWidth={0}
         items={items}
         groups={groups}
         groupProps={{ onRenderHeader: onRenderGroupHeader, showEmptyGroups: true }}
         columns={columns}
         setKey="set"
         selectionMode={SelectionMode.none}
         layoutMode={DetailsListLayoutMode.justified}
         onRenderItemColumn={renderItem}
      />
   </Stack>;
}

export const BisectionPanel: React.FC<{ jobDetails: JobDetailsV2, stepId?: string }> = observer(({ jobDetails, stepId }) => {

   const dataView = jobDetails.getDataView<StepBisectionView>("StepBisectionView");

   useEffect(() => {
      return () => {
         dataView?.clear();
      };
   }, [dataView]);

   dataView.subscribe();

   dataView.set(stepId);

   if (!dataView.bisections?.length) {
      return null;
   }

   return (<Stack id={sideRail.url} styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
      <Stack className={hordeClasses.raised}>
         <Stack tokens={{ childrenGap: 12 }}>
            <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Bisection</Text>
            <Stack styles={{ root: { paddingLeft: 4, paddingRight: 0, paddingTop: 8, paddingBottom: 4 } }}>
               <div style={{ overflowY: 'auto', overflowX: 'hidden', minHeight: "400px", maxHeight: "400px" }} data-is-scrollable={true}>
                  <StepBisectionList jobDetails={jobDetails} dataView={dataView} />
               </div>
            </Stack>
         </Stack>
      </Stack>
   </Stack>);
});


