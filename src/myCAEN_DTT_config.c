/* DTT version: 5720 desktop (with DPP_PSD firmware)
 * CAEN library version: Rel. 2.6.8  - Nov 2015
 *
 * The module 'myCAEN_DTT_config' offers a series of functions and data
 * structures to fill the CAEN DTT digitizer parameters 'dttParams', the
 * communication parameter 'linkNum', the DPP-PSD firmware parameters
 * 'dppParams' and the acquisition time 'acqTime' with values read from a config
 * text file. The module is also capable of giving interactive parameter editing
 * via shell.
 * The operation of 'parameter setting' from file or shell performs some error
 * checks before setting the values (the number of the first file line with an
 * error is displayed on 'stderr').
 * The module can inform the client IF any parameters are modified via shell
 * and which one were edited.
 * 'Print to stdout' functions are offered to display parameter values.
 * The configuration file could be updated after updating parameters.
 * The program could initialize a new configuration file if when launched no
 * configuration file is found.
 * The exported function 'acquireParameterValues()' returns a code to inform the
 * caller if the user asked to quit the program or if the acquisition should be
 * started.
 * 
 * 'myCAEN_DTT_config' module version: a0.4
 */

#include "myCAEN_DTT_config.h"
#include <CAENDigitizerType.h>
#include "multiValueParametersStats.h"
#include "defaultConfigFileBuilder.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "keyb.h"
#include "Functions.h"

#define MY_BUFF_SIZE 303
#define MAX_UNS_32BIT_INTEGER 4294967295

#define NO_OF_SECTIONS 4
#define SECT1_LEN 4
#define SECT2_LEN 5
#define SECT3_LEN 14
#define SECT4_LEN 1


DigitizerParams_t dttParams;
CAEN_DGTZ_DPP_PSD_Params_t dppParams;
unsigned long int acqTime = 0U;
int linkNum=0;

static char *myReadLine;
static FILE *myReadFile;
static FILE *myOutputFile;
static unsigned currentLineNo = 0;
/* string used to store the first two comma-separated values of each 'line' of
   DPP-PSD channel-specific parameters as shown to the user. Each line must follow
   the sintax <ch no.>,<enable bit>,<value> */
static char *chanSpecificParamInitialString;
/* the int value read by stdin while the user is modifying DPP-PSD parameters
 */
static int dppIntValueToSet;
/* the CAEN_DGTZ_DPP_TriggerConfig_t value read by stdin while the user is
 * modifying the trgc parameter
 */
static CAEN_DGTZ_DPP_TriggerConfig_t dppTrgcValueToSet;

/* the width of the interval between the higher DTT channel enabled and channel
   number 0 */
static int zeroToMaxChIntervalWidth = 0;
/* the width of the interval between the higher DTT channel enabled and channel
 * number 0 in the variable DigitizerParams_t.ChannelMask stored in the file
 * 'tdcr.ini' */
static int fileZeroToMaxChIntervalWidth = 0;
/* modifiedSection[num]=0 if section number 'num' wasn't modified by the user
 * since the last 'reset' of the structure, otherwise modifiedSection[num]!=0 
 */
static unsigned int modifiedSections[NO_OF_SECTIONS+1];
/* modifiedParameters[sectNum][paramNum]=0 if parameter number 'paramNum' in
 * section number 'sectNum' wasn't modified by the user since the last 'reset'
 * of the structure, otherwise modifiedParameters[sectNum][paramNum]!=0
 */
static unsigned int* modifiedParameters[NO_OF_SECTIONS+1];

/* 0 if myReadFile is opened, otherwise !=0 */
static unsigned int myReadFileClosed = 1;

// parser functions
static int DGTZ_LinkNumParseAndSet(void);
static int DGTZ_LinkTypeParseAndSet(void);
static int VMEBaseAddressParseAndSet(void);
static int DGTZ_IOLevelParseAndSet(void);
static int DPP_AcqModeParseAndSet(void);
static int DGTZ_RecordLengthParseAndSet(void);
static int DPP_EventAggrParseAndSet(void);
static int DGTZ_PulsePolarityParseAndSet(void);
static int DPP_purhParseAndSet(void);
static int DPP_purgapParseAndSet(void);
static int DPP_blthrParseAndSet(void);
static int DPP_bltmoParseAndSet(void);
static int DPP_trghoParseAndSet(void);
static int channelEnableMaskParseAndSet(void);
static int DPP_thrParseAndSet(void);
static int DPP_nsblParseAndSet(void);
static int DPP_lgateParseAndSet(void);
static int DPP_sgateParseAndSet(void);
static int DPP_pgateParseAndSet(void);
static int DPP_selftParseAndSet(void);
static int DPP_trgcParseAndSet(void);
static int DPP_tvawParseAndSet(void);
static int DPP_csensParseAndSet(void);

// check functions for DPP parameters
static int isDppPsdIntParameterValueValid(void);
static int isDppPsdTrgcParameterValueValid(void);

// print parameters' functions
static void printAllParameters(void);
static void printSingleSection(unsigned int);
static void printSingleSectionUpdatedView(unsigned int);
static void printAllParametersUpdatedView(void);
static void linkTypeValueToStr(char*);
static void iOLevelValueToStr(char*);
static void acqModeValueToStr(char*);
static void pulsePolarityValueToStr(char*);
static void printChThresholds(void);
static void printChNsbl(void);
static void printChLGate(void);
static void printChSGate(void);
static void printChPGate(void);
static void printChSelfTrg(void);
static void printChTrgConfig(void);
static void printChTvaw(void);
static void printChCsens(void);
static void purhValueToStr(char*);

static void updateChanSpecificParamInitialString(int);
static void printWarningAndListNotUpdatedParam(void);

// modified section / parameter arrays' functions
static void resetModifiedParametersArr(void);

// save to file functions
static int openFileToWrite(void);
static void saveCurrentParametersToFile(void);
static int copyToFileNextCommentedLines(void);

// utility functions
static void resetCharOutputBuff(char*, int);

// other functions
static int interactiveParamsModify(void);
static void setZeroToMaxChIntervalWidth(void);
static int wereParametersModified(void);


/* Initializes buffers 'myReadLine', 'chanSpecificParamInitialString', opens
   file "tdcr.ini", initializes 'modifiedParameters' structures
 */
static void initMyCAEN_DTT_config(void){
  if(  (myReadLine=(char*)calloc(MY_BUFF_SIZE, sizeof(char))) == NULL  ){
    fputs("Error trying allocating memory",stderr);
    exit(EXIT_FAILURE);
  }
  if(  (chanSpecificParamInitialString=(char*)calloc(6, sizeof(char))) == NULL  ){
    fputs("Error trying allocating memory", stderr);
    exit(EXIT_FAILURE);
  }
  if( (myReadFile=fopen("tdcr.ini", "r"))==NULL ){
    printf("\"tdcr.ini\" not found. Creating a default configuration file...");
    createDefaultConfigFile(0);
    printf(" Done!\n");
    if(  (myReadFile = fopen("tdcr.ini", "r")) == NULL  ){
      perror("An error occurred while trying to open \"tdcr.ini\"");
      exit(EXIT_FAILURE);
    }
  }
  myReadFileClosed = 0;
  if(  (modifiedParameters[1] = (unsigned int*)malloc( (1+SECT1_LEN)*sizeof(int) )) == NULL  ){
    fputs("Error trying allocating memory", stderr);
    exit(EXIT_FAILURE);
  }
  if (  (modifiedParameters[2] = (unsigned int*)malloc( (1+SECT2_LEN)*sizeof(int) )) == NULL  ){
    fputs("Error trying allocating memory", stderr);
    exit(EXIT_FAILURE);
  }
  if (  (modifiedParameters[3] = (unsigned int*)malloc( (1+SECT3_LEN)*sizeof(int) )) == NULL  ){
    fputs("Error trying allocating memory", stderr);
    exit(EXIT_FAILURE);
  }
  if (  (modifiedParameters[4] = (unsigned int*)malloc( (1+SECT4_LEN)*sizeof(int) )) == NULL  ){
    fputs("Error trying allocating memory", stderr);
    exit(EXIT_FAILURE);
  }

  resetModifiedParametersArr();
}

/* Tries to open a file to save the current parameters. In case of error warns
 * the user printing an error message to stderr.
 * In case of failure returns 0, otherwise returns a non-zero integer. 
 *
 * @return returns 0 in case of failure, otherwise a non-zero integer
 */
static int openFileToWrite(void){
  int success = 0;
    if(  (myOutputFile = fopen("updated_tdcr_temp","w")) == NULL  ){
      perror("Opening output config file failed");
    }
    else
      success = 1;
  return success;
}

/* Frees 'myReadLine''s, 'chanSpecificParamInitialString''s and
   'modifiedParameters[]' memory, closes 'myReadFile'
 */
static void freeDataStructureMemory(void){
  register int i = 0;
    if( !myReadFileClosed ){
      if (fclose(myReadFile) == EOF){
        fputs("Error closing \"tdcr.ini\" !", stderr);
      }
    }
    free(myReadLine);
    free(chanSpecificParamInitialString);
    for (i = 1; i < NO_OF_SECTIONS+1; i++){
      free( modifiedParameters[i] );
    }
}

/* Intializes to 0 all the elements from index 1 to the last element of each
 * 'modifiedSections' array and of 'modifiedParameters'.
 */
static void resetModifiedParametersArr(void){
  register int i = 0;
    for (i = 1; i < NO_OF_SECTIONS + 1; i++){
      *(modifiedSections + i) = 0;
    }
    for (i = 1; i < SECT1_LEN + 1; i++){
      *(modifiedParameters[1] + i) = 0;
    }
    for (i = 1; i < SECT2_LEN + 1; i++){
      *(modifiedParameters[2] + i) = 0;
    }
    for (i = 1; i < SECT3_LEN + 1; i++){
      *(modifiedParameters[3] + i) = 0;
    }
    *(modifiedParameters[4] + 1) = 0;
}

/* Returns a non-zero integer if one or more parameters were modified since the
 * last call of resetModifiedParametersArr(). Otherwise returs 0.
 *
 * @return 0 if no parameters were modified since the last call of
 * resetModifiedParametersArr(), otherwise returns a non-zero integer
 */
static int wereParametersModified(void){
  int modified = 0;
  register int i = 1;
    for (; i < NO_OF_SECTIONS + 1; i++){
      if( modifiedSections[i] ){
        modified = 1;
        break;
      }
    }
  return modified;
}

/* Copies a channel number, a ',', an enable bit then another ',' into
 * 'chanSpecificParamInitialString'. The channel number is set to 0 the first
 * time the function is called and after each call, the channel number is 
 * increased by 1. The 'enable' bit is read from dttParams.ChannelMask: the
 * first time the function is called it copies the least significant bit of
 * the channel mask; every time the function is called the enable bit copied is
 * the more significant bit next to the last one read.
 * The function is put to the initial state if the parameter 'reset' is not 0.
 * The original content of 'chanSpecificParamInitialString' is erased every
 * time the function is called.
 *
 * @param reset if not 0 the function is reset to the initial state
 */
static void updateChanSpecificParamInitialString(int reset){
  static int ch = 0;
  static unsigned int scannerBit = 1;
  static int i = 0;
  char tempStr[6] = { '\0', '\0', '\0', '\0', '\0', '\0' };
    if (reset){
      ch = i = 0;
      scannerBit = 1;
    }
    resetCharOutputBuff(chanSpecificParamInitialString,5);

    sprintf(tempStr, "%d,%u,", ch, (dttParams.ChannelMask & scannerBit) >> i);
    strcpy(chanSpecificParamInitialString, tempStr);

    ch++;
    i++;
    scannerBit = scannerBit << 1;
}

/* Copies all the elements of 'myReadLine' from the first "valid" char to the
 * last one.
 * N.B. "valid char"=ASCII code belonging to the range [33-126]
 * The elements are copied to the beginning of 'myReadLine'. After the last
 * char is copied the string is NUL teminated.
 *
 * If the first "valid" char comes after the first occurrence of CR / LF / '\0'
 * then 'myReadLine' is zeroed!
 */
static void cleanMyReadLine(void){
  int curStart;
  int curEnd=curStart=-1;
  int validCharCount=0;
  register int i;
  register int limit= MY_BUFF_SIZE - 1;

    // put 'curStart' on the first "valid" character of 'myReadLine'
    for (i = 0; i < limit; i++){
      if (  (myReadLine[i] > 32 && myReadLine[i] < 127)  ){
        curStart = i;
        break;
      }
      else if (  (myReadLine[i] == 10) || (myReadLine[i] == 13) || (myReadLine[i] == '\0')  ){
        curStart = -1;
        break;
      }
    }

    if (curStart != -1){
      /* assign to 'curEnd' the value of the index of the last "valid" char +1 
       */
      for (i = limit-1; i > curStart; i--){
        if (  (myReadLine[i] > 32) && (myReadLine[i] < 127)  ){
          curEnd = i+1;
          break;
        }
      }
      if (curEnd == -1)
        curEnd= curStart+1;
      // "clean" the string by copying the "good" part and terminate it
      validCharCount= curEnd - curStart;
      memmove(myReadLine, myReadLine + curStart, validCharCount);
      *(myReadLine + validCharCount) = '\0';
    }
    else{
      memset(myReadLine, '\0', MY_BUFF_SIZE);     // blanks myReadLine
    }

}

/* Blanks 'myReadLine' and fills it with the next valid (=uncommented and not
 * empty) line in 'myReadFile'.
 * Returns 0 if EOF is reached or in case of read error, otherwise an int other
 * than 0. If 0 is returned you could check feof(myReadFile)
 * and ferror(myReadFile). In case of read error perror() is called by
 * this function.
 *
 * @return 0 if reached the EOF or in case of read error, otherwise an int
 * other than 0
 */
static int scanNextLine(void){
  // 0 if myReadFile should not be read anymore
  int continueScanningUntilEOF= 1;
  // not 0 if myReadLine should be parsed
  int foundValidLine = 0;
// no check is performed to identify if a line is longer than the imposed limit

    memset(myReadLine, '\0', MY_BUFF_SIZE);       // blanks myReadLine
    while (  fgets(myReadLine, MY_BUFF_SIZE - 1, myReadFile) != NULL  ){
      currentLineNo++;
      cleanMyReadLine();
      if (  (*myReadLine != '\0')&&(*myReadLine != '#')  ){     // skip empty
        foundValidLine= 1;                              //  lines and comments
        break;
      }
      else
        memset(myReadLine, '\0', MY_BUFF_SIZE);
    }
    if (!foundValidLine){
      if (feof(myReadFile)){
        continueScanningUntilEOF= 0;
      }
      else{
        if (ferror(myReadFile)){
          continueScanningUntilEOF= 0;
          perror(NULL);
        }
      }
    }

  return continueScanningUntilEOF;
}

/* Blanks 'myReadLine' and fills it with the next not empty line taken from
 * stdin. 'myReadLine' will contain <=MY_BUFF_SIZE char: the read of stdin will
 * be halted when a "\n" is read or when it is reached the MY_BUFF_SIZEth
 * character.
 * Returns 0 in case of read error, otherwise an int other than 0. In case of
 * read error perror() is called by this function.
 *
 * @return 0 in case of read error, otherwise an int other than 0
 */
static int scanNextLineStdin(void){
  // 0 if read error / found EOF
  int continueScanningUntilEOF = 1;
  // not 0 if myReadLine should be parsed
  int foundValidLine = 0;
// no check is performed to identify if a line is longer than the imposed limit!

    memset(myReadLine, '\0', MY_BUFF_SIZE);       // blanks myReadLine
    while(  fgets(myReadLine, MY_BUFF_SIZE - 1, stdin) != NULL  ){
      cleanMyReadLine();
      if(  (*myReadLine != '\0') && (*myReadLine != '#')  ){     // skip empty
        foundValidLine = 1;                              //  lines and comments
        break;
      }
      else
        memset(myReadLine, '\0', MY_BUFF_SIZE);
    }
    if( !foundValidLine ){
      if( feof(stdin) ){
        continueScanningUntilEOF = 0;
      }
      else{
        if( ferror(stdin) ){
          continueScanningUntilEOF = 0;
          perror(NULL);
        }
      }
    }

  return continueScanningUntilEOF;
}

/* Sets 'zeroToMaxChIntervalWidth' analyzing the channel enable mask.
 * 'zeroToMaxChIntervalWidth' is used for sintax checks on configuration inputs
 * and for memory allocation of temporary variables.
 *
 * e.g. channel enable mask: ch3=0 ch2=1 ch1=0 ch0=1 -> 0toMaxChIntWidth=4-1=3
 * channel enable mask: ch3=1 ch2=1 ch1=0 ch0=0 -> 0toMaxChIntWidth=4-0=4
 * channel enable mask: ch3=0 ch2=0 ch1=1 ch0=1 -> 0toMaxChIntWidth=4-2=2
 *
 * N.B. this function requires that 'dttParams.ChannelMask' is 32b long. If
 * the channel mask variable is not 32b long the function must be modified to
 * work properly!
 */
static void setZeroToMaxChIntervalWidth(void){
  register int i = 0;
  register int j = 1;
  int success= 0;
    if (  sizeof(dttParams.ChannelMask)!= 4  ){
      fputs("myCAEN_DTT_config: fatal error! Expected a ChannelMask 32b long", stderr);
      exit(EXIT_FAILURE);
    }
    zeroToMaxChIntervalWidth= dttParams.ChannelMask;
    for (; i < 32; i++){
      if (zeroToMaxChIntervalWidth == 1){
        zeroToMaxChIntervalWidth= j;
        success= 1;
        break;
      }
      else{
        j++;
        zeroToMaxChIntervalWidth= zeroToMaxChIntervalWidth >> 1;
      }
    }
    if (!success){
      fputs("myCAEN_DTT_config: fatal error! 0 channels were enabled", stderr);
      exit(EXIT_FAILURE);
    }
}

/* Calls initMyCAEN_DTT_config(), parses "tdcr.ini" to fill 'dttParams',
 * 'dppParams', 'linkNum' and 'acqTime' then calls freeDataStructureMemory() to
 * permit interactive parameter editing via shell.
 *
 * @return 0 if the user asked to quit the program, 1 if the user asked to start
 * the acquisition
 */
int acquireParameterValues(void){
  int returnVal = 0;
  int multiValueParametersStatsInitialized = 0;
    initMyCAEN_DTT_config();

    printf("Parsing file \'tdcr.ini\'...");

    /*
     *    #####  Communication parameters  #####
     */
    // LinkNum
    if (  !( scanNextLine() && DGTZ_LinkNumParseAndSet() )  ){
      fprintf(stderr, "\ntdcr.ini: error at line: %d\n", currentLineNo);
      goto parserEnd;
    }
    // CAEN_DGTZ_LinkType
    if (  !( scanNextLine() && DGTZ_LinkTypeParseAndSet() )  ){
      fprintf(stderr, "\ntdcr.ini: error at line: %d\n", currentLineNo);
      goto parserEnd;
    }
    // VMEBaseAddress
    if (  !( scanNextLine() && VMEBaseAddressParseAndSet() )  ){
      fprintf(stderr, "\ntdcr.ini: error at line: %d\n", currentLineNo);
      goto parserEnd;
    }
    // CAEN_DGTZ_IOLevel
    if (  !( scanNextLine() && DGTZ_IOLevelParseAndSet() )  ){
      fprintf(stderr, "\ntdcr.ini: error at line: %d\n", currentLineNo);
      goto parserEnd;
    }
    /*
     *    #####  Acquisition parameters  #####
     */
    // DPP_AcqMode
    if (  !( scanNextLine() && DPP_AcqModeParseAndSet() )  ){
      fprintf(stderr, "\ntdcr.ini: error at line: %d\n", currentLineNo);
      goto parserEnd;
    }
    // DGTZ_RecordLength
    if (  !(scanNextLine() && DGTZ_RecordLengthParseAndSet())  ){
      fprintf(stderr, "\ntdcr.ini: error at line: %d\n", currentLineNo);
      goto parserEnd;
    }
    // Channel enable mask
    if (  !(scanNextLine() && channelEnableMaskParseAndSet())  ){
      fprintf(stderr, "\ntdcr.ini: error at line: %d\n", currentLineNo);
      goto parserEnd;
    }
    else{
      setZeroToMaxChIntervalWidth();
      fileZeroToMaxChIntervalWidth = zeroToMaxChIntervalWidth;
    }
    // EventAggr
    if (  !(scanNextLine() && DPP_EventAggrParseAndSet())  ){
      fprintf(stderr, "\ntdcr.ini: error at line: %d\n", currentLineNo);
      goto parserEnd;
    }
    // DGTZ_PulsePolarity
    if (  !(scanNextLine() && DGTZ_PulsePolarityParseAndSet())  ){
      fprintf(stderr, "\ntdcr.ini: error at line: %d\n", currentLineNo);
      goto parserEnd;
    }
    /*
     *    #####      DPP parameters      #####
     */
    // DPP_thr
    if ( !DPP_thrParseAndSet() ){
      goto parserEnd;
    }
    // DPP_nsbl
    if ( !DPP_nsblParseAndSet() ){
      goto parserEnd;
    }
    // DPP_lgate
    if ( !DPP_lgateParseAndSet() ){
      goto parserEnd;
    }
    // DPP_sgate
    if ( !DPP_sgateParseAndSet() ){
      goto parserEnd;
    }
    // DPP_pgate
    if ( !DPP_pgateParseAndSet() ){
      goto parserEnd;
    }
    // DPP_selft
    if ( !DPP_selftParseAndSet() ){
      goto parserEnd;
    }
    // DPP_trgc
    if ( !DPP_trgcParseAndSet() ){
      goto parserEnd;
    }
    // DPP_tvaw
    if ( !DPP_tvawParseAndSet() ){
      goto parserEnd;
    }
    // DPP_csens
    if ( !DPP_csensParseAndSet() ){
      goto parserEnd;
    }
    // DPP_purh
    if (  !(scanNextLine() && DPP_purhParseAndSet())  ){
      fprintf(stderr, "\ntdcr.ini: error at line: %d\n", currentLineNo);
      goto parserEnd;
    }
    // DPP_purgap
    if (  !(scanNextLine() && DPP_purgapParseAndSet())  ){
      fprintf(stderr, "\ntdcr.ini: error at line: %d\n", currentLineNo);
      goto parserEnd;
    }
    // DPP_blthr
    if (  !(scanNextLine() && DPP_blthrParseAndSet())  ){
      fprintf(stderr, "\ntdcr.ini: error at line: %d\n", currentLineNo);
      goto parserEnd;
    }
    // DPP_bltmo
    if (  !(scanNextLine() && DPP_bltmoParseAndSet())  ){
      fprintf(stderr, "\ntdcr.ini: error at line: %d\n", currentLineNo);
      goto parserEnd;
    }
    // DPP_trgho
    if (  !(scanNextLine() && DPP_trghoParseAndSet())  ){
      fprintf(stderr, "\ntdcr.ini: error at line: %d\n", currentLineNo);
      goto parserEnd;
    }
    // Acquisition time (milliseconds)
    if ( scanNextLine() ){
      if (myReadLine != NULL){
        errno = 0;
        acqTime = strtoul(myReadLine, NULL, 10);
        if (acqTime == 0){
          // check if strtoul() returned 0 because of 'invalid' string argument
          if (strcmp(myReadLine, "0") != 0){
            fprintf(stderr, "\ntdcr.ini: error at line: %d\n", currentLineNo);
            goto parserEnd;
          }
        }
        else if (acqTime == ULONG_MAX){
          /* check if strtoul() returned ULONG_MAX because of out of range string
          argument */
          if (errno == ERANGE){
            fprintf(stderr, "\ntdcr.ini: error at line: %d\n", currentLineNo);
            goto parserEnd;
          }
        }
      }
      else
        goto parserEnd;
    }
    else
      goto parserEnd;

    printf(" Done.\n\nActual parameters:\n\n");
    initMultiValueParametersStats(zeroToMaxChIntervalWidth);
    multiValueParametersStatsInitialized = 1;
    returnVal = interactiveParamsModify();


parserEnd:
    freeDataStructureMemory();
    if (multiValueParametersStatsInitialized)
      freeMultiValParamStatsDataStructuresMemory();


  return returnVal;

}

/* Function that asks the user via stdin/stdout which parameters he would like
 * to modify and then stores the values in the right variables. The function
 * performs some error checks.
 *
 * @return 0 if the user asked to quit the program, 1 if the user asked to start
 * the acquisition
 */
static int interactiveParamsModify(void){
  int returnVal = 0;
  int chRead=0;
  int stayHere = 1;
  unsigned long int acqTimeTemp;
  int menuChoice=-1;
  int readMenuNumberOK = -1;
  int tempDPP_int_values[MAX_DPP_PSD_CHANNEL_SIZE];
  CAEN_DGTZ_DPP_TriggerConfig_t tempDPP_trgc_values[MAX_DPP_PSD_CHANNEL_SIZE];
  register int i = 0;


menu1:
    printAllParameters();
  menu1A:
    if( wereParametersModified() ){
      /* if multi-value parameters are not consistent with ch.mask, warn the 
       * user and hide 'saving to file' and 'start acquisition' options
       */
      if( !areParamsUpdated() ){
        printWarningAndListNotUpdatedParam();
        printf("\nEnter \'M\' to modifiy parameters, \'P\' to show parameters that were modified, or \'Q\' to quit:\n");
      }
      else
        printf("\nEnter \'S\' to start the acquisition, \'M\' to modifiy parameters, \'P\' to show parameters that were modified, \'F\' to save current parameters to file or \'Q\' to quit:\n");
    }
    else{
      printf("\nEnter \'S\' to start the acquisition, \'M\' to modifiy parameters or \'Q\' to quit:\n");
    }
    stayHere = 1;
    do{
      chRead = getch();
      if( wereParametersModified() ){
        if( !areParamsUpdated() ){
          if(  (chRead == 'm') || (chRead == 'M') || (chRead == 'q')
              || (chRead == 'Q') || (chRead == 'p') || (chRead == 'P')  )
            stayHere = 0;
        }
        else{
          if(  (chRead == 's') || (chRead == 'S') || (chRead == 'm')
              || (chRead == 'M') || (chRead == 'q') || (chRead == 'Q')
              || (chRead == 'p') || (chRead == 'P') || (chRead == 'f')
              || (chRead == 'F')  )
            stayHere = 0;
        }
      }
      else{
        if( (chRead == 's') || (chRead == 'S') || (chRead == 'm')
            || (chRead == 'M') || (chRead == 'q') || (chRead == 'Q') )
          stayHere = 0;
      }
    } while (stayHere);

    /*
     *    #####      Modify parameters     #####
     */
    if(  (chRead == 'm') || (chRead == 'M')  ){
menu2:
      printAllParameters();
      // if multi-value parameters are not consistent with ch.mask, warn the user
      if (!areParamsUpdated()){
        printWarningAndListNotUpdatedParam();
      }
      printf("\nChoose the block number (1-4) to which the parameter belongs to (press TAB to return to the previous menu):\n");
      stayHere = 1;
      do{
        chRead = getch();
        if(  (chRead == '1') || (chRead == '2') || (chRead == '3')
            || (chRead == '4') || (chRead == '\t')  )
          stayHere = 0;
      } while (stayHere);
      if (chRead == '1'){
menu3:
        /*
         *    #####        Modify Comm.Params       #####
         */
        puts("");
        printSingleSection(1);
        // if multi-value parameters are not consistent with ch.mask, warn the user
        if( !areParamsUpdated() ){
          printWarningAndListNotUpdatedParam();
        }
        printf("Type the number (1-4) of the parameter you wish to change (press TAB to return to the previous menu):\n");
        stayHere = 1;
        do{
          chRead = getch();
          if(  (chRead == '1') || (chRead == '2') || (chRead == '3')
              || (chRead == '4') || (chRead == '\t')  )
            stayHere = 0;
        } while (stayHere);
        if (chRead == '1'){
          /*
           *    #####         Modify LinkNum         #####
           */
          printf("\nEnter the new value for parameter \'Link number\':\n");
          if( scanNextLineStdin() ){
            if( DGTZ_LinkNumParseAndSet() ){
              modifiedSections[1] = modifiedParameters[1][1] = 1;
              printf("\n\'Link number\' value modified.\n");
              goto menu3;
            }
            else{
              printf("\nInvalid value.\n");
              goto menu3;
            }
          }
          else{
            printf("Read error.\n");
            goto menu3;
          }
        }
        if (chRead == '2'){
          /*
           *    #####         Modify LinkType        #####
           */
          printf("\nEnter the new value for parameter \'Link type\':\n");
          if (scanNextLineStdin()){
            if (DGTZ_LinkTypeParseAndSet()){
              modifiedSections[1] = modifiedParameters[1][2] = 1;
              printf("\n\'Link type\' value modified.\n");
              goto menu3;
            }
            else{
              printf("\nInvalid value.\n");
              goto menu3;
            }
          }
          else{
            printf("Read error.\n");
            goto menu3;
          }
        }
        else if (chRead == '3'){
          /*
           *    #####      Modify VMEBaseAddress     #####
           */
          printf("\nEnter the new value for parameter \'VMEBaseAddress\':\n");
          if( scanNextLineStdin() ){
            if( VMEBaseAddressParseAndSet() ){
              modifiedSections[1] = modifiedParameters[1][3] = 1;
              printf("\n\'VMEBaseAddress\' value modified.\n");
              goto menu3;
            }
            else{
              printf("\nInvalid value.\n");
              goto menu3;
            }
          }
          else{
            printf("Read error.\n");
            goto menu3;
          }
        }
        else if (chRead == '4'){
          /*
           *    #####     Modify CAEN_DGTZ_IOLevel   #####
           */
          printf("\nEnter the new value for parameter \'CAEN_DGTZ_IOLevel\':\n");
          if( scanNextLineStdin() ){
            if( DGTZ_IOLevelParseAndSet() ){
              modifiedSections[1] = modifiedParameters[1][4] = 1;
              printf("\n\'CAEN_DGTZ_IOLevel\' value modified.\n");
              goto menu3;
            }
            else{
              printf("\nInvalid value.\n");
              goto menu3;
            }
          }
          else{
            printf("Read error.\n");
            goto menu3;
          }
        }
        else if (chRead == '\t'){
          puts("");
          goto menu2;
        }
      }               // end if() 'modify 'communication parameters''
      else if (chRead == '2'){
menu4:
        /*
         *    #####      Modify Acquisit.Params     #####
         */
        puts("");
        printSingleSection(2);
        // if multi-value parameters are not consistent with ch.mask, warn the user
        if( !areParamsUpdated() ){
          printWarningAndListNotUpdatedParam();
        }
        printf("Type the number (1-5) of the parameter you wish to change (press TAB to return to the previous menu):\n");
        stayHere = 1;
        do{
          chRead = getch();
          if ((chRead == '1') || (chRead == '2') || (chRead == '3') || (chRead == '4')
            || (chRead == '5') || (chRead == '\t'))
            stayHere = 0;
        } while (stayHere);
        if (chRead == '1'){
          /*
           *    #####   Modify DPP_Acquisition Mode  #####
           */
          printf("\nEnter the new value for parameter \'DPP_AcqMode\':\n");
          if( scanNextLineStdin() ){
            if( DPP_AcqModeParseAndSet() ){
              modifiedSections[2] = modifiedParameters[2][1] = 1;
              printf("\n\'DPP_AcqMode\' value modified.\n");
              goto menu4;
            }
            else{
              printf("\nInvalid value.\n");
              goto menu4;
            }
          }
          else{
            printf("Read error.\n");
            goto menu4;
          }
        }
        else if (chRead == '2'){
          /*
           *    #####       Modify RecordLength      #####
           */
          printf("\nEnter the new value for parameter \'RecordLength\':\n");
          if( scanNextLineStdin() ){
            if( DGTZ_RecordLengthParseAndSet() ){
              modifiedSections[2] = modifiedParameters[2][2] = 1;
              printf("\n\'RecordLength\' value modified.\n");
              goto menu4;
            }
            else{
              printf("\nInvalid value.\n");
              goto menu4;
            }
          }
          else{
            printf("Read error.\n");
            goto menu4;
          }
        }
        else if (chRead == '3'){
          /*
           *    #####   Modify Channel enable mask   #####
           */
          printf("\nEnter the new value for parameter \'ChannelMask\':\n0x");
          if( scanNextLineStdin() ){
            if( channelEnableMaskParseAndSet() ){
              modifiedSections[2] = modifiedParameters[2][3] = 1;
              printf("\n\'ChannelMask\' value modified.\n");
              setZeroToMaxChIntervalWidth();
              setMVPSZeroToMaxChIntervalWidth(zeroToMaxChIntervalWidth);
              goto menu4;
            }
            else{
              printf("\nInvalid value.\n");
              goto menu4;
            }
          }
          else{
            printf("Read error.\n");
            goto menu4;
          }
        }
        else if (chRead == '4'){
          /*
           *    #####         Modify EventAggr       #####
           */
          printf("\nEnter the new value for parameter \'EventAggr\':\n");
          if( scanNextLineStdin() ){
            if( DPP_EventAggrParseAndSet() ){
              modifiedSections[2] = modifiedParameters[2][4] = 1;
              printf("\n\'EventAggr\' value modified.\n");
              goto menu4;
            }
            else{
              printf("\nInvalid value.\n");
              goto menu4;
            }
          }
          else{
            printf("Read error.\n");
            goto menu4;
          }
        }
        else if (chRead == '5'){
          /*
           *    #####       Modify PulsePolarity     #####
           */
          printf("\nEnter the new value for parameter \'PulsePolarity\':\n");
          if( scanNextLineStdin() ){
            if( DGTZ_PulsePolarityParseAndSet() ){
              modifiedSections[2] = modifiedParameters[2][5] = 1;
              printf("\n\'PulsePolarity\' value modified.\n");
              goto menu4;
            }
            else{
              printf("\nInvalid value.\n");
              goto menu4;
            }
          }
          else{
            printf("Read error.\n");
            goto menu4;
          }
        }
        else if (chRead == '\t'){
          puts("");
          goto menu2;
        }
      }               // end if() 'modify 'Acquisition Parameters''
      else if (chRead == '3'){
menu5:
        /*
         *    #####      Modify DPP_PSD.Params      #####
         */
        puts("");
        printSingleSection(3);
        // if multi-value parameters are not consistent with ch.mask, warn the user
        if (!areParamsUpdated()){
          printWarningAndListNotUpdatedParam();
        }
        stayHere = 1;       
        do{
          readMenuNumberOK = 0;
          printf("Type the number (1-14) of the parameter you wish to change, then press ENTER (type 'back', then press ENTER, to return to the previous menu):\n");
          if( scanNextLineStdin() ){
            if(  strncmp(myReadLine,"back",4) == 0  ){
              puts("");
              goto menu2;
            }
            else if(  !((menuChoice=atoi(myReadLine)) < 1) && !(menuChoice > 14)  ){
              readMenuNumberOK = 1;
              stayHere = 0;
            }
          }
          else{
            printf("Read error.\n");
          }          
        } while (stayHere);
        if (readMenuNumberOK){
          if (menuChoice == 1){
            /*
             *    #####      Modify Trigg.Threshold    #####
             */
            printf("\nEnter the new values for parameter \'Trigger Threshold\':\n(ch. number,ch. enable,trigger threshold value)\n");
            for(i = 0; i < zeroToMaxChIntervalWidth; i++){
              i ? updateChanSpecificParamInitialString(0) : updateChanSpecificParamInitialString(1);
              printf("%s", chanSpecificParamInitialString);
              if( scanNextLineStdin() ){
                if( isDppPsdIntParameterValueValid() ){
                  tempDPP_int_values[i] = dppIntValueToSet;
                }
                else{
                  printf("\nInvalid value.\n");
                  goto menu5;
                }
              }
              else{
                printf("Read error.\n");
                goto menu5;
              }
            }
            for (i = 0; i < zeroToMaxChIntervalWidth; i++){
              dppParams.thr[i] = tempDPP_int_values[i];
            }
            modifiedSections[3] = modifiedParameters[3][1] = 1;
            // set 'thr' as updated
            setParamAsUpdated(0);
            printf("\n\'Trigger Threshold\' value modified.\n");
            goto menu5;
          }
          else if (menuChoice == 2){
            /*
             *    #####          Modify nsbl         #####
             */
            printf("\nEnter the new values for parameter \'nsbl\' (no. of samples for baseline mean):\n(ch. number,ch. enable,nsbl value)\n");
            for (i = 0; i < zeroToMaxChIntervalWidth; i++){
              i ? updateChanSpecificParamInitialString(0) : updateChanSpecificParamInitialString(1);
              printf("%s", chanSpecificParamInitialString);
              if (scanNextLineStdin()){
                if( isDppPsdIntParameterValueValid() ){
                  tempDPP_int_values[i] = dppIntValueToSet;
                }
                else{
                  printf("\nInvalid value.\n");
                  goto menu5;
                }
              }
              else{
                printf("Read error.\n");
                goto menu5;
              }
            }
            for (i = 0; i < zeroToMaxChIntervalWidth; i++){
              dppParams.nsbl[i] = tempDPP_int_values[i];
            }
            modifiedSections[3] = modifiedParameters[3][2] = 1;
            // set 'nsbl' as updated
            setParamAsUpdated(7);
            printf("\n\'nsbl\' value modified.\n");
            goto menu5;
          }
          else if (menuChoice == 3){
            /*
             *    #####         Modify lgate         #####
             */
            printf("\nEnter the new values for parameter \'lgate\':\n(ch. number,ch. enable,lgate value)\n");
            for (i = 0; i < zeroToMaxChIntervalWidth; i++){
              i ? updateChanSpecificParamInitialString(0) : updateChanSpecificParamInitialString(1);
              printf("%s", chanSpecificParamInitialString);
              if( scanNextLineStdin() ){
                if( isDppPsdIntParameterValueValid() ){
                  tempDPP_int_values[i] = dppIntValueToSet;
                }
                else{
                  printf("\nInvalid value.\n");
                  goto menu5;
                }
              }
              else{
                printf("Read error.\n");
                goto menu5;
              }
            }
            for(i = 0; i < zeroToMaxChIntervalWidth; i++){
              dppParams.lgate[i] = tempDPP_int_values[i];
            }
            modifiedSections[3] = modifiedParameters[3][3] = 1;
            // set 'lgate' as updated
            setParamAsUpdated(4);
            printf("\n\'lgate\' value modified.\n");
            goto menu5;
          }
          else if (menuChoice == 4){
            /*
             *    #####         Modify sgate         #####
             */
            printf("\nEnter the new values for parameter \'sgate\':\n(ch. number,ch. enable,sgate value)\n");
            for (i = 0; i < zeroToMaxChIntervalWidth; i++){
              i ? updateChanSpecificParamInitialString(0) : updateChanSpecificParamInitialString(1);
              printf("%s", chanSpecificParamInitialString);
              if( scanNextLineStdin() ){
                if( isDppPsdIntParameterValueValid() ){
                  tempDPP_int_values[i] = dppIntValueToSet;
                }
                else{
                  printf("\nInvalid value.\n");
                  goto menu5;
                }
              }
              else{
                printf("Read error.\n");
                goto menu5;
              }
            }
            for(i = 0; i < zeroToMaxChIntervalWidth; i++){
              dppParams.sgate[i] = tempDPP_int_values[i];
            }
            modifiedSections[3] = modifiedParameters[3][4] = 1;
            // set 'sgate' as updated
            setParamAsUpdated(3);
            printf("\n\'sgate\' value modified.\n");
            goto menu5;
          }
          else if (menuChoice == 5){
            /*
             *    #####         Modify pgate         #####
             */
            printf("\nEnter the new values for parameter \'pgate\':\n(ch. number,ch. enable,pgate value)\n");
            for(i = 0; i < zeroToMaxChIntervalWidth; i++){
              i ? updateChanSpecificParamInitialString(0) : updateChanSpecificParamInitialString(1);
              printf("%s", chanSpecificParamInitialString);
              if( scanNextLineStdin() ){
                if( isDppPsdIntParameterValueValid() ){
                  tempDPP_int_values[i] = dppIntValueToSet;
                }
                else{
                  printf("\nInvalid value.\n");
                  goto menu5;
                }
              }
              else{
                printf("Read error.\n");
                goto menu5;
              }
            }
            for(i = 0; i < zeroToMaxChIntervalWidth; i++){
              dppParams.pgate[i] = tempDPP_int_values[i];
            }
            modifiedSections[3] = modifiedParameters[3][5] = 1;
            // set 'pgate' as updated
            setParamAsUpdated(5);
            printf("\n\'pgate\' value modified.\n");
            goto menu5;
          }
          else if(menuChoice == 6){
            /*
             *    #####         Modify selft         #####
             */
            printf("\nEnter the new values for parameter \'selft\':\n(ch. number,ch. enable,selft value)\n");
            for(i = 0; i < zeroToMaxChIntervalWidth; i++){
              i ? updateChanSpecificParamInitialString(0) : updateChanSpecificParamInitialString(1);
              printf("%s", chanSpecificParamInitialString);
              if( scanNextLineStdin() ){
                if( isDppPsdIntParameterValueValid() ){
                  tempDPP_int_values[i] = dppIntValueToSet;
                }
                else{
                  printf("\nInvalid value.\n");
                  goto menu5;
                }
              }
              else{
                printf("Read error.\n");
                goto menu5;
              }
            }
            for(i = 0; i < zeroToMaxChIntervalWidth; i++){
              dppParams.selft[i] = tempDPP_int_values[i];
            }
            modifiedSections[3] = modifiedParameters[3][6] = 1;
            // set 'selft' as updated
            setParamAsUpdated(1);
            printf("\n\'selft\' value modified.\n");
            goto menu5;
          }
          else if (menuChoice == 7){
            /*
             *    #####        Modify Trigger.conf     #####
             */
            printf("\nEnter the new values for parameter \'trgc\':\n(ch. number,ch. enable,trgc value)\n");
            for(i = 0; i < zeroToMaxChIntervalWidth; i++){
              i ? updateChanSpecificParamInitialString(0) : updateChanSpecificParamInitialString(1);
              printf("%s", chanSpecificParamInitialString);
              if( scanNextLineStdin() ){
                if( isDppPsdTrgcParameterValueValid() ){
                  tempDPP_trgc_values[i] = dppTrgcValueToSet;
                }
                else{
                  printf("\nInvalid value.\n");
                  goto menu5;
                }
              }
              else{
                printf("Read error.\n");
                goto menu5;
              }
            }
            for(i = 0; i < zeroToMaxChIntervalWidth; i++){
              dppParams.trgc[i] = tempDPP_trgc_values[i];
            }
            modifiedSections[3] = modifiedParameters[3][7] = 1;
            // set 'trgc' as updated
            setParamAsUpdated(8);
            printf("\n\'Trigger configuration\' value modified.\n");
            goto menu5;
          }
          else if (menuChoice == 8){
            /*
             *    #####         Modify tvaw          #####
             */
            printf("\nEnter the new values for parameter \'tvaw\':\n(ch. number,ch. enable,tvaw value)\n");
            for(i = 0; i < zeroToMaxChIntervalWidth; i++){
              i ? updateChanSpecificParamInitialString(0) : updateChanSpecificParamInitialString(1);
              printf("%s", chanSpecificParamInitialString);
              if( scanNextLineStdin() ){
                if( isDppPsdIntParameterValueValid() ){
                  tempDPP_int_values[i] = dppIntValueToSet;
                }
                else{
                  printf("\nInvalid value.\n");
                  goto menu5;
                }
              }
              else{
                printf("Read error.\n");
                goto menu5;
              }
            }
            for(i = 0; i < zeroToMaxChIntervalWidth; i++){
              dppParams.tvaw[i] = tempDPP_int_values[i];
            }
            modifiedSections[3] = modifiedParameters[3][8] = 1;
            // set 'tvaw' as updated
            setParamAsUpdated(6);
            printf("\n\'tvaw\' value modified.\n");
            goto menu5;
          }
          else if (menuChoice == 9){
            /*
             *    #####         Modify csens         #####
             */
            printf("\nEnter the new values for parameter \'csens\':\n(ch. number,ch. enable,csens value)\n");
            for(i = 0; i < zeroToMaxChIntervalWidth; i++){
              i ? updateChanSpecificParamInitialString(0) : updateChanSpecificParamInitialString(1);
              printf("%s", chanSpecificParamInitialString);
              if( scanNextLineStdin() ){
                if( isDppPsdIntParameterValueValid() ){
                  tempDPP_int_values[i] = dppIntValueToSet;
                }
                else{
                  printf("\nInvalid value.\n");
                  goto menu5;
                }
              }
              else{
                printf("Read error.\n");
                goto menu5;
              }
            }
            for(i = 0; i < zeroToMaxChIntervalWidth; i++){
              dppParams.csens[i] = tempDPP_int_values[i];
            }
            modifiedSections[3] = modifiedParameters[3][9] = 1;
            // set 'csens' as updated
            setParamAsUpdated(2);
            printf("\n\'csens\' value modified.\n");
            goto menu5;
          }
          else if(menuChoice == 10){
            /*
             *    #####          Modify purh         #####
             */
            printf("\nEnter the new value for parameter \'purh\':\n");
            if( scanNextLineStdin() ){
              if( DPP_purhParseAndSet() ){
                modifiedSections[3] = modifiedParameters[3][10] = 1;
                printf("\n\'purh\' value modified.\n");
                goto menu5;
              }
              else{
                printf("\nInvalid value.\n");
                goto menu5;
              }
            }
            else{
              printf("Read error.\n");
              goto menu5;
            }
          }
          else if (menuChoice == 11){
            /*
             *    #####          Modify purgap         #####
             */
            printf("\nEnter the new value for parameter \'purgap\':\n");
            if( scanNextLineStdin() ){
              if( DPP_purgapParseAndSet() ){
                modifiedSections[3] = modifiedParameters[3][11] = 1;
                printf("\n\'purgap\' value modified.\n");
                goto menu5;
              }
              else{
                printf("\nInvalid value.\n");
                goto menu5;
              }
            }
            else{
              printf("Read error.\n");
              goto menu5;
            }
          }
          else if (menuChoice == 12){
            /*
             *    #####           Modify blthr         #####
             */
            printf("\nEnter the new value for parameter \'blthr\':\n");
            if( scanNextLineStdin() ){
              if( DPP_blthrParseAndSet() ){
                modifiedSections[3] = modifiedParameters[3][12] = 1;
                printf("\n\'blthr\' value modified.\n");
                goto menu5;
              }
              else{
                printf("\nInvalid value.\n");
                goto menu5;
              }
            }
            else{
              printf("Read error.\n");
              goto menu5;
            }
          }
          else if (menuChoice == 13){
            /*
             *    #####          Modify bltmo          #####
             */
            printf("\nEnter the new value for parameter \'bltmo\':\n");
            if( scanNextLineStdin() ){
              if( DPP_bltmoParseAndSet() ){
                modifiedSections[3] = modifiedParameters[3][13] = 1;
                printf("\n\'bltmo\' value modified.\n");
                goto menu5;
              }
              else{
                printf("\nInvalid value.\n");
                goto menu5;
              }
            }
            else{
              printf("Read error.\n");
              goto menu5;
            }
          }
          else if (menuChoice == 14){
            /*
             *    #####           Modify trgho         #####
             */
            printf("\nEnter the new value for parameter \'trgho\':\n");
            if( scanNextLineStdin() ){
              if( DPP_trghoParseAndSet() ){
                modifiedSections[3] = modifiedParameters[3][14] = 1;
                printf("\n\'trgho\' value modified.\n");
                goto menu5;
              }
              else{
                printf("\nInvalid value.\n");
                goto menu5;
              }
            }
            else{
              printf("Read error.\n");
              goto menu5;
            }
          }
        }
        else
          goto menu5;
      }               // end if() 'modify 'DPP_PSD parameters''
      else if (chRead == '4'){
menu6:
        /*
         *    #####        Modify Acquisit.Time       #####
         */
        puts("");
        printSingleSection(4);
        // if multi-value parameters are not consistent with ch.mask, warn the user
        if (!areParamsUpdated()){
          printWarningAndListNotUpdatedParam();
        }
        printf("Type the number (1) of the parameter you wish to change (press TAB to return to the previous menu):\n");
        stayHere = 1;
        do{
          chRead = getch();
          if ((chRead == '1') || (chRead == '\t'))
            stayHere = 0;
        } while (stayHere);
        if( chRead == '1' ){
          printf("\nEnter the new value for parameter \'Acquisition time\':\n");
          if( scanNextLineStdin() ){
            errno = 0;
            acqTimeTemp = strtoul(myReadLine, NULL, 10);
            if (acqTimeTemp == 0){
              // check if strtoul() returned 0 because of 'invalid' string argument
              if (strcmp(myReadLine, "0") != 0){
                printf("\nInvalid value.\n");
                goto menu6;
              }
            }
            else if (acqTimeTemp == ULONG_MAX){
              /* check if strtoul() returned ULONG_MAX because of out of range string
              argument */
              if (errno == ERANGE){
                printf("\nInvalid value (out of range).\n");
                goto menu6;
              }
            }
            acqTime = acqTimeTemp;
            modifiedSections[4] = modifiedParameters[4][1] = 1;
            printf("\n\'Acquisition time\' value modified.\n");
            goto menu6;
          }
          else{
            printf("Read error.\n");
            goto menu6;
          }
        }
        else if (chRead == '\t'){
          puts("");
          goto menu2;
        }
      }               // end if() 'modify 'acquisition time''
      else if (chRead == '\t'){
        puts("");
        goto menu1;
      }
    }               // end if() modify parameters

    /*
     *    #####    Print parameters that were modified by user    #####
     */
    else if( (chRead == 'p') || (chRead == 'P') ){
      puts("");
      printAllParametersUpdatedView();
      goto menu1A;
    }               // end if() print modified parameters
    
    /*
     *    #####     Update config file with current parameters    #####
     */
    else if(  (chRead == 'f') || (chRead == 'F')  ){
      if( openFileToWrite() ){
		    printf("\nSaving current configuration to file... ");
        saveCurrentParametersToFile();
		    goto menu1A;
      }
    }               // end if() save parameters to file

    /*
     *    #####        Return the 'quit the program' code         #####
     */
    else if(  (chRead == 'q') || (chRead == 'Q')  ){
      returnVal = 0;
    }

    /*
     *    #####      Return the 'start the acquisition' code      #####
     */
    else if(  (chRead == 's') || (chRead == 'S')  ){
      returnVal = 1;
    }

  return returnVal;
}

/* Reads from 'myReadFile' commented and blank lines and puts them to
   'myOutputFile'. Reading ends when a 'valid' line is found ('valid'=
   uncommented and not blank). When the control is returned to the
   caller, the last read line is found in 'myReadLine'.
   
   @return 0 in case of write error or if EOF is read, otherwise returns a
   non-0 integer.
 */
static int copyToFileNextCommentedLines(void){
  int success = 1;

    do{
      memset(myReadLine, '\0', MY_BUFF_SIZE);     // blanks myReadLine

      if(  fgets(myReadLine, MY_BUFF_SIZE - 1, myReadFile) != NULL  ){
        cleanMyReadLine();
        if(  (*myReadLine == '\0') || (*myReadLine == '#')  ){
          if(  fputs(myReadLine, myOutputFile) == EOF  ){
            perror("Error while saving data on disk");
            success = 0;
            break;
          }
          if(  fputs("\n", myOutputFile) == EOF  ){
            perror("Error while saving data on disk");
            success = 0;
            break;
          }
        }
        else
          break;
      }
      else{
        if (feof(myReadFile)){
          success = 0;
          break;
        }
        else{
          success = 0;
          perror("Error while reading \"tdcr.ini\"");
          break;
        }
      }
    } while(  (*myReadLine == '\0') || (*myReadLine == '#')  );

  return success;
}

/* Updates "tdcr.ini" writing the current configuration of the digitizer's
 * parameters.
 */
static void saveCurrentParametersToFile(void){
  int success = 1, i;
  unsigned int scannerBit = 1;
    rewind(myReadFile);

    if( copyToFileNextCommentedLines() )
      ;
    else{
      goto destroy_output_file;
    }

    //        #####  Communication parameters  #####
    // save LinkNum
    if (modifiedParameters[1][1]){
      if (fprintf(myOutputFile, "%d\n", linkNum) < 1){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
    }
    else{
      if (fputs(myReadLine, myOutputFile) == EOF){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
      if (fputs("\n", myOutputFile) == EOF){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
    }

    if (copyToFileNextCommentedLines())
      ;
    else{
      goto destroy_output_file;
    }

    // save LinkType
    if( modifiedParameters[1][2]){
      memset(myReadLine, '\0', MY_BUFF_SIZE);     // blanks myReadLine
      linkTypeValueToStr(myReadLine);
    }
    if(  fputs(myReadLine, myOutputFile) == EOF  ){
      perror("Error while saving data on disk");
      goto destroy_output_file;
    }
    if(  fputs("\n", myOutputFile) == EOF  ){
      perror("Error while saving data on disk");
      goto destroy_output_file;
    }

    if (copyToFileNextCommentedLines())
      ;
    else{
      goto destroy_output_file;
    }

    // save VMEBaseAddress
    if(  modifiedParameters[1][3]  ){
      memset(myReadLine, '\0', MY_BUFF_SIZE);     // blanks myReadLine
      sprintf(myReadLine, "%lu", dttParams.VMEBaseAddress);
    }
    if(  fputs(myReadLine, myOutputFile) == EOF  ){
      perror("Error while saving data on disk");
      goto destroy_output_file;
    }
    if(  fputs("\n", myOutputFile) == EOF  ){
      perror("Error while saving data on disk");
      goto destroy_output_file;
    }

    if (copyToFileNextCommentedLines())
      ;
    else{
      goto destroy_output_file;
    }

    // save CAEN_DGTZ_IOLevel
    if (modifiedParameters[1][4]){
      memset(myReadLine, '\0', MY_BUFF_SIZE);     // blanks myReadLine
      iOLevelValueToStr(myReadLine);
    }
    if (fputs(myReadLine, myOutputFile) == EOF){
      perror("Error while saving data on disk");
      goto destroy_output_file;
    }
    if (fputs("\n", myOutputFile) == EOF){
      perror("Error while saving data on disk");
      goto destroy_output_file;
    }

    if (copyToFileNextCommentedLines())
      ;
    else{
      goto destroy_output_file;
    }

    //          #####  Acquisition parameters  #####
    // save DPP_AcqMode
    if( modifiedParameters[2][1] ){
      memset(myReadLine, '\0', MY_BUFF_SIZE);     // blanks myReadLine
      acqModeValueToStr(myReadLine);
    }
    if(  fputs(myReadLine, myOutputFile) == EOF  ){
      perror("Error while saving data on disk");
      goto destroy_output_file;
    }
    if(  fputs("\n", myOutputFile) == EOF  ){
      perror("Error while saving data on disk");
      goto destroy_output_file;
    }

    if( copyToFileNextCommentedLines() )
      ;
    else{
      goto destroy_output_file;
    }

    // save RecordLength
    if (modifiedParameters[2][2]){
      memset(myReadLine, '\0', MY_BUFF_SIZE);     // blanks myReadLine
      sprintf(myReadLine, "%lu", dttParams.RecordLength);
    }
    if (fputs(myReadLine, myOutputFile) == EOF){
      perror("Error while saving data on disk");
      goto destroy_output_file;
    }
    if (fputs("\n", myOutputFile) == EOF){
      perror("Error while saving data on disk");
      goto destroy_output_file;
    }

    if (copyToFileNextCommentedLines())
      ;
    else{
      goto destroy_output_file;
    }

    // save Channel enable mask
    if( modifiedParameters[2][3] ){
      if(  fprintf(myOutputFile, "%#x\n", dttParams.ChannelMask) < 1  ){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
    }
    else{
      if (fputs(myReadLine, myOutputFile) == EOF){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
      if (fputs("\n", myOutputFile) == EOF){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
    }

    if (copyToFileNextCommentedLines())
      ;
    else{
      goto destroy_output_file;
    }

    // save EventAggr
    if (modifiedParameters[2][4]){
      if(  fprintf(myOutputFile, "%d\n", dttParams.EventAggr) < 1  ){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
    }
    else{
      if (fputs(myReadLine, myOutputFile) == EOF){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
      if (fputs("\n", myOutputFile) == EOF){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
    }

    if (copyToFileNextCommentedLines())
      ;
    else{
      goto destroy_output_file;
    }

    // save PulsePolarity
    if (modifiedParameters[2][5]){
      memset(myReadLine, '\0', MY_BUFF_SIZE);     // blanks myReadLine
      pulsePolarityValueToStr(myReadLine);
    }
    if (fputs(myReadLine, myOutputFile) == EOF){
      perror("Error while saving data on disk");
      goto destroy_output_file;
    }
    if (fputs("\n", myOutputFile) == EOF){
      perror("Error while saving data on disk");
      goto destroy_output_file;
    }

    if (copyToFileNextCommentedLines())
      ;
    else{
      goto destroy_output_file;
    }

    // save thr
    if(  !(modifiedParameters[3][1] || modifiedParameters[2][3])  ){
      // true if both are 'false'
      // 'thr' values on file are updated
      for( i = 0; i < fileZeroToMaxChIntervalWidth; i++ ){
        if(  fprintf(myOutputFile, "%s\n", myReadLine) < 1  ){
          perror("Error while saving data on disk");
          goto destroy_output_file;
        }

        if (copyToFileNextCommentedLines())
          ;
        else{
          goto destroy_output_file;
        }
      }
    }
    else{
      // put on the file the updated 'thr' lines and each comment.
      for( i = 0; i < fileZeroToMaxChIntervalWidth; i++ ){
        if( i <= zeroToMaxChIntervalWidth-1 ){
          fprintf(myOutputFile, "%d,%u,%d\n", i, (dttParams.ChannelMask & scannerBit) >> i, dppParams.thr[i]);
        }
        // do not put on the updated file the commented/blank lines that were
        // situated just after the last line of 'thr' values
        if(  i != fileZeroToMaxChIntervalWidth-1  ){
          if (copyToFileNextCommentedLines())
            ;
          else{
            goto destroy_output_file;
          }
        }
        scannerBit = scannerBit << 1;
      }
      // if the current 'zeroToMaxChIntervalWidth' is higher than
      // 'fileZeroToMaxChIntervalWidth', insert in the file the new 'thr' lines
      if( zeroToMaxChIntervalWidth > i ){
        for( ; i < zeroToMaxChIntervalWidth; i++ ){
          fprintf(myOutputFile, "%d,%u,%d\n", i, (dttParams.ChannelMask & scannerBit) >> i, dppParams.thr[i]);
          scannerBit = scannerBit << 1;
        }
      }
      if (copyToFileNextCommentedLines())
        ;
      else{
        goto destroy_output_file;
      }
    }

    // save nsbl
    scannerBit = 1;
    if(  !(modifiedParameters[3][2] || modifiedParameters[2][3])  ){
      // true if both are 'false'
      // 'nsbl' values on file are updated
      for( i = 0; i < fileZeroToMaxChIntervalWidth; i++ ){
        if( fprintf(myOutputFile, "%s\n", myReadLine) < 1 ){
          perror("Error while saving data on disk");
          goto destroy_output_file;
        }

        if (copyToFileNextCommentedLines())
          ;
        else{
          goto destroy_output_file;
        }
      }
    }
    else{
      // put on the file the updated 'nsbl' lines and each comment.
      for( i = 0; i < fileZeroToMaxChIntervalWidth; i++ ){
        if( i <= zeroToMaxChIntervalWidth - 1 ){
          fprintf(myOutputFile, "%d,%u,%d\n", i, (dttParams.ChannelMask & scannerBit) >> i, dppParams.nsbl[i]);
        }
        // do not put on the updated file the commented/blank lines that were
        // situated just after the last line of 'nsbl' values
        if(i != fileZeroToMaxChIntervalWidth - 1){
          if (copyToFileNextCommentedLines())
            ;
          else{
            goto destroy_output_file;
          }
        }
        scannerBit = scannerBit << 1;
      }
      // if the current 'zeroToMaxChIntervalWidth' is higher than
      // 'fileZeroToMaxChIntervalWidth', insert in the file the new 'nsbl' lines
      if(zeroToMaxChIntervalWidth > i){
        for(; i < zeroToMaxChIntervalWidth; i++){
          fprintf(myOutputFile, "%d,%u,%d\n", i, (dttParams.ChannelMask & scannerBit) >> i, dppParams.nsbl[i]);
          scannerBit = scannerBit << 1;
        }
      }
      if (copyToFileNextCommentedLines())
        ;
      else{
        goto destroy_output_file;
      }
    }

    // save lgate
    scannerBit = 1;
    if (!(modifiedParameters[3][3] || modifiedParameters[2][3])){
      // true if both are 'false'
      // 'lgate' values on file are updated
      for (i = 0; i < fileZeroToMaxChIntervalWidth; i++){
        if (fprintf(myOutputFile, "%s\n", myReadLine) < 1){
          perror("Error while saving data on disk");
          goto destroy_output_file;
        }

        if (copyToFileNextCommentedLines())
          ;
        else{
          goto destroy_output_file;
        }
      }
    }
    else{
      // put on the file the updated 'lgate' lines and each comment.
      for (i = 0; i < fileZeroToMaxChIntervalWidth; i++){
        if (i <= zeroToMaxChIntervalWidth - 1){
          fprintf(myOutputFile, "%d,%u,%d\n", i, (dttParams.ChannelMask & scannerBit) >> i, dppParams.lgate[i]);
        }
        // do not put on the updated file the commented/blank lines that were
        // situated just after the last line of 'lgate' values
        if (i != fileZeroToMaxChIntervalWidth - 1){
          if (copyToFileNextCommentedLines())
            ;
          else{
            goto destroy_output_file;
          }
        }
        scannerBit = scannerBit << 1;
      }
      // if the current 'zeroToMaxChIntervalWidth' is higher than
      // 'fileZeroToMaxChIntervalWidth', insert in the file the new 'lgate' lines
      if (zeroToMaxChIntervalWidth > i){
        for (; i < zeroToMaxChIntervalWidth; i++){
          fprintf(myOutputFile, "%d,%u,%d\n", i, (dttParams.ChannelMask & scannerBit) >> i, dppParams.lgate[i]);
          scannerBit = scannerBit << 1;
        }
      }
      if (copyToFileNextCommentedLines())
        ;
      else{
        goto destroy_output_file;
      }
    }

    // save sgate
    scannerBit = 1;
    if(  !(modifiedParameters[3][4] || modifiedParameters[2][3])  ){
      // true if both are 'false'
      // 'sgate' values on file are updated
      for (i = 0; i < fileZeroToMaxChIntervalWidth; i++){
        if (fprintf(myOutputFile, "%s\n", myReadLine) < 1){
          perror("Error while saving data on disk");
          goto destroy_output_file;
        }

        if (copyToFileNextCommentedLines())
          ;
        else{
          goto destroy_output_file;
        }
      }
    }
    else{
      // put on the file the updated 'sgate' lines and each comment.
      for (i = 0; i < fileZeroToMaxChIntervalWidth; i++){
        if (i <= zeroToMaxChIntervalWidth - 1){
          fprintf(myOutputFile, "%d,%u,%d\n", i, (dttParams.ChannelMask & scannerBit) >> i, dppParams.sgate[i]);
        }
        // do not put on the updated file the commented/blank lines that were
        // situated just after the last line of 'sgate' values
        if (i != fileZeroToMaxChIntervalWidth - 1){
          if (copyToFileNextCommentedLines())
            ;
          else{
            goto destroy_output_file;
          }
        }
        scannerBit = scannerBit << 1;
      }
      // if the current 'zeroToMaxChIntervalWidth' is higher than
      // 'fileZeroToMaxChIntervalWidth', insert in the file the new 'sgate' lines
      if (zeroToMaxChIntervalWidth > i){
        for (; i < zeroToMaxChIntervalWidth; i++){
          fprintf(myOutputFile, "%d,%u,%d\n", i, (dttParams.ChannelMask & scannerBit) >> i, dppParams.sgate[i]);
          scannerBit = scannerBit << 1;
        }
      }
      if (copyToFileNextCommentedLines())
        ;
      else{
        goto destroy_output_file;
      }
    }

    // save pgate
    scannerBit = 1;
    if(  !(modifiedParameters[3][5] || modifiedParameters[2][3])  ){
      // true if both are 'false'
      // 'pgate' values on file are updated
      for (i = 0; i < fileZeroToMaxChIntervalWidth; i++){
        if (fprintf(myOutputFile, "%s\n", myReadLine) < 1){
          perror("Error while saving data on disk");
          goto destroy_output_file;
        }

        if (copyToFileNextCommentedLines())
          ;
        else{
          goto destroy_output_file;
        }
      }
    }
    else{
      // put on the file the updated 'pgate' lines and each comment.
      for (i = 0; i < fileZeroToMaxChIntervalWidth; i++){
        if (i <= zeroToMaxChIntervalWidth - 1){
          fprintf(myOutputFile, "%d,%u,%d\n", i, (dttParams.ChannelMask & scannerBit) >> i, dppParams.pgate[i]);
        }
        // do not put on the updated file the commented/blank lines that were
        // situated just after the last line of 'pgate' values
        if (i != fileZeroToMaxChIntervalWidth - 1){
          if (copyToFileNextCommentedLines())
            ;
          else{
            goto destroy_output_file;
          }
        }
        scannerBit = scannerBit << 1;
      }
      // if the current 'zeroToMaxChIntervalWidth' is higher than
      // 'fileZeroToMaxChIntervalWidth', insert in the file the new 'pgate' lines
      if (zeroToMaxChIntervalWidth > i){
        for (; i < zeroToMaxChIntervalWidth; i++){
          fprintf(myOutputFile, "%d,%u,%d\n", i, (dttParams.ChannelMask & scannerBit) >> i, dppParams.pgate[i]);
          scannerBit = scannerBit << 1;
        }
      }
      if (copyToFileNextCommentedLines())
        ;
      else{
        goto destroy_output_file;
      }
    }

    // save selft
    scannerBit = 1;
    if(  !(modifiedParameters[3][6] || modifiedParameters[2][3])  ){
      // true if both are 'false'
      // 'selft' values on file are updated
      for (i = 0; i < fileZeroToMaxChIntervalWidth; i++){
        if (fprintf(myOutputFile, "%s\n", myReadLine) < 1){
          perror("Error while saving data on disk");
          goto destroy_output_file;
        }

        if (copyToFileNextCommentedLines())
          ;
        else{
          goto destroy_output_file;
        }
      }
    }
    else{
      // put on the file the updated 'selft' lines and each comment.
      for (i = 0; i < fileZeroToMaxChIntervalWidth; i++){
        if (i <= zeroToMaxChIntervalWidth - 1){
          fprintf(myOutputFile, "%d,%u,%d\n", i, (dttParams.ChannelMask & scannerBit) >> i, dppParams.selft[i]);
        }
        // do not put on the updated file the commented/blank lines that were
        // situated just after the last line of 'selft' values
        if (i != fileZeroToMaxChIntervalWidth - 1){
          if (copyToFileNextCommentedLines())
            ;
          else{
            goto destroy_output_file;
          }
        }
        scannerBit = scannerBit << 1;
      }
      // if the current 'zeroToMaxChIntervalWidth' is higher than
      // 'fileZeroToMaxChIntervalWidth', insert in the file the new 'selft' lines
      if (zeroToMaxChIntervalWidth > i){
        for (; i < zeroToMaxChIntervalWidth; i++){
          fprintf(myOutputFile, "%d,%u,%d\n", i, (dttParams.ChannelMask & scannerBit) >> i, dppParams.selft[i]);
          scannerBit = scannerBit << 1;
        }
      }
      if (copyToFileNextCommentedLines())
        ;
      else{
        goto destroy_output_file;
      }
    }

    // save trgc
    scannerBit = 1;
    if (!(modifiedParameters[3][7] || modifiedParameters[2][3])){
      // true if both are 'false'
      // 'trgc' values on file are updated
      for (i = 0; i < fileZeroToMaxChIntervalWidth; i++){
        if (fprintf(myOutputFile, "%s\n", myReadLine) < 1){
          perror("Error while saving data on disk");
          goto destroy_output_file;
        }

        if (copyToFileNextCommentedLines())
          ;
        else{
          goto destroy_output_file;
        }
      }
    }
    else{
      // put on the file the updated 'trgc' lines and each comment.
      for (i = 0; i < fileZeroToMaxChIntervalWidth; i++){
        if (i <= zeroToMaxChIntervalWidth - 1){
          if (dppParams.trgc[i] == CAEN_DGTZ_DPP_TriggerConfig_Threshold)
            fprintf(myOutputFile, "%d,%u,CAEN_DGTZ_DPP_TriggerConfig_Threshold\n", i, (dttParams.ChannelMask & scannerBit) >> i);
          else if (dppParams.trgc[i] == CAEN_DGTZ_DPP_TriggerConfig_Peak)
            fprintf(myOutputFile, "%d,%u,CAEN_DGTZ_DPP_TriggerConfig_Peak\n", i, (dttParams.ChannelMask & scannerBit) >> i);
          else
            fprintf(myOutputFile, "%d,%u,<unrecognized value>\n", i, (dttParams.ChannelMask & scannerBit) >> i);
        }
        // do not put on the updated file the commented/blank lines that were
        // situated just after the last line of 'trgc' values
        if (i != fileZeroToMaxChIntervalWidth - 1){
          if (copyToFileNextCommentedLines())
            ;
          else{
            goto destroy_output_file;
          }
        }
        scannerBit = scannerBit << 1;
      }
      // if the current 'zeroToMaxChIntervalWidth' is higher than
      // 'fileZeroToMaxChIntervalWidth', insert in the file the new 'trgc' lines
      if (zeroToMaxChIntervalWidth > i){
        for (; i < zeroToMaxChIntervalWidth; i++){
          if (dppParams.trgc[i] == CAEN_DGTZ_DPP_TriggerConfig_Threshold)
            fprintf(myOutputFile, "%d,%u,CAEN_DGTZ_DPP_TriggerConfig_Threshold\n", i, (dttParams.ChannelMask & scannerBit) >> i);
          else if (dppParams.trgc[i] == CAEN_DGTZ_DPP_TriggerConfig_Peak)
            fprintf(myOutputFile, "%d,%u,CAEN_DGTZ_DPP_TriggerConfig_Peak\n", i, (dttParams.ChannelMask & scannerBit) >> i);
          else
            fprintf(myOutputFile, "%d,%u,<unrecognized value>\n", i, (dttParams.ChannelMask & scannerBit) >> i);
          scannerBit = scannerBit << 1;
        }
      }
      if (copyToFileNextCommentedLines())
        ;
      else{
        goto destroy_output_file;
      }
    }

    // save tvaw
    scannerBit = 1;
    if (!(modifiedParameters[3][8] || modifiedParameters[2][3])){
      // true if both are 'false'
      // 'tvaw' values on file are updated
      for (i = 0; i < fileZeroToMaxChIntervalWidth; i++){
        if (fprintf(myOutputFile, "%s\n", myReadLine) < 1){
          perror("Error while saving data on disk");
          goto destroy_output_file;
        }

        if (copyToFileNextCommentedLines())
          ;
        else{
          goto destroy_output_file;
        }
      }
    }
    else{
      // put on the file the updated 'tvaw' lines and each comment.
      for (i = 0; i < fileZeroToMaxChIntervalWidth; i++){
        if (i <= zeroToMaxChIntervalWidth - 1){
          fprintf(myOutputFile, "%d,%u,%d\n", i, (dttParams.ChannelMask & scannerBit) >> i, dppParams.tvaw[i]);
        }
        // do not put on the updated file the commented/blank lines that were
        // situated just after the last line of 'tvaw' values
        if (i != fileZeroToMaxChIntervalWidth - 1){
          if (copyToFileNextCommentedLines())
            ;
          else{
            goto destroy_output_file;
          }
        }
        scannerBit = scannerBit << 1;
      }
      // if the current 'zeroToMaxChIntervalWidth' is higher than
      // 'fileZeroToMaxChIntervalWidth', insert in the file the new 'tvaw' lines
      if (zeroToMaxChIntervalWidth > i){
        for (; i < zeroToMaxChIntervalWidth; i++){
          fprintf(myOutputFile, "%d,%u,%d\n", i, (dttParams.ChannelMask & scannerBit) >> i, dppParams.tvaw[i]);
          scannerBit = scannerBit << 1;
        }
      }
      if (copyToFileNextCommentedLines())
        ;
      else{
        goto destroy_output_file;
      }
    }

    // save csens
    scannerBit = 1;
    if (!(modifiedParameters[3][9] || modifiedParameters[2][3])){
      // true if both are 'false'
      // 'csens' values on file are updated
      for (i = 0; i < fileZeroToMaxChIntervalWidth; i++){
        if (fprintf(myOutputFile, "%s\n", myReadLine) < 1){
          perror("Error while saving data on disk");
          goto destroy_output_file;
        }

        if (copyToFileNextCommentedLines())
          ;
        else{
          goto destroy_output_file;
        }
      }
    }
    else{
      // put on the file the updated 'csens' lines and each comment.
      for (i = 0; i < fileZeroToMaxChIntervalWidth; i++){
        if (i <= zeroToMaxChIntervalWidth - 1){
          fprintf(myOutputFile, "%d,%u,%d\n", i, (dttParams.ChannelMask & scannerBit) >> i, dppParams.csens[i]);
        }
        // do not put on the updated file the commented/blank lines that were
        // situated just after the last line of 'csens' values
        if (i != fileZeroToMaxChIntervalWidth - 1){
          if (copyToFileNextCommentedLines())
            ;
          else{
            goto destroy_output_file;
          }
        }
        scannerBit = scannerBit << 1;
      }
      // if the current 'zeroToMaxChIntervalWidth' is higher than
      // 'fileZeroToMaxChIntervalWidth', insert in the file the new 'csens' lines
      if (zeroToMaxChIntervalWidth > i){
        for (; i < zeroToMaxChIntervalWidth; i++){
          fprintf(myOutputFile, "%d,%u,%d\n", i, (dttParams.ChannelMask & scannerBit) >> i, dppParams.csens[i]);
          scannerBit = scannerBit << 1;
        }
      }
      if (copyToFileNextCommentedLines())
        ;
      else{
        goto destroy_output_file;
      }
    }

    // save purh
    if (modifiedParameters[3][10]){
      memset(myReadLine, '\0', MY_BUFF_SIZE);     // blanks myReadLine
      purhValueToStr(myReadLine);
    }
    if (fputs(myReadLine, myOutputFile) == EOF){
      perror("Error while saving data on disk");
      goto destroy_output_file;
    }
    if (fputs("\n", myOutputFile) == EOF){
      perror("Error while saving data on disk");
      goto destroy_output_file;
    }

    if (copyToFileNextCommentedLines())
      ;
    else{
      goto destroy_output_file;
    }

    // save purgap
    if (modifiedParameters[3][11]){
      if (fprintf(myOutputFile, "%d\n", dppParams.purgap) < 1){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
    }
    else{
      if (fputs(myReadLine, myOutputFile) == EOF){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
      if (fputs("\n", myOutputFile) == EOF){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
    }

    if (copyToFileNextCommentedLines())
      ;
    else{
      goto destroy_output_file;
    }

    // save blthr
    if (modifiedParameters[3][12]){
      if (fprintf(myOutputFile, "%d\n", dppParams.blthr) < 1){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
    }
    else{
      if (fputs(myReadLine, myOutputFile) == EOF){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
      if (fputs("\n", myOutputFile) == EOF){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
    }

    if (copyToFileNextCommentedLines())
      ;
    else{
      goto destroy_output_file;
    }

    // save bltmo
    if (modifiedParameters[3][13]){
      if (fprintf(myOutputFile, "%d\n", dppParams.bltmo) < 1){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
    }
    else{
      if (fputs(myReadLine, myOutputFile) == EOF){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
      if (fputs("\n", myOutputFile) == EOF){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
    }

    if (copyToFileNextCommentedLines())
      ;
    else{
      goto destroy_output_file;
    }

    // save trgho
    if (modifiedParameters[3][14]){
      if (fprintf(myOutputFile, "%d\n", dppParams.trgho) < 1){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
    }
    else{
      if (fputs(myReadLine, myOutputFile) == EOF){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
      if (fputs("\n", myOutputFile) == EOF){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
    }

    if (copyToFileNextCommentedLines())
      ;
    else{
      goto destroy_output_file;
    }

    //          #####     Acquisition time     #####
    // save acqTime
    if (modifiedParameters[4][1]){
      if (fprintf(myOutputFile, "%lu\n", acqTime) < 1){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
    }
    else{
      if (fputs(myReadLine, myOutputFile) == EOF){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
      if (fputs("\n", myOutputFile) == EOF){
        perror("Error while saving data on disk");
        goto destroy_output_file;
      }
    }
    
    copyToFileNextCommentedLines();


    // replace old tdcr.ini with the updated file
    if(  fclose(myReadFile) == EOF  ){
      fputs("Error closing \"tdcr.ini\" !", stderr);
    }
    else
      myReadFileClosed = 1;
    if( remove("tdcr.ini") )
      perror("Unable to update tdcr.ini (tried to delete the old file)");
    else{
      if(  fclose(myOutputFile) == EOF  ){
        fputs("Error closing \"updated_tdcr_temp\" (updated tdcr.ini temp file)!", stderr);
      }
      if(  rename("updated_tdcr_temp", "tdcr.ini")  )
        perror("Unable to update tdcr.ini (tried to rename \"updated_tdcr_temp\" (updated tdcr.ini temp file)");
      // open 'tdcr.ini'
      if(  (myReadFile = fopen("tdcr.ini", "r")) == NULL  ){
        perror("An error occurred while trying to open \"tdcr.ini\"");
      }
      else
        myReadFileClosed = 0;
    }

    if( !success ){
destroy_output_file:
      fclose(myOutputFile);
      if( remove("updated_tdcr_temp") ){
        perror("Error while trying to remove temp file");
      }
    }
	  else{
	    fileZeroToMaxChIntervalWidth = zeroToMaxChIntervalWidth;
	    printf("Current configuration saved!\n");
	  }

}

/* Prints to stdout all the parameters stored in 'dttParams', 'dppParams' and
 * the parameters 'linkNum' and 'acqTime'.
 */
static void printAllParameters(void){
  char charOutputBuff[45] = { '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
      '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
      '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
      '\0', '\0', '\0', '\0', '\0' };

    printf("1. Communication Parameters\n--------------------------------------------------\n");
    printf("Link number: %37d\n", linkNum);
    linkTypeValueToStr(charOutputBuff);
    printf("Link type: %39s\n", charOutputBuff);
    printf("VMEBaseAddress: %34lu\n", dttParams.VMEBaseAddress);
    resetCharOutputBuff(charOutputBuff, 45);
    iOLevelValueToStr(charOutputBuff);
    printf("CAEN_DGTZ_IOLevel: %31s\n__________________________________________________\n\n", charOutputBuff);

    printf("2. Acquisition Parameters\n--------------------------------------------------\n");
    resetCharOutputBuff(charOutputBuff, 45);
    acqModeValueToStr(charOutputBuff);
    printf("AcqMode: %41s\n", charOutputBuff);
    printf("RecordLength: %36lu\n", dttParams.RecordLength);
    printf("Channel enable mask: %#29x\n", dttParams.ChannelMask);
    resetCharOutputBuff(charOutputBuff, 45);
    sprintf(charOutputBuff, "%d%s", dttParams.EventAggr," samples");
    printf("EventAggr: %39s\n", charOutputBuff);
    resetCharOutputBuff(charOutputBuff, 45);
    pulsePolarityValueToStr(charOutputBuff);
    printf("PulsePolarity: %35s\n__________________________________________________\n\n", charOutputBuff);

    printf("3. DPP_PSD Parameters\n--------------------------------------------------\n");
    printf("Channel thresholds (thr):\n");
    printChThresholds();
    printf("No. of samples for the baseline averaging (nsbl):\n");
    printChNsbl();
    printf("Long gate (lgate):\n");
    printChLGate();
    printf("Short gate (sgate):\n");
    printChSGate();
    printf("Pre-gate (pgate):\n");
    printChPGate();
    printf("Self Trigger Mode (selft):\n");
    printChSelfTrg();
    printf("Trigger configuration (trgc):\n");
    printChTrgConfig();
    printf("Trigger Validation Acceptance Window (tvaw):\n");
    printChTvaw();
    printf("Charge Sensitivity level (csens):\n");
    printChCsens();
    resetCharOutputBuff(charOutputBuff, 45);
    purhValueToStr(charOutputBuff);
    printf("Pile-Up Rejection mode (purh): %36s\n", charOutputBuff);
    resetCharOutputBuff(charOutputBuff, 45);
    sprintf(charOutputBuff, "%d%s", dppParams.purgap, " LSB");
    printf("Pile-Up Rejection GAP value (purgap): %29s\n", charOutputBuff);
    printf("Baseline Threshold (blthr): %39d\n", dppParams.blthr);
    printf("Baseline Timeout (bltmo): %41d\n", dppParams.bltmo);
    resetCharOutputBuff(charOutputBuff, 45);
    sprintf(charOutputBuff, "%d%s", dppParams.trgho, " samples");
    printf("Trigger Hold Off (trgho): %41s\n___________________________________________________________________\n\n", charOutputBuff);

    printf("4. Acquisition time:\n---------------------------------\n");
    resetCharOutputBuff(charOutputBuff, 45);
    sprintf(charOutputBuff, "%lu%s", acqTime, "ms");
    printf("Acquisition time: %15s\n_________________________________\n\n", charOutputBuff);
}

/* Fills 'buffToClean' with 'buffSize' '\0'.
 *
 * @param buffToClean the start location for putting '\0' characters
 * @param buffSize the number of '\0' char to put
 */
static void resetCharOutputBuff(char* buffToClean, int buffSize){
  register int i = 0;
    for (; i < buffSize; i++){
      *(buffToClean + i) = '\0';
    }
}

/* Prints to stdout all the parameters of section number 'sectionNum' stored
 * in 'dttParams', 'dppParams', 'linkNum' and 'acqTime'.
 *
 * @param sectionNum The number that identifies the section to be displayed
 */
static void printSingleSection(unsigned int sectionNum){
  char charOutputBuff[45] = { '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
      '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
      '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };
    switch (sectionNum){
      case 1:
        // Communication Parameters
        printf("Communication Parameters\n------------------------------------------------------\n");
        printf("1.  Link number: %37d\n", linkNum);
        linkTypeValueToStr(charOutputBuff);
        printf("2.  Link type: %39s\n", charOutputBuff);
        printf("3.  VMEBaseAddress: %34lu\n", dttParams.VMEBaseAddress);
        resetCharOutputBuff(charOutputBuff, 45);
        iOLevelValueToStr(charOutputBuff);
        printf("4.  CAEN_DGTZ_IOLevel: %31s\n______________________________________________________\n\n", charOutputBuff);
        break;
      case 2:
        // Acquisition Parameters
        printf("Acquisition Parameters\n------------------------------------------------------\n");
        acqModeValueToStr(charOutputBuff);
        printf("1.  AcqMode: %41s\n", charOutputBuff);
        printf("2.  RecordLength: %36lu\n", dttParams.RecordLength);
        printf("3.  Channel enable mask: %#29x\n", dttParams.ChannelMask);
        resetCharOutputBuff(charOutputBuff, 45);
        sprintf(charOutputBuff, "%d%s", dttParams.EventAggr, " samples");
        printf("4.  EventAggr: %39s\n", charOutputBuff);
        resetCharOutputBuff(charOutputBuff, 45);
        pulsePolarityValueToStr(charOutputBuff);
        printf("5.  PulsePolarity: %35s\n______________________________________________________\n\n", charOutputBuff);
        break;
      case 3:
        // DPP_PSD Parameters
        printf("DPP_PSD Parameters\n-----------------------------------------------------\n");
        printf("1.  Channel thresholds (thr):\n");
        printChThresholds();
        printf("2.  No. of samples for the baseline averaging (nsbl):\n");
        printChNsbl();
        printf("3.  Long gate (lgate):\n");
        printChLGate();
        printf("4.  Short gate (sgate):\n");
        printChSGate();
        printf("5.  Pre-gate (pgate):\n");
        printChPGate();
        printf("6.  Self Trigger Mode (selft):\n");
        printChSelfTrg();
        printf("7.  Trigger configuration (trgc):\n");
        printChTrgConfig();
        printf("8.  Trigger Validation Acceptance Window (tvaw):\n");
        printChTvaw();
        printf("9.  Charge Sensitivity level (csens):\n");
        printChCsens();
        resetCharOutputBuff(charOutputBuff, 45);
        purhValueToStr(charOutputBuff);
        printf("10. Pile-Up Rejection mode (purh): %36s\n", charOutputBuff);
        resetCharOutputBuff(charOutputBuff, 45);
        sprintf(charOutputBuff, "%d%s", dppParams.purgap, " LSB");
        printf("11. Pile-Up Rejection GAP value (purgap): %29s\n", charOutputBuff);
        printf("12. Baseline Threshold (blthr): %39d\n", dppParams.blthr);
        printf("13. Baseline Timeout (bltmo): %41d\n", dppParams.bltmo);
        resetCharOutputBuff(charOutputBuff, 45);
        sprintf(charOutputBuff, "%d%s", dppParams.trgho, " samples");
        printf("14. Trigger Hold Off (trgho): %41s\n_______________________________________________________________________\n\n", charOutputBuff);
        break;
      case 4:
        // Acquisition time (milliseconds)
        printf("Acquisition time:\n-------------------------------------\n");
        resetCharOutputBuff(charOutputBuff, 45);
        sprintf(charOutputBuff, "%lu%s", acqTime, "ms");
        printf("1.  Acquisition time: %15s\n_____________________________________\n\n", charOutputBuff);
        break;
    }
}

/* Prints to stdout all the parameters of section number 'sectionNum' stored
 * in 'dttParams', 'dppParams', 'linkNum' and 'acqTime' that were modified by
 * the user since the last call of resetModifiedParametersArr().
 *
 * @param sectionNum The number that identifies the section to be displayed
 */
static void printSingleSectionUpdatedView(unsigned int sectionNum){
  char charOutputBuff[45] = { '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
        '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };
    switch (sectionNum){
      case 1:
        // Communication Parameters
        printf("Communication Parameters\n------------------------------------------------------\n");
        if( modifiedParameters[1][1] ){
          printf("1.  Link number: %37d\n", linkNum);
        }
        if (modifiedParameters[1][2]){
          linkTypeValueToStr(charOutputBuff);
          printf("2.  Link type: %39s\n", charOutputBuff);
        }
        if( modifiedParameters[1][3] ){
          printf("3.  VMEBaseAddress: %34lu\n", dttParams.VMEBaseAddress);
        }
        if( modifiedParameters[1][4] ){
          resetCharOutputBuff(charOutputBuff, 45);
          iOLevelValueToStr(charOutputBuff);
          printf("4.  CAEN_DGTZ_IOLevel: %31s\n", charOutputBuff);
        }
        printf("______________________________________________________\n\n");
        break;
      case 2:
        // Acquisition Parameters
        printf("Acquisition Parameters\n------------------------------------------------------\n");
        if (modifiedParameters[2][1]){
          acqModeValueToStr(charOutputBuff);
          printf("1.  AcqMode: %41s\n", charOutputBuff);
        }
        if (modifiedParameters[2][2]){
          printf("2.  RecordLength: %36lu\n", dttParams.RecordLength);
        }
        if (modifiedParameters[2][3]){
          printf("3.  Channel enable mask: %#29x\n", dttParams.ChannelMask);
        }
        if (modifiedParameters[2][4]){
          resetCharOutputBuff(charOutputBuff, 45);
          sprintf(charOutputBuff, "%d%s", dttParams.EventAggr, " samples");
          printf("4.  EventAggr: %39s\n", charOutputBuff);
        }
        if (modifiedParameters[2][5]){
          resetCharOutputBuff(charOutputBuff, 45);
          pulsePolarityValueToStr(charOutputBuff);
          printf("5.  PulsePolarity: %35s\n", charOutputBuff);
        }
        printf("______________________________________________________\n\n");
        break;
      case 3:
        // DPP_PSD Parameters
        printf("DPP_PSD Parameters\n------------------------------------------------------\n");
        if (modifiedParameters[3][1]){
          printf("1.  Channel thresholds (thr):\n");
          printChThresholds();
        }
        if (modifiedParameters[3][2]){
          printf("2.  No. of samples for the baseline averaging (nsbl):\n");
          printChNsbl();
        }
        if (modifiedParameters[3][3]){
          printf("3.  Long gate (lgate):\n");
          printChLGate();
        }
        if (modifiedParameters[3][4]){
          printf("4.  Short gate (sgate):\n");
          printChSGate();
        }
        if (modifiedParameters[3][5]){
          printf("5.  Pre-gate (pgate):\n");
          printChPGate();
        }
        if (modifiedParameters[3][6]){
          printf("6.  Self Trigger Mode (selft):\n");
          printChSelfTrg();
        }
        if (modifiedParameters[3][7]){
          printf("7.  Trigger configuration (trgc):\n");
          printChTrgConfig();
        }
        if (modifiedParameters[3][8]){
          printf("8.  Trigger Validation Acceptance Window (tvaw):\n");
          printChTvaw();
        }
        if (modifiedParameters[3][9]){
          printf("9.  Charge Sensitivity level (csens):\n");
          printChCsens();
        }
        if (modifiedParameters[3][10]){
          resetCharOutputBuff(charOutputBuff, 45);
          purhValueToStr(charOutputBuff);
          printf("10. Pile-Up Rejection mode (purh): %36s\n", charOutputBuff);
        }
        if (modifiedParameters[3][11]){
          resetCharOutputBuff(charOutputBuff, 45);
          sprintf(charOutputBuff, "%d%s", dppParams.purgap, " LSB");
          printf("11. Pile-Up Rejection GAP value (purgap): %29s\n", charOutputBuff);
        }
        if (modifiedParameters[3][12]){
          printf("12. Baseline Threshold (blthr): %39d\n", dppParams.blthr);
        }
        if (modifiedParameters[3][13]){
          printf("13. Baseline Timeout (bltmo): %41d\n", dppParams.bltmo);
        }
        if (modifiedParameters[3][14]){
          resetCharOutputBuff(charOutputBuff, 45);
          sprintf(charOutputBuff, "%d%s", dppParams.trgho, " samples");
          printf("14. Trigger Hold Off (trgho): %41s\n", charOutputBuff);
        }
        printf("_______________________________________________________________________\n\n");
        break;
      case 4:
        // Acquisition time (milliseconds)
        printf("Acquisition time:\n-------------------------------------\n");
        if (modifiedParameters[4][1]){
          resetCharOutputBuff(charOutputBuff, 45);
          sprintf(charOutputBuff, "%lu%s", acqTime, "ms");
          printf("1.  Acquisition time: %15s\n", charOutputBuff);
        }
        printf("_____________________________________\n\n");
        break;
    }
}

/* Prints to stdout all the parameters stored in 'dttParams', 'dppParams',
 * 'linkNum' and 'acqTime' that were modified by the user since the last call
 * of resetModifiedParametersArr().
 */
static void printAllParametersUpdatedView(void){
  if(modifiedSections[1]){
    // Communication Parameters
    printSingleSectionUpdatedView(1);
  }
  if(modifiedSections[2]){
    // Acquisition Parameters
    printSingleSectionUpdatedView(2);
  }
  if(modifiedSections[3]){
    // DPP_PSD Parameters
    printSingleSectionUpdatedView(3);
  }
  if (modifiedSections[4]){
    // Acquisition time (milliseconds)
    printSingleSectionUpdatedView(4);
  }
}

/* Prints to stdout a warning message to inform the user that some multi-value
 * parameters must be interactively updated (to allow saving to file the current
 * parameters or starting the acquisition).
 */
static void printWarningAndListNotUpdatedParam(void){
  printf("THE FOLLOWING PARAMETERS MUST BE UPDATED BEFORE SAVING / START ACQUISITION: ");
  printParamsToUpdate();
  printf(".\n\n");
}

/* Copies on 'dest' the string representation of the 'purh' member of
 * the struct 'dppParams'. If the value of 'purh' is not equal to
 * CAEN_DGTZ_DPP_PSD_PUR_DetectOnly or CAEN_DGTZ_DPP_PSD_PUR_Enabled, the
 * function will copy the string "<unrecognized value>".
 */
static void purhValueToStr(char* dest){
    if (  dppParams.purh == CAEN_DGTZ_DPP_PSD_PUR_DetectOnly  )
      strcpy(dest,"CAEN_DGTZ_DPP_PSD_PUR_DetectOnly");
    else if (  dppParams.purh == CAEN_DGTZ_DPP_PSD_PUR_Enabled  )
      strcpy(dest, "CAEN_DGTZ_DPP_PSD_PUR_Enabled");
    else
      strcpy(dest, "<unrecognized value>");
}

/* Prints to stdout for the first 'zeroToMaxChIntervalWidth' channels the
 * channel number, '(enabled)'/'(not enabled)' according to the enable
 * mask and then the value of dppParams.csens for that channel.
 */
static void printChCsens(void){
  register int i = 0;
  register unsigned scannerBit = 1;

    for (; i < zeroToMaxChIntervalWidth; i++){
      if (  (dttParams.ChannelMask & scannerBit) >> i  ){
        printf("ch%d (enabled): %d\n", i, dppParams.csens[i]);
      }
      else{
        printf("ch%d (not enabled): %d\n", i, dppParams.csens[i]);
      }
      scannerBit = scannerBit << 1;
    }
    puts("");
}

/* Prints to stdout for the first 'zeroToMaxChIntervalWidth' channels the
 * channel number, '(enabled)'/'(not enabled)' according to the enable
 * mask and then the value of dppParams.tvaw for that channel.
 */
static void printChTvaw(void){
  register int i = 0;
  register unsigned scannerBit = 1;

    for (; i < zeroToMaxChIntervalWidth; i++){
      if ((dttParams.ChannelMask & scannerBit) >> i){
        printf("ch%d (enabled): %d samples\n", i, dppParams.tvaw[i]);
      }
      else{
        printf("ch%d (not enabled): %d samples\n", i, dppParams.tvaw[i]);
      }
      scannerBit = scannerBit << 1;
    }
    puts("");
}

/* Prints to stdout for the first 'zeroToMaxChIntervalWidth' channels the
 * channel number, '(enabled)'/'(not enabled)' according to the enable
 * mask and then the value of dppParams.trgc for that channel. If the read
 * value is not recognized as valid, "<unrecognized value>" will be printed.
 */
static void printChTrgConfig(void){
  register int i = 0;
  register unsigned scannerBit = 1;

    for (; i < zeroToMaxChIntervalWidth; i++){
      if ((dttParams.ChannelMask & scannerBit) >> i){
        if (  dppParams.trgc[i] == CAEN_DGTZ_DPP_TriggerConfig_Threshold  )
          printf("ch%d (enabled): CAEN_DGTZ_DPP_TriggerConfig_Threshold\n", i);
        else if (  dppParams.trgc[i] == CAEN_DGTZ_DPP_TriggerConfig_Peak  )
          printf("ch%d (enabled): CAEN_DGTZ_DPP_TriggerConfig_Peak\n", i);
        else{
          printf("<unrecognized value>");
        }
      }
      else{
        if (  dppParams.trgc[i] == CAEN_DGTZ_DPP_TriggerConfig_Threshold  )
          printf("ch%d (not enabled): CAEN_DGTZ_DPP_TriggerConfig_Threshold\n", i);
        else if (  dppParams.trgc[i] == CAEN_DGTZ_DPP_TriggerConfig_Peak  )
          printf("ch%d (not enabled): CAEN_DGTZ_DPP_TriggerConfig_Peak\n", i);
        else{
          printf("<unrecognized value>");
        }
      }
      scannerBit = scannerBit << 1;
    }
    puts("");
}

/* Prints to stdout for the first 'zeroToMaxChIntervalWidth' channels the
 * channel number, '(enabled)'/'(not enabled)' according to the enable
 * mask and then the value of dppParams.selft for that channel.
 */
static void printChSelfTrg(void){
  register int i = 0;
  register unsigned scannerBit = 1;

    for (; i < zeroToMaxChIntervalWidth; i++){
      if ((dttParams.ChannelMask & scannerBit) >> i){
        printf("ch%d (enabled): %d\n", i, dppParams.selft[i]);
      }
      else{
        printf("ch%d (not enabled): %d\n", i, dppParams.selft[i]);
      }
      scannerBit = scannerBit << 1;
    }
    puts("");
}

/* Prints to stdout for the first 'zeroToMaxChIntervalWidth' channels the
 * channel number, '(enabled)'/'(not enabled)' according to the enable
 * mask and then the value of dppParams.pgate for that channel.
 */
static void printChPGate(void){
  register int i = 0;
  register unsigned scannerBit = 1;

    for (; i < zeroToMaxChIntervalWidth; i++){
      if (  (dttParams.ChannelMask & scannerBit) >> i  ){
        printf("ch%d (enabled): %d (*4ns)\n", i, dppParams.pgate[i]);
      }
      else{
        printf("ch%d (not enabled): %d (*4ns)\n", i, dppParams.pgate[i]);
      }
      scannerBit = scannerBit << 1;
    }
    puts("");
}

/* Prints to stdout for the first 'zeroToMaxChIntervalWidth' channels the
 * channel number, '(enabled)'/'(not enabled)' according to the enable
 * mask and then the value of dppParams.sgate for that channel.
 */
static void printChSGate(void){
  register int i = 0;
  register unsigned scannerBit = 1;

    for (; i < zeroToMaxChIntervalWidth; i++){
      if ((dttParams.ChannelMask & scannerBit) >> i){
        printf("ch%d (enabled): %d (*4ns)\n", i, dppParams.sgate[i]);
      }
      else{
        printf("ch%d (not enabled): %d (*4ns)\n", i, dppParams.sgate[i]);
      }
      scannerBit = scannerBit << 1;
    }
    puts("");
}

/* Prints to stdout for the first 'zeroToMaxChIntervalWidth' channels the
 * channel number, '(enabled)'/'(not enabled)' according to the enable
 * mask and then the value of dppParams.lgate for that channel.
 */
static void printChLGate(void){
  register int i = 0;
  register unsigned scannerBit = 1;

    for (; i < zeroToMaxChIntervalWidth; i++){
      if ((dttParams.ChannelMask & scannerBit) >> i){
        printf("ch%d (enabled): %d (*4ns)\n", i, dppParams.lgate[i]);
      }
      else{
        printf("ch%d (not enabled): %d (*4ns)\n", i, dppParams.lgate[i]);
      }
      scannerBit = scannerBit << 1;
    }
    puts("");
}

/* Prints to stdout for the first 'zeroToMaxChIntervalWidth' channels the
 * channel number, '(enabled)'/'(not enabled)' according to the enable
 * mask and then the value of dppParams.nsbl for that channel.
 */
static void printChNsbl(void){
  register int i = 0;
  register unsigned scannerBit = 1;

    for (; i < zeroToMaxChIntervalWidth; i++){
      if (  (dttParams.ChannelMask & scannerBit) >> i  ){
        printf("ch%d (enabled): %d samples\n", i, dppParams.nsbl[i]);
      }
      else{
        printf("ch%d (not enabled): %d samples\n", i, dppParams.nsbl[i]);
      }
      scannerBit = scannerBit << 1;
    }
    puts("");
}

/* Prints to stdout for the first 'zeroToMaxChIntervalWidth' channels the
 * channel number, '(enabled)'/'(not enabled)' according to the enable
 * mask and then the value of dppParams.thr for that channel.
 */
static void printChThresholds(void){
  register int i = 0;
  register unsigned scannerBit = 1;

    for(; i < zeroToMaxChIntervalWidth; i++){
      if (  (dttParams.ChannelMask & scannerBit) >> i  ){
        printf("ch%d (enabled): %d LSB\n", i, dppParams.thr[i]);
      }
      else{
        printf("ch%d (not enabled): %d LSB\n", i, dppParams.thr[i]);
      }
      scannerBit = scannerBit << 1;
    }
    puts("");
}

/* Copies on 'dest' the string representation of the 'PulsePolarity' member of
 * the struct 'dttParams'. If the value of 'PulsePolarity' is not equal to
 * CAEN_DGTZ_PulsePolarityPositive or CAEN_DGTZ_PulsePolarityNegative, the
 * function will copy the string "<unrecognized value>".
 */
static void pulsePolarityValueToStr(char* dest){
  if (dttParams.PulsePolarity == CAEN_DGTZ_PulsePolarityPositive)
    strcpy(dest,"CAEN_DGTZ_PulsePolarityPositive");
  else if (dttParams.PulsePolarity == CAEN_DGTZ_PulsePolarityNegative)
    strcpy(dest, "CAEN_DGTZ_PulsePolarityNegative");
  else
    strcpy(dest, "<unrecognized value>");
}

/* Copies on 'dest' the string representation of the 'AcqMode' member of the
 * struct 'dttParams'. If the value of 'AcqMode' is not equal to
 * CAEN_DGTZ_DPP_ACQ_MODE_Oscilloscope or CAEN_DGTZ_DPP_ACQ_MODE_List or
 * CAEN_DGTZ_DPP_ACQ_MODE_Mixed, the function will copy the string
 * "<unrecognized value>".
 */
static void acqModeValueToStr(char* dest){
  if (dttParams.AcqMode == CAEN_DGTZ_DPP_ACQ_MODE_Oscilloscope)
    strcpy(dest, "CAEN_DGTZ_DPP_ACQ_MODE_Oscilloscope");
  else if (dttParams.AcqMode == CAEN_DGTZ_DPP_ACQ_MODE_List)
    strcpy(dest, "CAEN_DGTZ_DPP_ACQ_MODE_List");
  else if (dttParams.AcqMode == CAEN_DGTZ_DPP_ACQ_MODE_Mixed)
    strcpy(dest, "CAEN_DGTZ_DPP_ACQ_MODE_Mixed");
  else
    strcpy(dest, "<unrecognized value>");
}

/* Copies on 'dest' the string representation of the 'LinkType' member of the
 * struct 'dttParams'. If the value of 'LinkType' is not equal to CAEN_DGTZ_USB
 * or CAEN_DGTZ_OpticalLink the function will copy the string
 * "<unrecognized value>".
 */
static void linkTypeValueToStr(char* dest){
    if (  dttParams.LinkType == CAEN_DGTZ_USB  )
      strcpy(dest,"CAEN_DGTZ_USB");
    else if (  dttParams.LinkType == CAEN_DGTZ_OpticalLink  )
      strcpy(dest,"CAEN_DGTZ_OpticalLink");
    else
      strcpy(dest,"<unrecognized value>");
}

/* Copies on 'dest' the string representation of the 'IOLevel' member of the
 * struct 'dttParams'. If the value of 'IOLevel' is not equal to
 * CAEN_DGTZ_IOLevel_TTL or CAEN_DGTZ_IOLevel_NIM then the function will copy
 * the string "<unrecognized value>".
 */
static void iOLevelValueToStr(char* dest){
    if (dttParams.IOlev == CAEN_DGTZ_IOLevel_TTL)
      strcpy(dest,"CAEN_DGTZ_IOLevel_TTL");
    else if (dttParams.IOlev == CAEN_DGTZ_IOLevel_NIM)
      strcpy(dest,"CAEN_DGTZ_IOLevel_NIM");
    else
      strcpy(dest,"<unrecognized value>");
}

/* Check if 'myReadLine' could be a valid string representation of a 'trgc'
 * value.  If the check is passed, 'myReadLine' is converted to
 * CAEN_DGTZ_DPP_TriggerConfig_t and stored in 'dppTrgcValueToSet'.
 * Returns 0 if 'myReadLine' is not a valid string representation of a 'trgc'
 * parameter value, otherwise returns a non-zero integer.
 *
 * @return 0 if 'myReadLine' is not a valid string representation of a
 * 'trgc' value, otherwise returns a non-zero integer
 */
static int isDppPsdTrgcParameterValueValid(void){
  int success = 0;

    if( myReadLine != NULL ){
      if( strcmp(myReadLine, "CAEN_DGTZ_DPP_TriggerConfig_Threshold") == 0 ){
        dppTrgcValueToSet = CAEN_DGTZ_DPP_TriggerConfig_Threshold;
        success = 1;
      }
      else if( strcmp(myReadLine, "CAEN_DGTZ_DPP_TriggerConfig_Peak") == 0 ){
        dppTrgcValueToSet = CAEN_DGTZ_DPP_TriggerConfig_Peak;
        success = 1;
      }
    }

  return success;
}

/* Check if 'myReadLine' could be a valid string representation of a
 * CAEN_DGTZ_DPP_PSD_Params_t.thr / nsbl / lgate / sgate / pgate / selft / tvaw
 * value.
 * If the check is passed,
 * 'myReadLine' is converted to int and stored in 'dppIntValueToSet'.
 * Returns 0 if 'myReadLine' is not a valid string representation of an
 * aforementioned DPP-PSD parameter, otherwise returns a non-zero integer.
 *
 * @return 0 if 'myReadLine' is not a valid string representation of a
 * CAEN_DGTZ_DPP_PSD_Params_t.thr / nsbl / lgate / sgate / pgate / selft / tvaw
 * value, otherwise returns a non-zero integer
 */
static int isDppPsdIntParameterValueValid(void){
  int success = 0;

    if (myReadLine != NULL){
      dppIntValueToSet = atoi(myReadLine);
      if (dppIntValueToSet == 0){
        // check if atoi() returned 0 because of 'invalid' argument value
        if (*myReadLine == '0' && *(myReadLine + 1) == '\0'){
          success = 1;
        }
        else
          ;
      }
      else{
        success = 1;
      }
    }

  return success;
}

/* Parses the string 'myReadLine' n times where n equals
 * 'zeroToMaxChIntervalWidth' (before parsing 'myReadLine', the string is filled
 * by calling the function 'scanNextLine()'). Then, possibly sets
 * 'dppParams.csens'. Each line is expected to follow this schema:
 * <channel number>,<'1' if enabled, '0' if disabled>,<value>
 * If one or more lines do not adhere to the schema then no value is set.
 * If an error is found the function prints to stderr its line number.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DPP_csensParseAndSet(void){
  unsigned int readEnable, scannerBit = 1;
  int readValue, readChNo;
  register int i = 0;
  int success = 0, error = 0;
  char* readStrPtr = NULL;

    for (; i< zeroToMaxChIntervalWidth; i++){
      scanNextLine();
      readStrPtr = myReadLine;
      if (readStrPtr != NULL){
        readStrPtr = strtok(readStrPtr, ",");
        if (readStrPtr != NULL){
          /*
           * check CHANNEL NUMBER token (if error found then break)
           */
          readChNo = atoi(readStrPtr);
          if (i == 0){
            if (readChNo == 0){
              // check if atoi() returned 0 because of 'invalid' token
              if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0')
                ;       // OK
              else{
                fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                error = 1;
                break;
              }
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else{
            if (readChNo == i)
              ;       // OK
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }

        readStrPtr = strtok(NULL, ",");
        /*
         * check ENABLE BIT (if error found then break)
         */
        if (readStrPtr != NULL){
          readEnable = atoi(readStrPtr);
          if (readEnable == 0){
            // check if atoi() returned 0 because of 'invalid' token
            if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0'){
              // check if channel is really not enabled
              if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
                fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                error = 1;
                break;
              }
              else
                ;   // OK
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else if (readEnable == 1){
            if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
            else
              ;     // OK
          }
          else{
            fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
            error = 1;
            break;
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }

        readStrPtr = strtok(NULL, ",");
        /*
         * parse VALUE (if error found then break)
         */
        if (readStrPtr != NULL){
          readValue = atoi(readStrPtr);
          if (readValue == 0){
            // check if atoi() returned 0 because of 'invalid' token
            if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0'){
              dppParams.csens[i] = readValue;
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else{
            dppParams.csens[i] = readValue;
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }
      }
      else{
        fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
        error = 1;
        break;
      }
      scannerBit = scannerBit << 1;
    }       // end of 'for'

    if ((i == zeroToMaxChIntervalWidth) && (!error))
      success = 1;

  return success;
}

/* Parses the string 'myReadLine' n times where n equals
 * 'zeroToMaxChIntervalWidth' (before parsing 'myReadLine', the string is filled
 * by calling the function 'scanNextLine()'). Then, possibly sets
 * 'dppParams.tvaw'. Each line is expected to follow this schema:
 * <channel number>,<'1' if enabled, '0' if disabled>,<value>
 * If one or more lines do not adhere to the schema then no value is set.
 * If an error is found the function prints to stderr its line number.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DPP_tvawParseAndSet(void){
  unsigned int readEnable, scannerBit = 1;
  int readValue, readChNo;
  register int i = 0;
  int success = 0, error = 0;
  char* readStrPtr = NULL;

    for (; i< zeroToMaxChIntervalWidth; i++){
      scanNextLine();
      readStrPtr = myReadLine;
      if (readStrPtr != NULL){
        readStrPtr = strtok(readStrPtr, ",");
        if (readStrPtr != NULL){
          /*
           * check CHANNEL NUMBER token (if error found then break)
           */
          readChNo = atoi(readStrPtr);
          if (i == 0){
            if (readChNo == 0){
              // check if atoi() returned 0 because of 'invalid' token
              if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0')
                ;       // OK
              else{
                fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                error = 1;
                break;
              }
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else{
            if (readChNo == i)
              ;       // OK
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }

        readStrPtr = strtok(NULL, ",");
        /*
         * check ENABLE BIT (if error found then break)
         */
        if (readStrPtr != NULL){
          readEnable = atoi(readStrPtr);
          if (readEnable == 0){
            // check if atoi() returned 0 because of 'invalid' token
            if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0'){
              // check if channel is really not enabled
              if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
                fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                error = 1;
                break;
              }
              else
                ;   // OK
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else if (readEnable == 1){
            if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
            else
              ;     // OK
          }
          else{
            fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
            error = 1;
            break;
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }

        readStrPtr = strtok(NULL, ",");
        /*
         * parse VALUE (if error found then break)
         */
        if (readStrPtr != NULL){
          readValue = atoi(readStrPtr);
          if (readValue == 0){
            // check if atoi() returned 0 because of 'invalid' token
            if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0'){
              dppParams.tvaw[i] = readValue;
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else{
            dppParams.tvaw[i] = readValue;
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }
      }
      else{
        fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
        error = 1;
        break;
      }
      scannerBit = scannerBit << 1;
    }       // end of 'for'

    if ((i == zeroToMaxChIntervalWidth) && (!error))
      success = 1;

  return success;
}

/* Parses the string 'myReadLine' n times where n equals
 * 'zeroToMaxChIntervalWidth' (before parsing 'myReadLine', the string is filled
 * by calling the function 'scanNextLine()'). Then, possibly sets
 * 'dppParams.trgc'. Each line is expected to follow this schema:
 * <channel number>,<'1' if enabled, '0' if disabled>,<value>
 * If one or more lines do not adhere to the schema then no value is set.
 * If an error is found the function prints to stderr its line number.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DPP_trgcParseAndSet(void){
  unsigned int readEnable, scannerBit = 1;
  register int readChNo;
  register int i = 0;
  int success = 0, error = 0;
  char* readStrPtr = NULL;

  for (; i< zeroToMaxChIntervalWidth; i++){
    scanNextLine();
    readStrPtr = myReadLine;
    if (readStrPtr != NULL){
      readStrPtr = strtok(readStrPtr, ",");
      if (readStrPtr != NULL){
        /*
         * check CHANNEL NUMBER token (if error found then break)
         */
        readChNo = atoi(readStrPtr);
        if (i == 0){
          if (readChNo == 0){
            // check if atoi() returned 0 because of 'invalid' token
            if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0')
              ;       // OK
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else{
            fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
            error = 1;
            break;
          }
        }
        else{
          if (readChNo == i)
            ;       // OK
          else{
            fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
            error = 1;
            break;
          }
        }
      }
      else{
        fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
        error = 1;
        break;
      }

      readStrPtr = strtok(NULL, ",");
      /*
       * check ENABLE BIT (if error found then break)
       */
      if (readStrPtr != NULL){
        readEnable = atoi(readStrPtr);
        if (readEnable == 0){
          // check if atoi() returned 0 because of 'invalid' token
          if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0'){
            // check if channel is really not enabled
            if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
            else
              ;   // OK
          }
          else{
            fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
            error = 1;
            break;
          }
        }
        else if (readEnable == 1){
          if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
            fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
            error = 1;
            break;
          }
          else
            ;     // OK
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }
      }
      else{
        fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
        error = 1;
        break;
      }

      readStrPtr = strtok(NULL, ",");
      /*
       * parse VALUE (if error found then break)
       */
      if (readStrPtr != NULL){
        if (  strcmp(readStrPtr, "CAEN_DGTZ_DPP_TriggerConfig_Threshold") == 0  ){
          dppParams.trgc[i] = CAEN_DGTZ_DPP_TriggerConfig_Threshold;
        }
        else if (  strcmp(readStrPtr, "CAEN_DGTZ_DPP_TriggerConfig_Peak") == 0  ){
          dppParams.trgc[i] = CAEN_DGTZ_DPP_TriggerConfig_Peak;
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }
      }
      else{
        fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
        error = 1;
        break;
      }
    }
    else{
      fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
      error = 1;
      break;
    }
    scannerBit = scannerBit << 1;
  }       // end of 'for'

  if (  (i == zeroToMaxChIntervalWidth) && (!error)  )
    success = 1;

  return success;
}

/* Parses the string 'myReadLine' n times where n equals
 * 'zeroToMaxChIntervalWidth' (before parsing 'myReadLine', the string is filled
 * by calling the function 'scanNextLine()'). Then, possibly sets
 * 'dppParams.selft'. Each line is expected to follow this schema:
 * <channel number>,<'1' if enabled, '0' if disabled>,<value>
 * If one or more lines do not adhere to the schema then no value is set.
 * If an error is found the function prints to stderr its line number.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DPP_selftParseAndSet(void){
  unsigned int readEnable, scannerBit = 1;
  int readValue, readChNo;
  register int i = 0;
  int success = 0, error = 0;
  char* readStrPtr = NULL;

    for (; i< zeroToMaxChIntervalWidth; i++){
      scanNextLine();
      readStrPtr = myReadLine;
      if (readStrPtr != NULL){
        readStrPtr = strtok(readStrPtr, ",");
        if (readStrPtr != NULL){
          /*
           * check CHANNEL NUMBER token (if error found then break)
           */
          readChNo = atoi(readStrPtr);
          if (i == 0){
            if (readChNo == 0){
              // check if atoi() returned 0 because of 'invalid' token
              if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0')
                ;       // OK
              else{
                fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                error = 1;
                break;
              }
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else{
            if (readChNo == i)
              ;       // OK
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }

        readStrPtr = strtok(NULL, ",");
        /*
         * check ENABLE BIT (if error found then break)
         */
        if (readStrPtr != NULL){
          readEnable = atoi(readStrPtr);
          if (readEnable == 0){
            // check if atoi() returned 0 because of 'invalid' token
            if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0'){
              // check if channel is really not enabled
              if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
                fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                error = 1;
                break;
              }
              else
                ;   // OK
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else if (readEnable == 1){
            if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
            else
              ;     // OK
          }
          else{
            fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
            error = 1;
            break;
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }

        readStrPtr = strtok(NULL, ",");
        /*
         * parse VALUE (if error found then break)
         */
        if (readStrPtr != NULL){
          readValue = atoi(readStrPtr);
          if (readValue == 0){
            // check if atoi() returned 0 because of 'invalid' token
            if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0'){
              dppParams.selft[i] = readValue;
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else{
            dppParams.selft[i] = readValue;
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }
      }
      else{
        fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
        error = 1;
        break;
      }
      scannerBit = scannerBit << 1;
    }       // end of 'for'

    if ((i == zeroToMaxChIntervalWidth) && (!error))
      success = 1;

  return success;
}

/* Parses the string 'myReadLine' n times where n equals
 * 'zeroToMaxChIntervalWidth' (before parsing 'myReadLine', the string is filled
 * by calling the function 'scanNextLine()'). Then, possibly sets
 * 'dppParams.pgate'. Each line is expected to follow this schema:
 * <channel number>,<'1' if enabled, '0' if disabled>,<value>
 * If one or more lines do not adhere to the schema then no value is set.
 * If an error is found the function prints to stderr its line number.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DPP_pgateParseAndSet(void){
  unsigned int readEnable, scannerBit = 1;
  int readValue, readChNo;
  register int i = 0;
  int success = 0, error = 0;
  char* readStrPtr = NULL;

    for (; i< zeroToMaxChIntervalWidth; i++){
      scanNextLine();
      readStrPtr = myReadLine;
      if (readStrPtr != NULL){
        readStrPtr = strtok(readStrPtr, ",");
        if (readStrPtr != NULL){
          /*
           * check CHANNEL NUMBER token (if error found then break)
           */
          readChNo = atoi(readStrPtr);
          if (i == 0){
            if (readChNo == 0){
              // check if atoi() returned 0 because of 'invalid' token
              if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0')
                ;       // OK
              else{
                fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                error = 1;
                break;
              }
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else{
            if (readChNo == i)
              ;       // OK
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }

        readStrPtr = strtok(NULL, ",");
        /*
         * check ENABLE BIT (if error found then break)
         */
        if (readStrPtr != NULL){
          readEnable = atoi(readStrPtr);
          if (readEnable == 0){
            // check if atoi() returned 0 because of 'invalid' token
            if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0'){
              // check if channel is really not enabled
              if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
                fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                error = 1;
                break;
              }
              else
                ;   // OK
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else if (readEnable == 1){
            if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
            else
              ;     // OK
          }
          else{
            fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
            error = 1;
            break;
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }

        readStrPtr = strtok(NULL, ",");
        /*
         * parse VALUE (if error found then break)
         */
        if (readStrPtr != NULL){
          readValue = atoi(readStrPtr);
          if (readValue == 0){
            // check if atoi() returned 0 because of 'invalid' token
            if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0'){
              dppParams.pgate[i] = readValue;
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else{
            dppParams.pgate[i] = readValue;
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }
      }
      else{
        fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
        error = 1;
        break;
      }
      scannerBit = scannerBit << 1;
    }       // end of 'for'

    if ((i == zeroToMaxChIntervalWidth) && (!error))
      success = 1;

  return success;
}

/* Parses the string 'myReadLine' n times where n equals
 * 'zeroToMaxChIntervalWidth' (before parsing 'myReadLine', the string is filled
 * by calling the function 'scanNextLine()'). Then, possibly sets
 * 'dppParams.sgate'. Each line is expected to follow this schema:
 * <channel number>,<'1' if enabled, '0' if disabled>,<value>
 * If one or more lines do not adhere to the schema then no value is set.
 * If an error is found the function prints to stderr its line number.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DPP_sgateParseAndSet(void){
  unsigned int readEnable, scannerBit = 1;
  int readValue, readChNo;
  register int i = 0;
  int success = 0, error = 0;
  char* readStrPtr = NULL;

    for (; i< zeroToMaxChIntervalWidth; i++){
      scanNextLine();
      readStrPtr = myReadLine;
      if (readStrPtr != NULL){
        readStrPtr = strtok(readStrPtr, ",");
        if (readStrPtr != NULL){
          /*
           * check CHANNEL NUMBER token (if error found then break)
           */
          readChNo = atoi(readStrPtr);
          if (i == 0){
            if (readChNo == 0){
              // check if atoi() returned 0 because of 'invalid' token
              if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0')
                ;       // OK
              else{
                fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                error = 1;
                break;
              }
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else{
            if (readChNo == i)
              ;       // OK
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }

        readStrPtr = strtok(NULL, ",");
        /*
         * check ENABLE BIT (if error found then break)
         */
        if (readStrPtr != NULL){
          readEnable = atoi(readStrPtr);
          if (readEnable == 0){
            // check if atoi() returned 0 because of 'invalid' token
            if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0'){
              // check if channel is really not enabled
              if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
                fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                error = 1;
                break;
              }
              else
                ;   // OK
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else if (readEnable == 1){
            if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
            else
              ;     // OK
          }
          else{
            fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
            error = 1;
            break;
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }

        readStrPtr = strtok(NULL, ",");
        /*
         * parse VALUE (if error found then break)
         */
        if (readStrPtr != NULL){
          readValue = atoi(readStrPtr);
          if (readValue == 0){
            // check if atoi() returned 0 because of 'invalid' token
            if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0'){
              dppParams.sgate[i] = readValue;
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else{
            dppParams.sgate[i] = readValue;
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }
      }
      else{
        fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
        error = 1;
        break;
      }
      scannerBit = scannerBit << 1;
    }       // end of 'for'

    if ((i == zeroToMaxChIntervalWidth) && (!error))
      success = 1;

  return success;
}

/* Parses the string 'myReadLine' n times where n equals
 * 'zeroToMaxChIntervalWidth' (before parsing 'myReadLine', the string is filled
 * by calling the function 'scanNextLine()'). Then, possibly sets
 * 'dppParams.lgate'. Each line is expected to follow this schema:
 * <channel number>,<'1' if enabled, '0' if disabled>,<value>
 * If one or more lines do not adhere to the schema then no value is set.
 * If an error is found the function prints to stderr its line number.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DPP_lgateParseAndSet(void){
  unsigned int readEnable, scannerBit = 1;
  int readValue, readChNo;
  register int i = 0;
  int success = 0, error = 0;
  char* readStrPtr = NULL;

    for (; i< zeroToMaxChIntervalWidth; i++){
      scanNextLine();
      readStrPtr = myReadLine;
      if (readStrPtr != NULL){
        readStrPtr = strtok(readStrPtr, ",");
        if (readStrPtr != NULL){
          /*
           * check CHANNEL NUMBER token (if error found then break)
           */
          readChNo = atoi(readStrPtr);
          if (i == 0){
            if (readChNo == 0){
              // check if atoi() returned 0 because of 'invalid' token
              if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0')
                ;       // OK
              else{
                fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                error = 1;
                break;
              }
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else{
            if (readChNo == i)
              ;       // OK
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }

        readStrPtr = strtok(NULL, ",");
        /*
         * check ENABLE BIT (if error found then break)
         */
        if (readStrPtr != NULL){
          readEnable = atoi(readStrPtr);
          if (readEnable == 0){
            // check if atoi() returned 0 because of 'invalid' token
            if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0'){
              // check if channel is really not enabled
              if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
                fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                error = 1;
                break;
              }
              else
                ;   // OK
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else if (readEnable == 1){
            if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
            else
              ;     // OK
          }
          else{
            fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
            error = 1;
            break;
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }

        readStrPtr = strtok(NULL, ",");
        /*
         * parse VALUE (if error found then break)
         */
        if (readStrPtr != NULL){
          readValue = atoi(readStrPtr);
          if (readValue == 0){
            // check if atoi() returned 0 because of 'invalid' token
            if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0'){
              dppParams.lgate[i] = readValue;
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else{
            dppParams.lgate[i] = readValue;
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }
      }
      else{
        fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
        error = 1;
        break;
      }
      scannerBit = scannerBit << 1;
    }       // end of 'for'

    if ((i == zeroToMaxChIntervalWidth) && (!error))
      success = 1;

  return success;
}

/* Parses the string 'myReadLine' n times where n equals
 * 'zeroToMaxChIntervalWidth' (before parsing 'myReadLine', the string is filled
 * by calling the function 'scanNextLine()'). Then, possibly sets
 * 'dppParams.nsbl'. Each line is expected to follow this schema:
 * <channel number>,<'1' if enabled, '0' if disabled>,<value>
 * If one or more lines do not adhere to the schema then no value is set.
 * If an error is found the function prints to stderr its line number.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DPP_nsblParseAndSet(void){
  unsigned int readEnable, scannerBit = 1;
  int readValue, readChNo;
  register int i = 0;
  int success = 0, error = 0;
  char* readStrPtr = NULL;

    for (; i< zeroToMaxChIntervalWidth; i++){
      scanNextLine();
      readStrPtr = myReadLine;
      if (readStrPtr != NULL){
        readStrPtr = strtok(readStrPtr, ",");
        if (readStrPtr != NULL){
          /*
           * check CHANNEL NUMBER token (if error found then break)
           */
          readChNo = atoi(readStrPtr);
          if (i == 0){
            if (readChNo == 0){
              // check if atoi() returned 0 because of 'invalid' token
              if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0')
                ;       // OK
              else{
                fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                error = 1;
                break;
              }
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else{
            if (readChNo == i)
              ;       // OK
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }

        readStrPtr = strtok(NULL, ",");
        /*
         * check ENABLE BIT (if error found then break)
         */
        if (readStrPtr != NULL){
          readEnable = atoi(readStrPtr);
          if (readEnable == 0){
            // check if atoi() returned 0 because of 'invalid' token
            if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0'){
              // check if channel is really not enabled
              if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
                fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                error = 1;
                break;
              }
              else
                ;   // OK
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else if (readEnable == 1){
            if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
            else
              ;     // OK
          }
          else{
            fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
            error = 1;
            break;
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }

        readStrPtr = strtok(NULL, ",");
        /*
         * parse VALUE (if error found then break)
         */
        if (readStrPtr != NULL){
          readValue = atoi(readStrPtr);
          if (readValue == 0){
            // check if atoi() returned 0 because of 'invalid' token
            if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0'){
              dppParams.nsbl[i] = readValue;
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else{
            dppParams.nsbl[i] = readValue;
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }
      }
      else{
        fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
        error = 1;
        break;
      }
      scannerBit = scannerBit << 1;
    }       // end of 'for'

    if ((i == zeroToMaxChIntervalWidth) && (!error))
      success = 1;

  return success;
}

/* Parses the string 'myReadLine' n times where n equals
 * 'zeroToMaxChIntervalWidth' (before parsing 'myReadLine', the string is filled
 * by calling the function 'scanNextLine()'). Then, possibly sets
 * 'dppParams.thr'. Each line is expected to follow this schema:
 * <channel number>,<'1' if enabled, '0' if disabled>,<value>
 * If one or more lines do not adhere to the schema then no value is set.
 * If an error is found the function prints to stderr its line number.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DPP_thrParseAndSet(void){
  unsigned int readEnable, scannerBit=1;
  int readValue, readChNo;
  register int i = 0;
  int success = 0, error=0;
  char* readStrPtr=NULL;

    for (  ; i< zeroToMaxChIntervalWidth; i++  ){
      scanNextLine();
      readStrPtr = myReadLine;
      if (readStrPtr != NULL){
        readStrPtr = strtok(readStrPtr, ",");
        if (readStrPtr != NULL){
          /*
           * check CHANNEL NUMBER token (if error found then break)
           */
          readChNo = atoi(readStrPtr);
          if (i == 0){
            if (readChNo == 0){
              // check if atoi() returned 0 because of 'invalid' token
              if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0')
                ;       // OK
              else{
                fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                error= 1;
                break;
              }
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else{
            if (readChNo == i)
              ;       // OK
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }

        readStrPtr = strtok(NULL, ",");
        /*
         * check ENABLE BIT (if error found then break)
         */
        if (readStrPtr != NULL){
          readEnable = atoi(readStrPtr);
            if (readEnable== 0){
              // check if atoi() returned 0 because of 'invalid' token
              if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0'){
                // check if channel is really not enabled
                if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
                  fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                  error = 1;
                  break;
                }
                else
                  ;   // OK
              }
              else{
                fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                error = 1;
                break;
              }
            }
            else if(readEnable==1){
              if (readEnable ^ ((dttParams.ChannelMask & scannerBit) >> i)){
                fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
                error = 1;
                break;
              }
              else
                ;     // OK
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }

        readStrPtr = strtok(NULL, ",");
        /*
         * parse VALUE (if error found then break)
         */
        if (readStrPtr != NULL){
          readValue = atoi(readStrPtr);
          if (readValue == 0){
            // check if atoi() returned 0 because of 'invalid' token
            if (*readStrPtr == '0' && *(readStrPtr + 1) == '\0'){
              dppParams.thr[i] = readValue;
            }
            else{
              fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
              error = 1;
              break;
            }
          }
          else{
            dppParams.thr[i] = readValue;
          }
        }
        else{
          fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
          error = 1;
          break;
        }
      }
      else{
        fprintf(stderr, "tdcr.ini: error in line %d", currentLineNo);
        error = 1;
        break;
      }
      scannerBit = scannerBit << 1;
    }       // end of 'for'

    if (  (i == zeroToMaxChIntervalWidth) && (!error)  )
      success = 1;

  return success;
}

/* Parses the string 'myReadLine' then possibly sets 'dttParams.ChannelMask'. If
 * the string does not match a valid positive hexadecimal number, then no value
 * is set.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int channelEnableMaskParseAndSet(void){
  uint32_t readMask;
  int success= 0;
    if (myReadLine != NULL){
      errno = 0;
      readMask= strtoul(myReadLine, NULL, 16);
      if (readMask > 0){
        dttParams.ChannelMask= readMask;
        success= 1;
      }
    }
  return success;
}

/* Parses the string 'myReadLine' then possibly sets 'dppParams.trgho'. If
 * the string does not match a non-negative int, then no value is set.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DPP_trghoParseAndSet(void){
  int success= 0;
  int readLineToInt;
    if (myReadLine != NULL){
      if (  !((readLineToInt = atoi(myReadLine))<0)  ){
        if (readLineToInt == 0){
          // check if atoi() returned 0 because of 'invalid' string argument
          if (strcmp(myReadLine, "0") == 0){
            dppParams.trgho= readLineToInt;
            success= 1;
          }
        }
        else{
          dppParams.trgho= readLineToInt;
          success = 1;
        }
      }
    }
  return success;
}

/* Parses the string 'myReadLine' then possibly sets 'dppParams.bltmo'. If
 * the string does not match an int, then no value is set.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DPP_bltmoParseAndSet(void){
  int success = 0;
  int readLineToInt;
    if (myReadLine != NULL){
      readLineToInt= atoi(myReadLine);
      if (readLineToInt == 0){
        // check if atoi() returned 0 because of 'invalid' string argument
        if (strcmp(myReadLine, "0") == 0){
          dppParams.bltmo= readLineToInt;
          success= 1;
        }
      }
      else{
        dppParams.bltmo= readLineToInt;
        success= 1;
      }
    }
  return success;
}

/* Parses the string 'myReadLine' then possibly sets 'dppParams.blthr'. If
 * the string does not match an int, then no value is set.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DPP_blthrParseAndSet(void){
  int success = 0;
  int readLineToInt;
  if (myReadLine != NULL){
    readLineToInt = atoi(myReadLine);
    if (readLineToInt == 0){
      // check if atoi() returned 0 because of 'invalid' string argument
      if (strcmp(myReadLine, "0") == 0){
        dppParams.blthr= readLineToInt;
        success= 1;
      }
    }
    else{
      dppParams.blthr= readLineToInt;
      success = 1;
    }
  }
  return success;
}

/* Parses the string 'myReadLine' then possibly sets 'dppParams.purgap'. If
 * the string does not match an int, then no value is set.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DPP_purgapParseAndSet(void){
  int success= 0;
  int readLineToInt;
    if ( myReadLine!=NULL ){
      readLineToInt= atoi(myReadLine);
      if (readLineToInt == 0){
        // check if atoi() returned 0 because of 'invalid' string argument
        if (strcmp(myReadLine, "0") == 0){
          dppParams.purgap= readLineToInt;
          success = 1;
        }
      }
      else{
        dppParams.purgap= readLineToInt;
        success = 1;
      }
    }
  return success;
}

/* Parses the string 'myReadLine' then possibly sets 'dppParams.purh'. If
 * the string does not match a valid 'CAEN_DGTZ_DPP_PUR_t' element name,
 * then no value is set.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * N.B. Accepted values: CAEN_DGTZ_DPP_PSD_PUR_DetectOnly,
 * CAEN_DGTZ_DPP_PSD_PUR_Enabled
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DPP_purhParseAndSet(void){
  int success = 0;
    if (myReadLine != NULL){
      if (strcmp(myReadLine, "CAEN_DGTZ_DPP_PSD_PUR_DetectOnly") == 0){
        dppParams.purh= CAEN_DGTZ_DPP_PSD_PUR_DetectOnly;
        success = 1;
      }
      else if (strcmp(myReadLine, "CAEN_DGTZ_DPP_PSD_PUR_Enabled") == 0){
        dppParams.purh= CAEN_DGTZ_DPP_PSD_PUR_Enabled;
        success = 1;
      }
    }
  return success;
}

/* Parses the string 'myReadLine' then possibly sets 'dttParams.PulsePolarity'.
 * If the string does not match a valid 'CAEN_DGTZ_PulsePolarity_t' element
 * name, then no value is set.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * N.B. Accepted values: CAEN_DGTZ_PulsePolarityPositive,
 * CAEN_DGTZ_PulsePolarityNegative
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DGTZ_PulsePolarityParseAndSet(void){
  int success = 0;
    if (myReadLine != NULL){
      if (strcmp(myReadLine, "CAEN_DGTZ_PulsePolarityPositive") == 0){
        dttParams.PulsePolarity= CAEN_DGTZ_PulsePolarityPositive;
        success = 1;
      }
      else if (strcmp(myReadLine, "CAEN_DGTZ_PulsePolarityNegative") == 0){
        dttParams.PulsePolarity= CAEN_DGTZ_PulsePolarityNegative;
        success = 1;
      }
    }
  return success;
}

/* Parses the string 'myReadLine' then possibly sets 'dttParams.EventAggr'. If
 * the string does not match a non-negative int, then no value is set.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DPP_EventAggrParseAndSet(void){
  int success= 0;
  int readLineToInt;
    if ( myReadLine!=NULL ){
      if (  !( (readLineToInt = atoi(myReadLine))<0 )  ){
        if (readLineToInt == 0){
          // check if atoi() returned 0 because of 'invalid' string argument
          if (strcmp(myReadLine, "0") == 0){
            dttParams.EventAggr= readLineToInt;
            success = 1;
          }
        }
        else{
          dttParams.EventAggr= readLineToInt;
          success= 1;
        }
      }
    }
  return success;
}

/* Parses the string 'myReadLine' then possibly sets 'dttParams.RecordLength'.
 * If the string is not a valid decimal representation of a 32 bit unsigned int,
 * then no value is set.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DGTZ_RecordLengthParseAndSet(void){
  int success= 0;
  unsigned long int readLineToUnsignedLong;
    if (myReadLine != NULL){
      errno = 0;
      readLineToUnsignedLong = strtoul(myReadLine, NULL, 10);
      if (readLineToUnsignedLong == 0){
        // check if strtoul() returned 0 because of 'invalid' string argument
        if (strcmp(myReadLine, "0") == 0){
          dttParams.RecordLength = readLineToUnsignedLong;
          success = 1;
        }
      }
      else if (readLineToUnsignedLong == ULONG_MAX){
        /* check if strtoul() returned ULONG_MAX because of out of range string
        argument */
        if (errno != ERANGE){
          if (readLineToUnsignedLong <= MAX_UNS_32BIT_INTEGER){
            dttParams.RecordLength = readLineToUnsignedLong;
            success = 1;
          }
        }
      }
      else{
        if (readLineToUnsignedLong <= MAX_UNS_32BIT_INTEGER){
          dttParams.RecordLength = readLineToUnsignedLong;
          success = 1;
        }
      }
    }
  return success;
}

/* Parses the string 'myReadLine' then possibly sets 'dttParams.AcqMode'. If
 * the string does not match a valid 'CAEN_DGTZ_DPP_AcqMode_t' element name,
 * then no value is set.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * N.B. Accepted values: CAEN_DGTZ_DPP_ACQ_MODE_Oscilloscope,
 * CAEN_DGTZ_DPP_ACQ_MODE_List, CAEN_DGTZ_DPP_ACQ_MODE_Mixed
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DPP_AcqModeParseAndSet(void){
  int success = 0;
    if (myReadLine != NULL){
      if (strcmp(myReadLine, "CAEN_DGTZ_DPP_ACQ_MODE_Oscilloscope") == 0){
        dttParams.AcqMode = CAEN_DGTZ_DPP_ACQ_MODE_Oscilloscope;
        success = 1;
      }
      else if (strcmp(myReadLine, "CAEN_DGTZ_DPP_ACQ_MODE_List") == 0){
        dttParams.AcqMode = CAEN_DGTZ_DPP_ACQ_MODE_List;
        success = 1;
      }
      else if (strcmp(myReadLine, "CAEN_DGTZ_DPP_ACQ_MODE_Mixed") == 0){
        dttParams.AcqMode = CAEN_DGTZ_DPP_ACQ_MODE_Mixed;
        success = 1;
      }
    }
  return success;
}

/* Parses the string 'myReadLine' then possibly sets 'dttParams.IOlev'. If
 * the string does not match a valid 'CAEN_DGTZ_IOLevel_t' element name,
 * then no value is set.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * N.B. Accepted values: CAEN_DGTZ_IOLevel_NIM, CAEN_DGTZ_IOLevel_TTL
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DGTZ_IOLevelParseAndSet(void){
  int success = 0;
    if (myReadLine != NULL){
      if (strcmp(myReadLine, "CAEN_DGTZ_IOLevel_TTL") == 0){
        dttParams.IOlev = CAEN_DGTZ_IOLevel_TTL;
        success = 1;
      }
      else if (strcmp(myReadLine, "CAEN_DGTZ_IOLevel_NIM") == 0){
        dttParams.IOlev = CAEN_DGTZ_IOLevel_NIM;
        success = 1;
      }
    }
  return success;
}

/* Parses the string 'myReadLine' then possibly sets 'linkNum'. If
 * the string does not match a non-negative int, then no value is set.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DGTZ_LinkNumParseAndSet(void){
  int success = 0;
  int readLineToInt;
    if (myReadLine != NULL){
      if(  !((readLineToInt = atoi(myReadLine))<0)  ){
        if (readLineToInt == 0){
          // check if atoi() returned 0 because of 'invalid' string argument
          if (strcmp(myReadLine, "0") == 0){
            linkNum = readLineToInt;
            success = 1;
          }
        }
        else{
          linkNum = readLineToInt;
          success = 1;
        }
      }
    }
  return success;
}

/* Parses the string 'myReadLine' then possibly sets 'dttParams.LinkType'. If
 * the string does not match a valid 'CAEN_DGTZ_ConnectionType' element name,
 * then no value is set.
 *
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * N.B. Accepted values: CAEN_DGTZ_USB, CAEN_DGTZ_OpticalLink
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int DGTZ_LinkTypeParseAndSet(void){
  int success = 0;
    if (myReadLine != NULL){
      if (strcmp(myReadLine, "CAEN_DGTZ_USB") == 0){
        dttParams.LinkType = CAEN_DGTZ_USB;
        success = 1;
      }
      else if (strcmp(myReadLine, "CAEN_DGTZ_OpticalLink") == 0){
        dttParams.LinkType = CAEN_DGTZ_OpticalLink;
        success = 1;
      }
    }
  return success;
}

/* Parses the string 'myReadLine' then possibly sets 'dttParams.VMEBaseAddress'.
 * If the string is not a valid decimal representation of a 32 bit unsigned int,
 * then no value is set.
 * 
 * Returns 0 in case of failure otherwise returns a different number.
 *
 * @return 0 in case of failure otherwise returns a different number
 */
static int VMEBaseAddressParseAndSet(void){
  int success= 0;
  unsigned long int readLineToUnsignedLong;
    if (myReadLine != NULL){
      errno = 0;
      readLineToUnsignedLong = strtoul(myReadLine, NULL, 10);
      if (readLineToUnsignedLong == 0){
        // check if strtoul() returned 0 because of 'invalid' string argument
        if (strcmp(myReadLine, "0") == 0){
          dttParams.VMEBaseAddress = readLineToUnsignedLong;
          success = 1;
        }
      }
      else if (readLineToUnsignedLong == ULONG_MAX){
        /* check if strtoul() returned ULONG_MAX because of out of range string
           argument */
        if (errno != ERANGE){
          if (readLineToUnsignedLong <= MAX_UNS_32BIT_INTEGER){
            dttParams.VMEBaseAddress = readLineToUnsignedLong;
            success = 1;
          }
        }
      }
      else{
        if (readLineToUnsignedLong <= MAX_UNS_32BIT_INTEGER){
          dttParams.VMEBaseAddress = readLineToUnsignedLong;
          success = 1;
        }
      }
    }
  return success;
}


#undef MY_BUFF_SIZE
#undef MAX_UNS_32BIT_INTEGER
