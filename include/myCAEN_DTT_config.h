/* DTT version: 5720 desktop (with DPP_PSD firmware)
 * CAEN library version: Rel. 2.6.8  - Nov 2015
 * 'myCAEN_DTT_config' module version: a0.4
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
 */

#ifndef _MYCAEN_DTT_CONFIG
  #define _MYCAEN_DTT_CONFIG
  #include "Functions.h"
  #include <CAENDigitizerType.h>

  extern DigitizerParams_t dttParams;
  extern CAEN_DGTZ_DPP_PSD_Params_t dppParams;
  extern unsigned long int acqTime;
  extern int linkNum;
  /* Parses "tdcr.ini" to fill 'dttParams', 'dppParams', 'linkNum' and 'acqTime'
   * then permits interactive parameter editing via shell.
   * If "tdcr.ini" isn't found, a default version is automatically created.
   *
   * @return 0 if the user asked to quit the program, 1 if the user asked to start
   * the acquisition
   */
  extern int acquireParameterValues(void);
#endif
