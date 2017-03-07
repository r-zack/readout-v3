/* This module permits to know if all multi-value parameters (i.e. one value for
 * each channel) of CAEN DTT are 'consistent' with the enable mask.
 * To be 'consistent' with the enable mask each parameter must have one value
 * for each channel described by the enable mask.
 *
 * the parameter are identified in this module by these ids:
 * 0. thr, 1.selft, 2.csens, 3.sgate, 4.lgate, 5.pgate, 6.tvaw, 7.nsbl,
 * 8.trgc
 *
 * 'multiValueParametersStats' module version: a0.1
 */

#include "multiValueParametersStats.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// the number of multi-value parameters considered by this module
#define MULTI_VALUE_PAR_NUMBER 9

/* array of flags each related to a multi-value parameter (i.e. one value for
 * each channel). For each parameter the flag is '1' if the parameter needs to
 * be 'updated', otherwise the flag is setted to '0'. A parameter is 'updated'
 * if its number of channel-related values is equal or greater than the interval
 * between channel 0 and the 'higher' enabled channel.
 */
static unsigned* paramNeedToUpdateList;
/* array of C-strings that describe the names of the multi-value parameters
 * considered by this module.
 */
static char** paramNamesList;
/* array that stores the number of values stored in each multi-value parameter
 * (i.e. one value for each channel).
 */
static unsigned* paramNumOfValues;
/* the width of the interval between the higher DTT channel enabled and channel
* number 0
*/
static unsigned zeroToMaxChIntervalWidth = 0;

/* Updates the list that stores the information about which parameters are
 * 'updated' and which aren't updated. A parameter will be marked as 'updated'
 * if its number of channel-related values will be equal or greater than the
 * interval between channel 0 and the 'higher' enabled channel.
 */
static void updateUpdatedParametersList(void);


/* Allocates the memory needed by the data structures used by this module and
 * initializes the structures 'paramNeedToUpdateList', 'paramNameList',
 * 'paramNumOfValues'.
 * Sets the status of the module considering the width of the interval between
 * the higher DTT channel 'enabled' and channel number 0 equal to
 * 'zeroToMaxChIntervalWidthValue'.
 * Initializes the module status to consider all the multi-value
 * parameters as having 'zeroToMaxChIntervalWidthValue' values.
 *
 * @param zeroToMaxChIntervalWidthValue the value used to set the status of the
 * program about the width of the interval between the higher DTT channel
 * 'enabled' and channel number 0
 */
void initMultiValueParametersStats(unsigned int zeroToMaxChIntervalWidthValue){
  register int i = 0;

  if ((paramNeedToUpdateList = (unsigned*)malloc(MULTI_VALUE_PAR_NUMBER*sizeof(unsigned))) == NULL){
    fputs("Error trying allocating memory", stderr);
    exit(EXIT_FAILURE);
  }
  for (; i < MULTI_VALUE_PAR_NUMBER; i++){
    *(paramNeedToUpdateList + i) = 0;
  }

  zeroToMaxChIntervalWidth = zeroToMaxChIntervalWidthValue;

  if ((paramNumOfValues = (unsigned*)malloc(MULTI_VALUE_PAR_NUMBER*sizeof(unsigned))) == NULL){
    fputs("Error trying allocating memory", stderr);
    exit(EXIT_FAILURE);
  }
  for (i = 0; i < MULTI_VALUE_PAR_NUMBER; i++){
    *(paramNumOfValues + i) = zeroToMaxChIntervalWidth;
  }

  if(  (paramNamesList = (char**)calloc(MULTI_VALUE_PAR_NUMBER, sizeof(char*))) == NULL  ){
    fputs("Error trying allocating memory", stderr);
    exit(EXIT_FAILURE);
  }
  // thr
  paramNamesList[0] = (char*)calloc(4, sizeof(char));
  // selft
  paramNamesList[1] = (char*)calloc(6, sizeof(char));
  // csens
  paramNamesList[2] = (char*)calloc(6, sizeof(char));
  // sgate
  paramNamesList[3] = (char*)calloc(6, sizeof(char));
  // lgate
  paramNamesList[4] = (char*)calloc(6, sizeof(char));
  // pgate
  paramNamesList[5] = (char*)calloc(6, sizeof(char));
  // tvaw
  paramNamesList[6] = (char*)calloc(5, sizeof(char));
  // nsbl
  paramNamesList[7] = (char*)calloc(5, sizeof(char));
  // trgc
  paramNamesList[8] = (char*)calloc(5, sizeof(char));
  for (i = 0; i < MULTI_VALUE_PAR_NUMBER; i++){
    if (*(paramNamesList + i) == NULL){
      fputs("Error trying allocating memory", stderr);
      exit(EXIT_FAILURE);
    }
  }
  strcpy(paramNamesList[0], "thr");
  strcpy(paramNamesList[1], "selft");
  strcpy(paramNamesList[2], "csens");
  strcpy(paramNamesList[3], "sgate");
  strcpy(paramNamesList[4], "lgate");
  strcpy(paramNamesList[5], "pgate");
  strcpy(paramNamesList[6], "tvaw");
  strcpy(paramNamesList[7], "nsbl");
  strcpy(paramNamesList[8], "trgc");
}

/* Frees 'paramNeedToUpdateList''s, 'paramNamesList''s and 'paramNumOfValues''s
 * allocated memory.
 */
void freeMultiValParamStatsDataStructuresMemory(void){
  register int i = 0;
    free(paramNeedToUpdateList);
    free(paramNumOfValues);
    for (i = 0; i < MULTI_VALUE_PAR_NUMBER; i++){
      free(*(paramNamesList + i));
    }
    free(paramNamesList);
}

/* 1. Sets the value of the variable that stores the width of the interval
 * between the higher DTT channel enabled and channel number 0 to 'value'.
 * 2. Calls updateUpdatedParametersList()
 *
 * @param value the value that will be stored
 */
void setMVPSZeroToMaxChIntervalWidth(unsigned int value){
  zeroToMaxChIntervalWidth = value;
  updateUpdatedParametersList();
}

/* Returns the value of the variable that stores the width of the interval
 * between the higher DTT channel enabled and channel number 0.
 *
 * @return the value of the variable that stores the width of the interval
 * between the higher DTT channel enabled and channel number 0
 */
unsigned getMVPSZeroToMaxChIntervalWidth(void){
  return zeroToMaxChIntervalWidth;
}


/* Prints to stdout the list of (comma-separated) parameters that are not
 * 'updated'.
 */
void printParamsToUpdate(void){
  register int i = 0;
  register unsigned count = 0;
    for (; i < MULTI_VALUE_PAR_NUMBER; i++){
      if (*(paramNeedToUpdateList + i)){
        if (count)
          printf(",%s", *(paramNamesList + i));
        else
          printf("%s", *(paramNamesList+i));
        count++;
      }
    }
}

/* Returns a non-zero integer if all the multi-value parameters do not need
 * to be updated to reach the minimum allowed number of channels configured.
 * Otherwise returns 0.
 *
 * @return a non-zero integer if all the multi-value parameters do not need
 * to be updated to reach the minimum allowed number of channels configured.
 * Otherwise returns 0.
 */
int areParamsUpdated(void){
  register int i = 0;
  int allUpdated = 1;
    for (; i < MULTI_VALUE_PAR_NUMBER; i++){
      if (*(paramNeedToUpdateList + i)){
        allUpdated = 0;
        break;
      }
    }
  return allUpdated;
}

/* Sets the status of a parameter to indicate that its number of
 * channel-related values is equal to the current width of the interval
 * between the higher DTT channel enabled and channel number 0.
 * If 'paramNo' is higher than the higher parameter id then the function
 * displays an error message to stderr and calls exit(EXIT_FAILURE).
 *
 * the parameter ids:
 * 0. thr, 1.selft, 2.csens, 3.sgate, 4.lgate, 5.pgate, 6.tvaw, 7.nsbl,
 * 8.trgc
 *
 * @param paramNo the number that identifies the parameter that should be
 * marked as 'updated'
 */
void setParamAsUpdated(unsigned paramNo){
  if (paramNo>MULTI_VALUE_PAR_NUMBER){
    fprintf(stderr, "setParamAsUpdated(): invalid paramNo");
    exit(EXIT_FAILURE);
  }
  *(paramNeedToUpdateList + paramNo) = 0;
  *(paramNumOfValues + paramNo) = zeroToMaxChIntervalWidth;
}

/* Updates the list that stores the information about which parameters are
 * 'updated' and which aren't updated. A parameter will be marked as 'updated'
 * if its number of channel-related values will be equal or greater than the
 * interval between channel 0 and the 'higher' enabled channel.
 */
static void updateUpdatedParametersList(void){
  register int i = 0;
    for (; i < MULTI_VALUE_PAR_NUMBER; i++){
      if (*(paramNumOfValues + i)<zeroToMaxChIntervalWidth)
        *(paramNeedToUpdateList + i) = 1;
      else
        *(paramNeedToUpdateList + i) = 0;
  }
}


#undef MULTI_VALUE_PAR_NUMBER
