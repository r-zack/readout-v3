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

#ifndef _PARAMS_HEADER_TO_FILE
  #define _PARAMS_HEADER_TO_FILE
  #include <stdio.h>

  /* Writes to the beginning of the file pointed by the parameter a header
	 * containing a textual representation of the CAEN DTT/DPP-PSD parameter
	 * values, the digitizer's 'Link number' and the acquisition time.
   *
   * @param fpout pointer to the file to write to.
	 * @return 0 if the function returns normally, otherwise if file errors are
   * encountered returns a non zero integer and a description of the encountered
   * error is printed to stderr.
	 */
	extern int printParamsHeaderToFile(FILE *fpout);
#endif
