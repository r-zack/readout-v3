/* This module allows the user to create a default config file for CAEN DTT
 * configuration module 'myCAEN_DTT_config'.
 *
 * 'defaultConfigFileBuilder' module version: a0.3
 */
#include "defaultConfigFileBuilder.h"
#include <stdio.h>

/* Creates a config file with default values for CAEN DTT parameters.
 * Returns 0 in case of failure and prints on 'stderr' a description of the
 * encountered problem. Otherwise, returns a non-zero integer.
 * If 'overwrite' is different from 0 then if an existing config file is found
 * it will be overwritten.
 *
 * @param overwrite if different from 0 the function overwrites an existing
 * config file if it is found
 * @return 0 in case of failure, otherwise returns a non-zero integer
 */
extern int createDefaultConfigFile(int overwrite){
  int success = 1, create=0;
  register int i = 0;
  FILE *fileOutput = NULL;
  // the number of elements of fileLines[]
  const int NUMBER_OF_LINES = 174;

    const char *fileLines[] = {
      "# NOTE: lines that start with '#' or that are blank are ignored!\n",
      "#\n",
      "\n",
      "#####                           #####\n",
      "##### Communication parameters  #####\n",
      "#####                           #####\n",
      "\n",
      "# LinkNum - Link number: in case of USB, the link numbers are assigned by the PC when you connect the cable to the device; it is 0 for the first device, 1 for the second and so on\n",
      "# (see \"OpenDigitizer\" in CAEN UM1935 manual)\n",
      "1\n",
      "\n",
      "# CAEN_DGTZ_LinkType - CAEN_DGTZ_USB / CAEN_DGTZ_OpticalLink (see \"OpenDigitizer\" in CAEN UM1935 manual)\n",
      "CAEN_DGTZ_USB\n",
      "\n",
      "# VMEBaseAddress - For direct USB connection, VMEBaseAddress must be 0 (see \"OpenDigitizer\" in CAEN UM1935 manual)\n",
      "0\n",
      "\n",
      "# CAEN_DGTZ_IOLevel - CAEN_DGTZ_IOLevel_NIM / CAEN_DGTZ_IOLevel_TTL (see \"Set / GetIOLevel\" in CAEN UM1935 manual)\n",
      "CAEN_DGTZ_IOLevel_TTL\n",
      "\n",
      "\n",
      "#####                         #####\n",
      "##### Acquisition parameters  #####\n",
      "#####                         #####\n",
      "\n",
      "# DPP_AcqMode - CAEN_DGTZ_DPP_ACQ_MODE_Oscilloscope / CAEN_DGTZ_DPP_ACQ_MODE_List / CAEN_DGTZ_DPP_ACQ_MODE_Mixed (see \"Set / GetDPPAcquisitionMode\" in CAEN UM1935 manual)\n",
      "CAEN_DGTZ_DPP_ACQ_MODE_List\n",
      "\n",
      "# RecordLength - Num. of samples of the waveforms (size of the acquisition window) (see \"Set / GetRecordLength\" in CAEN UM1935 manual)\n",
      "0\n",
      "\n",
      "# Channel enable mask - Sets which channels are enabled (see \"SetDPPParameters\" and \"Set / GetChannelEnableMask\" in CAEN UM1935 manual. NOTE: enter a hexadecimal number!)\n",
      "0xF\n",
      "\n",
      "# EventAggr - Number of events in one aggregate (0=automatic): set how many events to accumulate in the board memory before being available for readout\n",
      "# (see \"SetDPPEventAggregation\" in CAEN UM1935 manual)\n",
      "1000\n",
      "\n",
      "# PulsePolarity - CAEN_DGTZ_PulsePolarityPositive / CAEN_DGTZ_PulsePolarityNegative (see \"Set / GetChannelPulsePolarity\" in CAEN UM1935 manual)\n",
      "\n",
      "# this setting will be applied to all the channels\n",
      "CAEN_DGTZ_PulsePolarityNegative\n",
      "\n",
      "\n",
      "#####                         #####\n",
      "#####      DPP parameters     #####\n",
      "#####                         #####\n",
      "\n",
      "# WARNING: channel enable mask and 'enable bit' of channel-related parameters should be coherent and consistent! The parser will generate error if you specify in the following lines a\n",
      "# disabled channel as 'enabled' or vice-versa\n",
      "\n",
      "# thr - Trigger Threshold (the relative absolute value of the 'trigger threshold' expressed in LSB units. 1 LSB = 0.48 mV for CAEN 720 series (DT5790), 1 LSB = 0.97 mV for 751 series,\n",
      "# and 1 LSB = 0.12 mV for 725 and 730 series with 2 Vpp input range, or 0.03 mV for 725 and 730 series with 0.5 Vpp)\n",
      "# (see \"SetDPPParameters\" in CAEN UM1935 manual and \"Baseline\" in CAEN UM2580 DPSD UserManual)\n",
      "\n",
      "# this parameter is channel-related. For inserting the thr value for each channel you must adhere to this syntax: <channel number>,<'1' if enabled, '0' if disabled>,<value>\n",
      "# note that 'value' is ignored if the channel is disabled\n",
      "# WARNING: YOU MUST NOT WRITE COMMENTS OR PUT BLANK LINES BETWEEN THE LAST LINE OF A CHANNEL-RELATED PARAMETER AND THE COMMENT OF ITS NEXT PARAMETER!!!\n",
      "0,1,30\n",
      "1,1,100\n",
      "2,1,30\n",
      "3,1,35\n",
      "\n",
      "# nsbl - Number of Samples for BaseLine mean - the number of samples for the baseline averaging (options for x720: 0 = FIXED, 1 = 8, 2 = 32, 3 = 128;\n",
      "# options for x751: 0 = FIXED, 1 = 8, 2 = 16, 3 = 32, 4 = 64, 5 = 128, 6 = 256, 7 = 512; options for x730: 0 = FIXED, 1 = 16, 2 = 64, 3 = 256, 4 = 1024)\n",
      "# (see \"SetDPPParameters\" in CAEN UM1935 manual)\n",
      "\n",
      "# this parameter is channel-related. For inserting the nsbl value for each channel you must adhere to this syntax: <channel number>,<'1' if enabled, '0' if disabled>,<value>\n",
      "# note that 'value' is ignored if the channel is disabled\n",
      "# WARNING: YOU MUST NOT WRITE COMMENTS OR PUT BLANK LINES BETWEEN THE LAST LINE OF A CHANNEL-RELATED PARAMETER AND THE COMMENT OF ITS NEXT PARAMETER!!!\n",
      "0,1,2\n",
      "1,1,2\n",
      "2,1,2\n",
      "3,1,2\n",
      "\n",
      "# lgate - long gate width (N*4ns) - Long (i.e. total) Charge Integration Gate witdh (number of samples)\n",
      "# (see \"SetDPPParameters\" in CAEN UM1935 manual)\n",
      "\n",
      "# this parameter is channel-related. For inserting the lgate value for each channel you must adhere to this syntax: <channel number>,<'1' if enabled, '0' if disabled>,<value>\n",
      "# note that 'value' is ignored if the channel is disabled\n",
      "# WARNING: YOU MUST NOT WRITE COMMENTS OR PUT BLANK LINES BETWEEN THE LAST LINE OF A CHANNEL-RELATED PARAMETER AND THE COMMENT OF ITS NEXT PARAMETER!!!\n",
      "0,1,32\n",
      "1,1,32\n",
      "2,1,32\n",
      "3,1,32\n",
      "\n",
      "# sgate - short gate width (N*4ns) - Short (i.e. prompt) Charge Integration Gate width (number of samples)\n",
      "# (see \"SetDPPParameters\" in CAEN UM1935 manual)\n",
      "\n",
      "# this parameter is channel-related. For inserting the sgate value for each channel you must adhere to this syntax: <channel number>,<'1' if enabled, '0' if disabled>,<value>\n",
      "# note that 'value' is ignored if the channel is disabled\n",
      "# WARNING: YOU MUST NOT WRITE COMMENTS OR PUT BLANK LINES BETWEEN THE LAST LINE OF A CHANNEL-RELATED PARAMETER AND THE COMMENT OF ITS NEXT PARAMETER!!!\n",
      "0,1,18\n",
      "1,1,18\n",
      "2,1,18\n",
      "3,1,18\n",
      "\n",
      "# pgate - pre gate width (N*4ns) - Gate Offset (number of samples)\n",
      "# (see \"SetDPPParameters\" in CAEN UM1935 manual)\n",
      "\n",
      "# this parameter is channel-related. For inserting the pgate value for each channel you must adhere to this syntax: <channel number>,<'1' if enabled, '0' if disabled>,<value>\n",
      "# note that 'value' is ignored if the channel is disabled\n",
      "# WARNING: YOU MUST NOT WRITE COMMENTS OR PUT BLANK LINES BETWEEN THE LAST LINE OF A CHANNEL-RELATED PARAMETER AND THE COMMENT OF ITS NEXT PARAMETER!!!\n",
      "0,1,3\n",
      "1,1,3\n",
      "2,1,3\n",
      "3,1,3\n",
      "\n",
      "# selft - Self Trigger Mode: 0 -> Disabled, 1 -> Enabled - Channel self-trigger enable\n",
      "# (see \"SetDPPParameters\" in CAEN UM1935 manual)\n",
      "\n",
      "# this parameter is channel-related. For inserting the selft value for each channel you must adhere to this syntax: <channel number>,<'1' if enabled, '0' if disabled>,<value>\n",
      "# note that 'value' is ignored if the channel is disabled\n",
      "# WARNING: YOU MUST NOT WRITE COMMENTS OR PUT BLANK LINES BETWEEN THE LAST LINE OF A CHANNEL-RELATED PARAMETER AND THE COMMENT OF ITS NEXT PARAMETER!!!\n",
      "0,1,1\n",
      "1,1,1\n",
      "2,1,1\n",
      "3,1,1\n",
      "\n",
      "# trgc - Trigger configuration - CAEN_DGTZ_DPP_TriggerConfig_Peak -> trigger on peak. NOTE: Only for FW <= 13X.5 / CAEN_DGTZ_DPP_TriggerConfig_Threshold -> trigger on threshold\n",
      "# (see \"SetDPPParameters\" in CAEN UM1935 manual)\n",
      "\n",
      "# this parameter is channel-related. For inserting the trgc value for each channel you must adhere to this syntax: <channel number>,<'1' if enabled, '0' if disabled>,<value>\n",
      "# note that 'value' is ignored if the channel is disabled\n",
      "# WARNING: YOU MUST NOT WRITE COMMENTS OR PUT BLANK LINES BETWEEN THE LAST LINE OF A CHANNEL-RELATED PARAMETER AND THE COMMENT OF ITS NEXT PARAMETER!!!\n",
      "0,1,CAEN_DGTZ_DPP_TriggerConfig_Threshold\n",
      "1,1,CAEN_DGTZ_DPP_TriggerConfig_Threshold\n",
      "2,1,CAEN_DGTZ_DPP_TriggerConfig_Threshold\n",
      "3,1,CAEN_DGTZ_DPP_TriggerConfig_Threshold\n",
      "\n",
      "# tvaw - Trigger Validation Acceptance Window in coincidence mode (samples)\n",
      "# (see \"SetDPPParameters\" in CAEN UM1935 manual)\n",
      "\n",
      "# this parameter is channel-related. For inserting the tvaw value for each channel you must adhere to this syntax: <channel number>,<'1' if enabled, '0' if disabled>,<value>\n",
      "# note that 'value' is ignored if the channel is disabled\n",
      "# WARNING: YOU MUST NOT WRITE COMMENTS OR PUT BLANK LINES BETWEEN THE LAST LINE OF A CHANNEL-RELATED PARAMETER AND THE COMMENT OF ITS NEXT PARAMETER!!!\n",
      "0,1,50\n",
      "1,1,50\n",
      "2,1,50\n",
      "3,1,50\n",
      "\n",
      "# csens - Charge Sensitivity (options for x720: 0= 40, 1 = 160, 2= 640, 3 = 2560 fC/LSB; options for x751: 0 = 20, 1 = 40, 2 = 80, 3 = 160, 4  = 320, 5 = 640 fC/LSB)\n",
      "# (see \"SetDPPParameters\" in CAEN UM1935 manual)\n",
      "\n",
      "# this parameter is channel-related. For inserting the csens value for each channel you must adhere to this syntax: <channel number>,<'1' if enabled, '0' if disabled>,<value>\n",
      "# note that 'value' is ignored if the channel is disabled\n",
      "# WARNING: YOU MUST NOT WRITE COMMENTS OR PUT BLANK LINES BETWEEN THE LAST LINE OF A CHANNEL-RELATED PARAMETER AND THE COMMENT OF ITS NEXT PARAMETER!!!\n",
      "0,1,2\n",
      "1,1,2\n",
      "2,1,2\n",
      "3,1,2\n",
      "\n",
      "# purh - Pile-Up Rejection mode - CAEN_DGTZ_DPP_PSD_PUR_DetectOnly -> Only Detect Pile-Up / CAEN_DGTZ_DPP_PSD_PUR_Enabled -> Reject Pile-Up. Ignored for x751\n",
      "# (see \"SetDPPParameters\" in CAEN UM1935 manual)\n",
      "CAEN_DGTZ_DPP_PSD_PUR_DetectOnly\n",
      "\n",
      "# purgap - Purity Gap - Pile-Up Rejection GAP value (LSB). Ignored for x751\n",
      "# (see \"SetDPPParameters\" in CAEN UM1935 manual)\n",
      "100\n",
      "\n",
      "# blthr - Baseline Threshold\n",
      "# (see \"SetDPPParameters\" in CAEN UM1935 manual)\n",
      "3\n",
      "\n",
      "# bltmo - Baseline Timeout\n",
      "# (see \"SetDPPParameters\" in CAEN UM1935 manual)\n",
      "100\n",
      "\n",
      "# trgho - Trigger HoldOff (samples)\n",
      "# (see \"SetDPPParameters\" in CAEN UM1935 manual)\n",
      "256\n",
      "\n",
      "# acqtime - Acquisition Time (milliseconds)\n",
      "15000\n",
    };

    if(  (fileOutput = fopen("tdcr.ini", "r")) == NULL  ){
      // the config file probably doesn't exist
      create = 1;
    }
    else{
      if (!overwrite){
        fprintf(stderr, "\nFile \"tdcr.ini\" already exists!\n");
        fclose(fileOutput);
        success = 0;
      }
      else{
        fclose(fileOutput);
        create = 1;
      }
    }

    if( create&&success ){
      if(  (fileOutput = fopen("tdcr.ini", "w")) != NULL  ){
        // write default 'tdcr.ini' file
        for (; i < NUMBER_OF_LINES; i++){
          if (  fputs(fileLines[i],fileOutput) == EOF  ){
            fprintf(stderr, "\nError during writing on \"tdcr.ini\"!\n");
            fclose(fileOutput);
            success = 0;
            break;
          }
        }
        fclose(fileOutput);
      }
      else{
        fprintf(stderr, "\nUnable to create \"tdcr.ini\"!\n");
        success = 0;
      }      
    }

  return success;
}
