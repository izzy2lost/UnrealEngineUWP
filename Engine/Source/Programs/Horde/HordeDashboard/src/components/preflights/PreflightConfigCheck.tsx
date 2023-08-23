import { FontIcon, MaskedTextField, PrimaryButton, Spinner, SpinnerSize, Stack, Text, TextField } from "@fluentui/react";
import { useWindowSize } from "../../base/utilities/hooks";
import { hordeClasses, modeColors } from "../../styles/Styles";
import { Breadcrumbs } from "../Breadcrumbs";
import { TopNav } from "../TopNav";
import { useState } from "react";
import { useNavigate } from "react-router-dom";
import backend from "../../backend";
import dashboard, { StatusColor } from "../../backend/Dashboard";

const PreflightConfigPanel: React.FC = () => {

   const search = new URL(window.location.toString()).searchParams;
   const shelvedChange = search.get("shelvedchange") ? search.get("shelvedchange")! : undefined;

   const navigate = useNavigate();
   const [state, setState] = useState<{ initialCL?: string, submitting?: boolean, success?: boolean, message?: string }>({initialCL: shelvedChange});

   const maskFormat: { [key: string]: RegExp } = {
      '*': /[0-9]/,
   };

   const checkPreflight = async (preflightCL?: string) => {

      if (!preflightCL) {

         if (!shelvedChange) {
            return;
         }
   
         preflightCL = shelvedChange;
      }

      setState({ submitting: true });

      try {
         const response = await backend.checkPreflightConfig(parseInt(preflightCL));
         setState({ submitting: false, success: response.result, message: response.message });
      } catch (error) {
         setState({ submitting: false, success: false, message: error as string });
      }

   }

   if (state.initialCL) {
      checkPreflight(state.initialCL);
      return null;
   }


   return (<Stack>
      <Stack styles={{ root: { paddingTop: 18, paddingLeft: 12, paddingRight: 12, width: "100%" } }} >
         <Stack tokens={{ childrenGap: 12 }} >
            <Stack style={{ width: 800, paddingLeft: 12 }}>
               <Stack horizontal verticalAlign="center" tokens={{ childrenGap: 24 }}>
                  <Stack horizontal verticalAlign="center" tokens={{ childrenGap: 18 }}>
                     <Text styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Shelved Change</Text>
                     <MaskedTextField placeholder="Shelved Change" mask="***********" maskFormat={maskFormat} maskChar="" value={shelvedChange} onChange={(ev, newValue) => {
                        ev.preventDefault();

                        if (!newValue) {
                           navigate("/preflightconfig", { replace: true });
                        }

                        if (!isNaN(parseInt(newValue!))) {
                           navigate(`?shelvedchange=${newValue}`, { replace: true });
                        }

                     }} />
                  </Stack>
                  <Stack grow />
                  {!!state.submitting && <Stack>
                     <Spinner size={SpinnerSize.large} />
                  </Stack>}

                  {state.success === true && <Stack>
                     <FontIcon style={{ color: dashboard.getStatusColors().get(StatusColor.Success)!, fontSize: 24 }} iconName="Tick" />
                  </Stack>}

                  {state.success === false && <Stack>
                     <FontIcon style={{ color: dashboard.getStatusColors().get(StatusColor.Failure)!, fontSize: 24 }} iconName="Cross" />
                  </Stack>}

                  <Stack>
                     <PrimaryButton disabled={!shelvedChange || state.submitting} styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }} onClick={async () => {
                        checkPreflight(shelvedChange);
                     }}>Check</PrimaryButton>
                  </Stack>
               </Stack>
               {!!state.message && <Stack style={{ paddingTop: 24, paddingRight: 2 }}>
                  <TextField readOnly multiline resizable={false} label="Error Message" autoAdjustHeight={false} defaultValue={state.message} style={{ whiteSpace: "pre-wrap", height: 500 }}> </TextField>
               </Stack>}
            </Stack>
         </Stack>
      </Stack>
   </Stack>);
}


export const PreflightConfigView: React.FC = () => {

   const windowSize = useWindowSize();
   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);

   return <Stack className={hordeClasses.horde}>
      <TopNav />
      <Breadcrumbs items={[{ text: 'Preflight Configuration' }]} />
      <Stack horizontal>
         <div key={`windowsize_noticeview_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - (1440 / 2), flexShrink: 0, backgroundColor: modeColors.background }} />
         <Stack tokens={{ childrenGap: 0 }} styles={{ root: { backgroundColor: modeColors.background, width: "100%" } }}>
            <Stack style={{ maxWidth: 1440, paddingTop: 6, marginLeft: 4, height: 'calc(100vh - 8px)' }}>
               <Stack horizontal className={hordeClasses.raised}>
                  <Stack style={{ width: "100%", height: 'calc(100vh - 228px)' }} tokens={{ childrenGap: 18 }}>
                     <PreflightConfigPanel />
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   </Stack>
};
