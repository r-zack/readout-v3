/* DTT version: 5720 desktop (with DPP_PSD firmware)
 * CAEN library version: Rel. 2.6.8  - Nov 2015
 *
 * The module 'paramsHeaderToFile' offers a function that prints at the
 * beginning of a given FILE pointer a brief header containing a list of the
 * CAEN DTT/DPP-PSD parameter values, the digitizer's 'Link number' and the
 * acquisition time.
 *
 * 'paramsHeaderToFile' module version: a0.2
 */

#include <stdio.h>
#include "paramsHeaderToFile.h"
#include "myCAEN_DTT_config.h"
#include "Functions.h"
#include <CAENDigitizerType.h>


  /* returns the width of the interval between the higher DTT channel enabled
     and channel number 0 */
  static int getZeroToMaxChIntervalWidth(void);

  /* Writes to the beginning of the file pointed by the parameter a header
   * containing a textual representation of the CAEN DTT/DPP-PSD parameter
   * values and the acquisition time.
   *
   * @param fpout pointer to the file to write to.
   * @return 0 if the function returns normally, otherwise if file errors are
   * encountered returns a non zero integer and a description of the encountered
   * error is printed to stderr.
   */
  int printParamsHeaderToFile(FILE *fpout){
    int success = 0;
    char *communicationParComment = "# Communication parameters";
    char *acquisitionParComment = "# Acquisition parameters";
    char *dppParComment = "# DPP parameters (one line for each parameter)";
    char *acqTimeComment = "# Acquisition Time";
    register int i;
    register int zeroToMaxChIntervalWidth;

      zeroToMaxChIntervalWidth = getZeroToMaxChIntervalWidth();
      // Communication parameters
      if (fputs(communicationParComment, fpout) == EOF){
        perror("ParamsHeaderToFile - an error occurred while writing a file");
        success = 1;
        goto endPrintToFpout;
      }
      if (fprintf(fpout, "\n%d,%ld,%lu,%ld\n", linkNum, dttParams.LinkType, dttParams.VMEBaseAddress, dttParams.IOlev) < 9){
        fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
        success = 1;
        goto endPrintToFpout;
      }

      // Acquisition parameters
      if (fputs(acquisitionParComment, fpout) == EOF){
        perror("ParamsHeaderToFile - an error occurred while writing a file");
        success = 1;
        goto endPrintToFpout;
      }
      if (fprintf(fpout, "\n%ld,%lu,%#x,%d,%d\n", dttParams.AcqMode, dttParams.RecordLength, dttParams.ChannelMask, dttParams.EventAggr, dttParams.PulsePolarity) < 13){
        fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
        success = 1;
        goto endPrintToFpout;
      }

      // DPP-PSD parameters
      if (fputs(dppParComment, fpout) == EOF){
        perror("ParamsHeaderToFile - an error occurred while writing a file");
        success = 1;
        goto endPrintToFpout;
      }
      // thr
      for (i = 0; i < zeroToMaxChIntervalWidth; i++){
        if (i != 0){
          if(  fprintf(fpout, ",%d", dppParams.thr[i]) < 2  ){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
        else{           // i=0
          if(  fprintf(fpout, "\n%d", *(dppParams.thr)) < 2  ){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
      }
      // nsbl
      for (i = 0; i < zeroToMaxChIntervalWidth; i++){
        if (i != 0){
          if (fprintf(fpout, ",%d", dppParams.nsbl[i]) < 2){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
        else{           // i=0
          if (fprintf(fpout, "\n%d", *(dppParams.nsbl)) < 2){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
      }
      // lgate
      for (i = 0; i < zeroToMaxChIntervalWidth; i++){
        if (i != 0){
          if (fprintf(fpout, ",%d", dppParams.lgate[i]) < 2){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
        else{           // i=0
          if (fprintf(fpout, "\n%d", *(dppParams.lgate)) < 2){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
      }
      // sgate
      for (i = 0; i < zeroToMaxChIntervalWidth; i++){
        if (i != 0){
          if (fprintf(fpout, ",%d", dppParams.sgate[i]) < 2){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
        else{           // i=0
          if (fprintf(fpout, "\n%d", *(dppParams.sgate)) < 2){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
      }
      // pgate
      for (i = 0; i < zeroToMaxChIntervalWidth; i++){
        if (i != 0){
          if (fprintf(fpout, ",%d", dppParams.pgate[i]) < 2){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
        else{           // i=0
          if (fprintf(fpout, "\n%d", *(dppParams.pgate)) < 2){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
      }
      // selft
      for (i = 0; i < zeroToMaxChIntervalWidth; i++){
        if (i != 0){
          if (fprintf(fpout, ",%d", dppParams.selft[i]) < 2){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
        else{           // i=0
          if (fprintf(fpout, "\n%d", *(dppParams.selft)) < 2){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
      }
      // trgc
      for (i = 0; i < zeroToMaxChIntervalWidth; i++){
        if (i != 0){
          if (fprintf(fpout, ",%d", dppParams.trgc[i]) < 2){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
        else{           // i=0
          if (fprintf(fpout, "\n%d", *(dppParams.trgc)) < 2){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
      }
      // tvaw
      for (i = 0; i < zeroToMaxChIntervalWidth; i++){
        if (i != 0){
          if (fprintf(fpout, ",%d", dppParams.tvaw[i]) < 2){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
        else{           // i=0
          if (fprintf(fpout, "\n%d", *(dppParams.tvaw)) < 2){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
      }
      // csens
      for (i = 0; i < zeroToMaxChIntervalWidth; i++){
        if (i != 0){
          if (fprintf(fpout, ",%d", dppParams.csens[i]) < 2){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
        else{           // i=0
          if (fprintf(fpout, "\n%d", *(dppParams.csens)) < 2){
            fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
            success = 1;
            goto endPrintToFpout;
          }
        }
      }
      // purh, purgap, blthr, bltmo, trgho
      if(  fprintf(fpout, "\n%d\n%d\n%d\n%d\n%d\n", dppParams.purh, dppParams.purgap, dppParams.blthr, dppParams.bltmo, dppParams.trgho) < 11  ){
        fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
        success = 1;
        goto endPrintToFpout;
      }

      // Acquisition Time
      if(  fputs(acqTimeComment, fpout) == EOF  ){
        perror("ParamsHeaderToFile - an error occurred while writing a file");
        success = 1;
        goto endPrintToFpout;
      }
      if(  fprintf(fpout, "\n%lu\n\n", acqTime) < 4  ){
        fprintf(stderr, "ParamsHeaderToFile - an error occurred while writing a file");
        success = 1;
        goto endPrintToFpout;
      }


      if( fflush(fpout) ){
        perror("ParamsHeaderToFile - fflush() error");
        success = 2;
        goto endPrintToFpout;
      }

endPrintToFpout:
    return success;
  }

  /* Returns the width of the interval between the higher DTT channel enabled
     and channel number 0
  
     @return the width of the interval between the higher DTT channel enabled
     and channel number 0
  */
  static int getZeroToMaxChIntervalWidth(void){
    register int i = 0;
    register int j = 1;
    uint32_t zeroToMaxChIntervalWidth;
    int success = 0;
      if( sizeof(dttParams.ChannelMask) != 4 ){
        fputs("paramsHeaderToFile: fatal error! Expected a ChannelMask 32b long", stderr);
        exit(EXIT_FAILURE);
      }
      zeroToMaxChIntervalWidth = dttParams.ChannelMask;
      for (; i < 32; i++){
        if (zeroToMaxChIntervalWidth == 1){
          zeroToMaxChIntervalWidth = j;
          success = 1;
          break;
        }
        else{
          j++;
          zeroToMaxChIntervalWidth = zeroToMaxChIntervalWidth >> 1;
        }
      }
      if (!success){
        fputs("paramsHeaderToFile: fatal error! 0 channels were enabled", stderr);
        exit(EXIT_FAILURE);
      }

    return (int)zeroToMaxChIntervalWidth;
  }
