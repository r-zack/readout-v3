/* This module allows the user to create a default config file for CAEN DTT
 * configuration module 'myCAEN_DTT_config'.
 *
 * 'defaultConfigFileBuilder' module version: a0.3
 */
#ifndef _DEFAULT_CONFIG_FILE_BUILDER
  #define _DEFAULT_CONFIG_FILE_BUILDER
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
  extern int createDefaultConfigFile(int overwrite);
#endif
