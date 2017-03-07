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

#ifndef _MULTIVALUEPARAMETERSSTATS
  #define _MULTIVALUEPARAMETERSSTATS
  /* Allocates the memory needed by the data structures used by this module and
   * initializes the structures 'paramNeedToUpdateList', 'paramNameList',
   * 'paramNumOfValues'.
   * Sets the status of the module considering the width of the interval between
   * the higher DTT channel 'enabled' and channel number 0 equal to
   * 'zeroToMaxChIntervalWidthValue'.
   * Initializes the module status to consider all the multi-value
   * parameters as having 'zeroToMaxChIntervalWidthValue' values.
   *
   * @param zeroToMaxChIntervalWidthValue the value used to set
   * 'zeroToMaxChIntervalWidth'
   */
  extern void initMultiValueParametersStats(unsigned int zeroToMaxChIntervalWidthValue);
  /* Prints to stdout a list of comma-separated parameters that need to be
   * updated by the user
   */
  extern void printParamsToUpdate(void);
  /* 1. Sets the value of the variable that stores the width of the interval
   * between the higher DTT channel enabled and channel number 0.
   * 2. Calls updateUpdatedParametersList()
   *
   * @param value the value that will be stored
   */
  extern void setMVPSZeroToMaxChIntervalWidth(unsigned int value);
  /* Returns the value of the variable that stores the width of the interval
   * between the higher DTT channel enabled and channel number 0.
   *
   * @return the value of the variable that stores the width of the interval
   * between the higher DTT channel enabled and channel number 0
   */
  extern unsigned getMVPSZeroToMaxChIntervalWidth(void);
  /* Returns a non-zero integer if all the multi-value parameters do not need
   * to be updated to reach the minimum allowed number of channels configured.
   * Otherwise returns 0.
   *
   * @return a non-zero integer if all the multi-value parameters do not need
   * to be updated to reach the minimum allowed number of channels configured.
   * Otherwise returns 0.
   */
  extern int areParamsUpdated(void);
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
  extern void setParamAsUpdated(unsigned paramNo);
  /* Frees the allocated memory for the data structures used by this module.
   */
  extern void freeMultiValParamStatsDataStructuresMemory(void);
#endif
