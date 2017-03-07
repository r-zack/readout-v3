/******************************************************************************
*
* CAEN SpA - Front End Division
* Via Vetraia, 11 - 55049 - Viareggio ITALY
* +390594388398 - www.caen.it
*
***************************************************************************//**
* \note TERMS OF USE:
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation. This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. The user relies on the
* software, documentation and results solely at his own risk.
******************************************************************************/

#include <CAENDigitizer.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "myCAEN_DTT_config.h"
#include "paramsHeaderToFile.h"

//#define MANUAL_BUFFER_SETTING   0
// The following define must be set to the actual number of connected boards
#define MAXNB   1
// NB: the following define MUST specify the ACTUAL max allowed number of board's channels
// it is needed for consistency inside the CAENDigitizer's functions used to allocate the memory
#define MaxNChannels 4

#define MAXNBITS 12

/* include some useful functions from file Functions.h
you can find this file in the src directory */
#include "Functions.h"

/* ###########################################################################
*  Functions
*  ########################################################################### */

/* --------------------------------------------------------------------------------------------------------- */
/*! \fn      int ProgramDigitizer(int handle, DigitizerParams_t Params, CAEN_DGTZ_DPPParamsPHA_t DPPParams)
 *   \brief   Program the registers of the digitizer with the relevant parameters
 *   \return  0=success; -1=error */
/* --------------------------------------------------------------------------------------------------------- */
int ProgramDigitizer(int handle, DigitizerParams_t Params, CAEN_DGTZ_DPP_PSD_Params_t DPPParams)
{
	/* This function uses the CAENDigitizer API functions to perform the digitizer's initial configuration */
	int i, ret = 0;

	/* Reset the digitizer */
	ret |= CAEN_DGTZ_Reset(handle);

	if (ret) {
		printf("ERROR: can't reset the digitizer.\n");
		return -1;
	}

	/* Set the DPP acquisition mode
	This setting affects the modes Mixed and List (see CAEN_DGTZ_DPP_AcqMode_t definition for details)
	CAEN_DGTZ_DPP_SAVE_PARAM_EnergyOnly        Only energy (DPP-PHA) or charge (DPP-PSD/DPP-CI v2) is returned
	CAEN_DGTZ_DPP_SAVE_PARAM_TimeOnly        Only time is returned
	CAEN_DGTZ_DPP_SAVE_PARAM_EnergyAndTime    Both energy/charge and time are returned
	CAEN_DGTZ_DPP_SAVE_PARAM_None            No histogram data is returned */
	ret |= CAEN_DGTZ_SetDPPAcquisitionMode(handle, Params.AcqMode, CAEN_DGTZ_DPP_SAVE_PARAM_EnergyAndTime);

	// Set the digitizer acquisition mode (CAEN_DGTZ_SW_CONTROLLED or CAEN_DGTZ_S_IN_CONTROLLED)
	ret |= CAEN_DGTZ_SetAcquisitionMode(handle, CAEN_DGTZ_SW_CONTROLLED);

	// Set the I/O level (CAEN_DGTZ_IOLevel_NIM or CAEN_DGTZ_IOLevel_TTL)
	ret |= CAEN_DGTZ_SetIOLevel(handle, Params.IOlev);

	/* Set the digitizer's behaviour when an external trigger arrives:

	CAEN_DGTZ_TRGMODE_DISABLED: do nothing
	CAEN_DGTZ_TRGMODE_EXTOUT_ONLY: generate the Trigger Output signal
	CAEN_DGTZ_TRGMODE_ACQ_ONLY = generate acquisition trigger
	CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT = generate both Trigger Output and acquisition trigger

	see CAENDigitizer user manual, chapter "Trigger configuration" for details */
	ret |= CAEN_DGTZ_SetExtTriggerInputMode(handle, CAEN_DGTZ_TRGMODE_ACQ_ONLY);

	// Set the enabled channels
	ret |= CAEN_DGTZ_SetChannelEnableMask(handle, Params.ChannelMask);

	// Set how many events to accumulate in the board memory before being available for readout
	ret |= CAEN_DGTZ_SetDPPEventAggregation(handle, Params.EventAggr, 0);

	/* Set the mode used to syncronize the acquisition between different boards.
	   In this example the sync is disabled */
	ret |= CAEN_DGTZ_SetRunSynchronizationMode(handle, CAEN_DGTZ_RUN_SYNC_Disabled);

	// Set the DPP specific parameters for the channels in the given channelMask
	ret |= CAEN_DGTZ_SetDPPParameters(handle, Params.ChannelMask, &DPPParams);

	for (i = 0; i < MaxNChannels; i++) {
		if (Params.ChannelMask & (1 << i)) {
			// Set the number of samples for each waveform (you can set different RL for different channels)
			ret |= CAEN_DGTZ_SetRecordLength(handle, Params.RecordLength, i);

			// Set a DC offset to the input signal to adapt it to digitizer's dynamic range
			ret |= CAEN_DGTZ_SetChannelDCOffset(handle, i, 0x2F5C);//cambio 0x8000 -> 0x0000: 21/07/2014 ore 9:37, cambio in DE00 08/01/2015 per spostare DC offset

			// Set the Pre-Trigger size (in samples)
			ret |= CAEN_DGTZ_SetDPPPreTriggerSize(handle, i, 18);

			// Set the polarity for the given channel (CAEN_DGTZ_PulsePolarityPositive or CAEN_DGTZ_PulsePolarityNegative)
			ret |= CAEN_DGTZ_SetChannelPulsePolarity(handle, i, Params.PulsePolarity);
		}
	}

	return 0;

}

/* ########################################################################### */
/* MAIN                                                                        */
/* ########################################################################### */
int main(int argc, char *argv[])
{
	/* The following variable is the type returned from most of CAENDigitizer
	library functions and is used to check if there was an error in function
	execution. For example:
	ret = CAEN_DGTZ_some_function(some_args);
	if(ret) printf("Some error"); */
	CAEN_DGTZ_ErrorCode ret;

	/* Buffers to store the data. The memory must be allocated using the appropriate
	CAENDigitizer API functions (see below), so they must not be initialized here
	NB: you must use the right type for different DPP analysis (in this case PSD) */
	char *buffer = NULL;										// readout buffer
	CAEN_DGTZ_DPP_PSD_Event_t       *Events[MaxNChannels];		// events buffer
	CAEN_DGTZ_DPP_PSD_Waveforms_t   *Waveform = NULL;			// waveforms buffer

	/* The following variables will store the digitizer configuration parameters */
	CAEN_DGTZ_DPP_PSD_Params_t DPPParams[MAXNB];
	DigitizerParams_t          Params[MAXNB];

	/* Arrays for data analysis */
	uint64_t PrevTime[MAXNB][MaxNChannels];
	uint64_t ExtendedTT[MAXNB][MaxNChannels];
	uint32_t *EHistoShort[MAXNB][MaxNChannels];   // Energy Histograms for short gate charge integration
	uint32_t *EHistoLong[MAXNB][MaxNChannels];    // Energy Histograms for long gate charge integration
	float *EHistoRatio[MAXNB][MaxNChannels];      // Energy Histograms for ratio Long/Short
	int ECnt[MAXNB][MaxNChannels];                // Number-of-Entries Counter for Energy Histograms short and long gate
	int TrgCnt[MAXNB][MaxNChannels];

	/* The following variable will be used to get an handler for the digitizer. The
	handler will be used for most of CAENDigitizer functions to identify the board */
	int handle[MAXNB];

	/* Other variables */
	unsigned int i, b, ch, ev;
	int Quit = 0;
	int AcqRun = 0;
	uint32_t AllocatedSize, BufferSize;
	int Nb = 0;
	int DoSaveWave[MAXNB][MaxNChannels];
	int MajorNumber;
	int BitMask = 0;
	uint64_t CurrentTime, PrevRateTime, ElapsedTime;
	uint64_t StartAcqTime, EndAcqTime, acquisitionTime = 0;
	uint32_t NumEvents[MaxNChannels];
	CAEN_DGTZ_BoardInfo_t           BoardInfo;
	FILE *fpout;
	char fnameOut[255];
	char filename[255];
	int userRequestedAction = 0;        // code returned by acquireParameterValues()
	/* var to keep track of what should NOT be closed / freed on program exit */
	int isFpoutOpen = 0;
  /* Elapsed time from the beginning of the acquisition (milliseconds) */
  unsigned long timePassedSinceStart = 0ul;
  int hoursPassed, minToDisplayPassed, sToDisplayPassed, sPassedSinceStart;
  /* Var to keep track of the number of events recorded from each channel
  (each channel has got its own 'counter') */
  unsigned long int totalRecordedEvents[MaxNChannels];

  /* ************************************************************************ *
   * PLEASE READ CAREFULLY: the current version of this program defines the   *
   * macro 'MAXNB' and gives to it the value '1'. The macro is then used      *
   * sometimes along this source code.                                        *
   * However, actually the code is written and was tested to be used          *
   * exclusively with just 1 board connected!                                 *
   * The aforementioned macro and the part of the code that use it were left  *
   * in the code just to ease future implementations that will really enable  *
   * the multi-board functionality. So in the current version of the code the *
   * loops on 'MAXNB' will iterate just once.                                 *
   *                                                                          *
   * DO NOT CHANGE THE VALUE OF 'MAXNB' WITHOUT MAKING THE APPROPRIATE        *
   * SOURCE CODE CHANGES!!!!!!!!                                              *
   * ************************************************************************ */

  memset(DoSaveWave, 0, MAXNB*MaxNChannels*sizeof(int));

	for (i = 0; i < MAXNBITS; i++)
		BitMask |= 1 << i; /* Create a bit mask based on number of bits of the board */

	/* *************************************************************************************** */
	/* Set Parameters                                                                          */
	/* *************************************************************************************** */
	memset(&Params, 0, MAXNB*sizeof(DigitizerParams_t));
	memset(&DPPParams, 0, MAXNB*sizeof(CAEN_DGTZ_DPP_PSD_Params_t));

	for (b = 0; b < MAXNB; b++) {
		for (ch = 0; ch < MaxNChannels; ch++)
		{
			EHistoShort[b][ch] = NULL; // Set all histograms pointers to NULL (we will allocate them later)
			EHistoLong[b][ch] = NULL;
			EHistoRatio[b][ch] = NULL;
		}
	}

  /* initialize to 0 the elements of 'totalRecordedEvents' */
  for (ch = 0; ch < MaxNChannels; ch++){
    *(totalRecordedEvents + ch) = 0;
  }


	/* Get the acquisition time and DTT / DPP-PSD parameter values from
	   'config file'/'user input' */
	userRequestedAction = acquireParameterValues();
	if (userRequestedAction == 0){						// QUIT
		// QUIT the program
		goto QuitProgram;
	}
	else if (userRequestedAction == 1){					// START THE ACQUISITION
		/* copy the just-acquired parameter values into 'Params', 'DPPParams',
		   'acquisitionTime', then continue to START the acquisition    */
		Params[0] = dttParams;
		DPPParams[0] = dppParams;
		acquisitionTime = (uint64_t)acqTime;
	}


	/* *************************************************************************************** */
	/* Open the digitizer and read board information                                           */
	/* *************************************************************************************** */
	/* The following function is used to open the digitizer with the given connection parameters
	and get the handler to it */

	/* The following is for one board connected via USB direct link
	in this case you must set Params[0].LinkType = CAEN_DGTZ_USB and Params[0].VMEBaseAddress = 0 */
	ret = CAEN_DGTZ_OpenDigitizer(Params[0].LinkType, linkNum, 0, Params[0].VMEBaseAddress, &handle[0]);

	if (ret)
	{
		printf("Can't open digitizer\n");
		goto QuitProgram;
	}

	/* Once we have the handler to the digitizer, we use it to call the other functions */
	ret = CAEN_DGTZ_GetInfo(handle[0], &BoardInfo);
	if (ret)
	{
		printf("Can't read board info\n");
		goto QuitProgram;
	}
	printf("\nConnected to CAEN Digitizer Model %s, recognized as board %d\n", BoardInfo.ModelName, linkNum);
	printf("ROC FPGA Release is %s\n", BoardInfo.ROC_FirmwareRel);
	printf("AMC FPGA Release is %s\n", BoardInfo.AMC_FirmwareRel);

	// Check firmware revision (only DPP firmware can be used with this Demo) */
	sscanf(BoardInfo.AMC_FirmwareRel, "%d", &MajorNumber);
	if (MajorNumber != 131 && MajorNumber != 132)
	{
		printf("This digitizer has not a DPP-PSD firmware\n");
		goto QuitProgram;
	}


	/* *************************************************************************************** */
	/* Program the digitizer (see function ProgramDigitizer)                                   */
	/* *************************************************************************************** */
	for (b = 0; b < MAXNB; b++) {
		ret = ProgramDigitizer(handle[b], Params[b], DPPParams[b]);
		if (ret) {
			printf("Failed to program the digitizer\n");
			goto QuitProgram;
		}
	}

	/* WARNING: The mallocs MUST be done after the digitizer programming,
	because the following functions needs to know the digitizer configuration
	to allocate the right memory amount */

	/* Allocate memory for the readout buffer */
	ret = CAEN_DGTZ_MallocReadoutBuffer(handle[0], &buffer, &AllocatedSize);

	/* Allocate memory for the events */
	ret |= CAEN_DGTZ_MallocDPPEvents(handle[0], Events, &AllocatedSize);

	/* Allocate memory for the waveforms */
	ret |= CAEN_DGTZ_MallocDPPWaveforms(handle[0], &Waveform, &AllocatedSize);

	if (ret)
	{
		printf("Can't allocate memory buffers\n");
		goto QuitProgram;
	}
	//printf("%d\n", (int)ret);
	//getch();
	/*******************************************************************************************/
	/*   richiedo il nome del file senza estensione al quale aggiungo l'informazione su data   */
	/*   e ora del tipo tc99_25072014_153420                                                   */
	/*******************************************************************************************/
	printf("\nNome del file di output: ");
	scanf("%s", filename);
	time_t now = time(0);
	struct tm *gmtm = gmtime(&now);
	sprintf(fnameOut, "%s.dat", filename);
	//printf("%s\n", fnameOut);

	printf("AcqTime: %lu ms\n", acquisitionTime);

	if ((fpout = fopen(fnameOut, "w")) == NULL)
	{
		printf("Errore Apertura file!!!!!\n");
		exit(-1);
	}
  else{
    isFpoutOpen = 1;
    if( printParamsHeaderToFile(fpout) ){
      fprintf(stderr,"An error occurred while writing a file!\n");
      exit(EXIT_FAILURE);
    }
  }

	printf("Soglia ChA: %d\n", DPPParams[0].thr[0]);
	printf("Soglia ChB: %d\n", DPPParams[0].thr[2]);
	printf("Soglia ChC: %d\n", DPPParams[0].thr[3]);
	/* *************************************************************************************** */
	/* Readout Loop                                                                            */
	/* *************************************************************************************** */
	// Clear Histograms and counters
	for (b = 0; b < MAXNB; b++)
	{
		for (ch = 0; ch < MaxNChannels; ch++) {
			// Allocate Memory for Histos and set them to 0
			EHistoShort[b][ch] = (uint32_t *)malloc((1 << MAXNBITS)*sizeof(uint32_t));
			memset(EHistoShort[b][ch], 0, (1 << MAXNBITS)*sizeof(uint32_t));
			EHistoLong[b][ch] = (uint32_t *)malloc((1 << MAXNBITS)*sizeof(uint32_t));
			memset(EHistoLong[b][ch], 0, (1 << MAXNBITS)*sizeof(uint32_t));
			EHistoRatio[b][ch] = (float *)malloc((1 << MAXNBITS)*sizeof(float));
			memset(EHistoRatio[b][ch], 0, (1 << MAXNBITS)*sizeof(float));
			TrgCnt[b][ch] = 0;
			ECnt[b][ch] = 0;
			PrevTime[b][ch] = 0;
			ExtendedTT[b][ch] = 0;
		}
	}


	fprintf(fpout, "#    ch       timestamp       Qs       Ql           ExtendedTT\n");

	for (b = 0; b < MAXNB; b++)
	{
		// Start Acquisition
		// NB: the acquisition for each board starts when the following line is executed
		// so in general the acquisition does NOT starts syncronously for different boards
		CAEN_DGTZ_SWStartAcquisition(handle[b]);
		printf("Acquisition Started for Board %d\n", linkNum);
	}

	AcqRun = 1;
	StartAcqTime = PrevRateTime = get_time();

	while (!Quit)
	{
		/* Calculate throughput and trigger rate (every second) */
		CurrentTime = get_time();
		ElapsedTime = CurrentTime - PrevRateTime; /* milliseconds */
		if (ElapsedTime > 1000)
		{
			system(CLEARSCR);
      /* Calculate h,m,seconds passed since Start acquisition */
      timePassedSinceStart += ElapsedTime;
      sPassedSinceStart = timePassedSinceStart / 1000ul;
      sToDisplayPassed = sPassedSinceStart % 60ul;
      minToDisplayPassed = (sPassedSinceStart / 60ul) % 60ul;
      hoursPassed = ((sPassedSinceStart / 60ul) / 60ul) % 60ul;
      // Display readout filename, 'h,m,seconds' since Start acquisition, readout rate
      printf("Data readout file: %s\nElapsed time: %dh %dmin %ds\n\nReadout Rate=%.2f MB\n", fnameOut, hoursPassed, minToDisplayPassed, sToDisplayPassed, (float)Nb / ((float)ElapsedTime*1048.576f));
			for (b = 0; b < MAXNB; b++)
			{
        printf("\nBoard %d:\n", linkNum);
				for (i = 0; i < MaxNChannels; i++)
				{
					if (TrgCnt[b][i]>0)
            printf("\tCh %d:\tTrgRate=%.2f KHz\tTotal events: %lu\n", b * 8 + i, (float)TrgCnt[b][i] / (float)ElapsedTime, totalRecordedEvents[i]);
					else
						printf("\tCh %d:\tNo Data\n", i);
					TrgCnt[b][i] = 0;
				}
			}
			Nb = 0;
			PrevRateTime = CurrentTime;
			printf("\n\n");
		}

		/* Read data from the boards */
		for (b = 0; b < MAXNB; b++)
		{
			/* Read data from the board */
			ret = CAEN_DGTZ_ReadData(handle[b], CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer, &BufferSize);
			if (ret)
			{
				printf("Readout Error\n");
				goto QuitProgram;
			}
			if (BufferSize == 0)
				continue;        // torna all'inizio del while

			Nb += BufferSize;
			//ret = DataConsistencyCheck((uint32_t *)buffer, BufferSize/4);
			ret |= CAEN_DGTZ_GetDPPEvents(handle[b], buffer, BufferSize, Events, NumEvents);
			if (ret)
			{
				printf("Data Error: %d\n", ret);
				goto QuitProgram;
			}

			/* Analyze data */
			for (ch = 0; ch < MaxNChannels; ch++)
			{
				if (!(Params[b].ChannelMask & (1 << ch)))
					continue;

				/* Update Histograms */
				for (ev = 0; ev < NumEvents[ch]; ev++)
				{
					TrgCnt[b][ch]++;
					/* Time Tag */
					if (Events[ch][ev].TimeTag < PrevTime[b][ch])
					{
						ExtendedTT[b][ch]++;
						printf("%7d\n", ExtendedTT[b][ch]);
					}

					PrevTime[b][ch] = Events[ch][ev].TimeTag;
					fprintf(fpout, "%7d, %15lu, %7d, %7d, %15lu\n", ch, Events[ch][ev].TimeTag, (Events[ch][ev].ChargeShort & BitMask), (Events[ch][ev].ChargeLong & BitMask), ExtendedTT[b][ch]);
				} // loop on events

        /* Update the event counter of each channel */
        /* NOTE: the next assignment relies on the fact that MAXNB=1
           See the 'PLEASE READ CAREFULLY' note for more information on this */
        totalRecordedEvents[ch] += NumEvents[ch];
			} // loop on channels
		} // loop on boards
		EndAcqTime = get_time();

		if ((EndAcqTime - StartAcqTime) >= acquisitionTime)
		{
			for (b = 0; b < MAXNB; b++)
			{
				// Stop Acquisition
				CAEN_DGTZ_SWStopAcquisition(handle[b]);
				printf("Acquisition Stopped for Board %d\n", linkNum);
        // DELETE THIS LINE!!!!!!!!!!!!!!!
        // DELETE THIS LINE!!!!!!!!!!!!!!!

        // Retrieve the remaining data from buffer
        if (CAEN_DGTZ_ReadData(handle[0], CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer, &BufferSize) == 0){

          if(  CAEN_DGTZ_WriteRegister( handle[0], 0x803a, (uint32_t)1u ) == 0  ){

            if (CAEN_DGTZ_ReadData(handle[0], CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer, &BufferSize) == 0){
              if (BufferSize == 0){
                fprintf(stderr, "\nBuffer size of the last readout = 0!\n");
              }
              else{
                fprintf(stderr, "\nProsegui1.\n");
              }
            }
            else
              fprintf(stderr, "NOK1");

          }
          else{
            fprintf(stderr, "\nWriteRegister error!\n");
          }

        }
        else
          fprintf(stderr, "NOK2");

        // DELETE THIS LINE!!!!!!!!!!!!!!!
        // DELETE THIS LINE!!!!!!!!!!!!!!!
			}
			AcqRun = 0;
			goto QuitProgram;
		}
	} // End of readout loop



QuitProgram:
	printf("Acquisition Time: %lu [ms] \n", EndAcqTime - StartAcqTime);
	/* stop the acquisition, close the device and free the buffers */
	for (b = 0; b < MAXNB; b++) {
		CAEN_DGTZ_SWStopAcquisition(handle[b]);
		CAEN_DGTZ_CloseDigitizer(handle[b]);
		for (ch = 0; ch < MaxNChannels; ch++) {
			free(EHistoShort[b][ch]);
			free(EHistoLong[b][ch]);
			free(EHistoRatio[b][ch]);
		}
	}
	CAEN_DGTZ_FreeReadoutBuffer(&buffer);
	CAEN_DGTZ_FreeDPPEvents(handle[0], Events);
	CAEN_DGTZ_FreeDPPWaveforms(handle[0], Waveform);
	if (isFpoutOpen)
		fclose(fpout);

	return ret;
}
