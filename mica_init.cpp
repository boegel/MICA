/*
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework.
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

//#include <cstdint>
//#include <sys/mount.h>
//#include <limits.h>

/* MICA includes */
#include "mica_init.h"

/*
 * Setup MICA log file.
 */
void setup_mica_log(ofstream *log){

	(*log).open("mica.log", ios::out|ios::trunc);
	if(!(*log).is_open()){
		ERROR_MSG("Could not create log file, aborting.");
		exit(1);
	}
}

/*
 * Read mica.conf config file for MICA.
 *
 * analysis_type: 'all' | 'ilp' | 'ilp_one' | 'itypes' | 'ppm' | 'reg' | 'stride' | 'memfootprint' | 'memstackdist' | 'custom'
 * interval_size: 'full' | <integer>
 * ilp_size: <integer>
 * itypes_spec_file: <string>
 */
enum CONFIG_PARAM {UNKNOWN_CONFIG_PARAM = -1, ANALYSIS_TYPE = 0, INTERVAL_SIZE, ILP_SIZE, _BLOCK_SIZE, _PAGE_SIZE, ITYPES_SPEC_FILE, APPEND_PID, CONF_PAR_CNT};
const char* config_params_str[CONF_PAR_CNT] = {"analysis_type",   "interval_size", "ilp_size", "block_size", "page_size", "itypes_spec_file"};
enum ANALYSIS_TYPE {UNKNOWN_ANALYSIS_TYPE = -1, ALL=0, ILP, ILP_ONE, ITYPES, PPM, MICA_REG, STRIDE, MEMFOOTPRINT, MEMSTACKDIST, CUSTOM, ANA_TYPE_CNT};
const char* analysis_types_str[ANA_TYPE_CNT] = { "all",   "ilp", "ilp_one", "itypes", "ppm", "reg", "stride", "memfootprint", "memstackdist", "custom"};

enum CONFIG_PARAM findConfigParam(char* s){

	if(strcmp(s, "analysis_type") == 0){ return ANALYSIS_TYPE; }
	if(strcmp(s, "interval_size") == 0){ return INTERVAL_SIZE; }
	if(strcmp(s, "ilp_size") == 0){ return ILP_SIZE; }
	if(strcmp(s, "block_size") == 0){ return _BLOCK_SIZE; }
	if(strcmp(s, "page_size") == 0){ return _PAGE_SIZE; }
	if(strcmp(s, "itypes_spec_file") == 0){ return ITYPES_SPEC_FILE; }
	if(strcmp(s, "append_pid") == 0){ return APPEND_PID; }

	return UNKNOWN_CONFIG_PARAM;
}

enum ANALYSIS_TYPE findAnalysisType(char* s){

	if(strcmp(s, "all") == 0){ return ALL; }
	if(strcmp(s, "ilp") == 0){ return ILP; }
	if(strcmp(s, "ilp_one") == 0){ return ILP_ONE; }
	if(strcmp(s, "itypes") == 0){ return ITYPES; }
	if(strcmp(s, "ppm") == 0){ return PPM; }
	if(strcmp(s, "reg") == 0){ return MICA_REG; }
	if(strcmp(s, "stride") == 0){ return STRIDE; }
	if(strcmp(s, "memfootprint") == 0){ return MEMFOOTPRINT; }
	if(strcmp(s, "memstackdist") == 0){ return MEMSTACKDIST; }
	if(strcmp(s, "custom") == 0){ return CUSTOM; }

	return UNKNOWN_ANALYSIS_TYPE;
}

void read_config(ofstream* log, INT64* intervalSize, MODE* mode, UINT32* _ilp_win_size, UINT32* _block_size, UINT32* _page_size, char** _itypes_spec_file, int* append_pid){

	int i;
	char* param;
	char* val;
	FILE* config_file = fopen("mica.conf","r");

	/* a config file named 'mica.conf' is required */
	if(config_file == (FILE*)NULL){
		cerr << "ERROR: No config file 'mica.conf' found, please create one!" << endl;
		(*log) << "ERROR: No config file 'mica.conf' found, please create one!" << endl;
		exit(1);
	}

	(*log) << "Reading config file ..." << endl;

	param = (char*)checked_malloc(1000*sizeof(char));
	val = (char*)checked_malloc(1000*sizeof(char));

	// default values
	*mode = UNKNOWN_MODE;
	*_ilp_win_size = 0;
	*_block_size = 6; // default block size = 64 bytes (2^6)
	*_page_size = 12; // default page size = 4KB (2^12)

	while(!feof(config_file)){

		if (fscanf(config_file, "%[^:]: %s\n", param, val) != 2)
		{
			cerr << "ERROR: invalid config entry found" << endl;
			(*log) << "ERROR: invalid config entry found" << endl;
			exit(1);
		}

		switch(findConfigParam(param)){

			case ANALYSIS_TYPE:
				// figure out mode we are running in
				cerr << "Analysis type: " << val << endl;

				switch(findAnalysisType(val)){

					case ALL:
						*mode = MODE_ALL;
						cerr << "Measuring ALL characteristics..." << endl;
						(*log) << "Measuring ALL characteristics..." << endl;
						break;

					case ILP:
						*mode = MODE_ILP;
						cerr << "Measuring ILP characteristics..." << endl;
						(*log) << "Measuring ILP characteristics..." << endl;
						break;

					case ILP_ONE:
						*mode = MODE_ILP_ONE;
						cerr << "Measuring ILP characteristics for a given window size..." << endl;
						(*log) << "Measuring ILP characteristics for a given window size..." << endl;
						break;

					case ITYPES:
						*mode = MODE_ITYPES;
						cerr << "Measuring ITYPES characteristics..." << endl;
						(*log) << "Measuring ITYPES characteristics..." << endl;
						break;

					case PPM:
						*mode = MODE_PPM;
						cerr << "Measuring PPM characteristics..." << endl;
						(*log) << "Measuring PPM characteristics..." << endl;
						break;

					case MICA_REG:
						*mode = MODE_REG;
						cerr << "Measuring REG characteristics..." << endl;
						(*log) << "Measuring REG characteristics..." << endl;
						break;

					case STRIDE:
						*mode = MODE_STRIDE;
						cerr << "Measuring STRIDE characteristics..." << endl;
						(*log) << "Measuring STRIDE characteristics..." << endl;
						break;

					case MEMFOOTPRINT:
						*mode = MODE_MEMFOOTPRINT;
						cerr << "Measuring MEMFOOTPRINT characteristics..." << endl;
						(*log) << "Measuring MEMFOOTPRINT characteristics..." << endl;
						break;

					case MEMSTACKDIST:
						*mode = MODE_MEMSTACKDIST;
						cerr << "Measuring MEMSTACKDIST characteristics..." << endl;
						(*log) << "Measuring MEMSTACKDIST characteristics..." << endl;
						break;

					case CUSTOM:
						*mode = MODE_CUSTOM;
						(*log) << "Measuring CUSTOM characteristics..." << endl;
						break;

					default:
						(*log) << endl << "ERROR: Unknown analysis type chosen!" << endl;
						cerr << "Known analysis types:" << endl;
						for(i=0; i < ANA_TYPE_CNT; i++){
							cerr << "\t" << analysis_types_str[i] << endl;
						}
						break;
					}
				break;

			case INTERVAL_SIZE:
				cerr << "interval size: " << val << endl;
				(*log) << "interval size: " << val << endl;

				if(strcmp(val, "full") == 0){
					*intervalSize = -1;
					cerr << "Returning data for full execution..." << endl;
					(*log) << "Returning data for full execution..." << endl;
				}
				else{
					*intervalSize = (INT64) atoll(val);
					cerr << "Returning data for each interval of " << *intervalSize << " instructions..." << endl;
					(*log) << "Returning data for each interval of " << *intervalSize << " instructions..." << endl;
				}
				break;

			case ILP_SIZE:

				*_ilp_win_size = (UINT32)atoi(val);
				cerr << "ILP window size: " << *_ilp_win_size << endl;
				(*log) << "ILP window size: " << *_ilp_win_size << endl;
				break;

			case _BLOCK_SIZE:
				*_block_size = (UINT32)atoi(val);
				cerr << "block size: 2^" << *_block_size << endl;
				(*log) << "block size: 2^" << *_block_size << endl;
				break;

			case _PAGE_SIZE:
				*_page_size = (UINT32)atoi(val);
				cerr << "page size: 2^" << *_page_size << endl;
				(*log) << "page size: 2^" << *_page_size << endl;
				break;

			case ITYPES_SPEC_FILE:
				*_itypes_spec_file = (char*)checked_malloc((strlen(val)+1)*sizeof(char));
				strcpy(*_itypes_spec_file, val);
				cerr << "ITYPES spec file: " << *_itypes_spec_file << endl;
				(*log) << "ITYPES spec file: " << *_itypes_spec_file << endl;
				break;

			case APPEND_PID:
				if (strcmp(val, "yes")==0)
				{
					*append_pid = 1;
					cerr << "append pid: yes" << endl;
					(*log) << "append pid: yes" << endl;
				}
				else if (strcmp(val, "no")==0)
				{
					*append_pid = 0;
					cerr << "append pid: no" << endl;
					(*log) << "append pid: no" << endl;
				}
				else
				{
					cerr << "ERROR! append_pid can be either yes or no" << endl;
					(*log) << "ERROR! append_pid can be either yes or no" << endl;
					exit(1);
				}
				break;
			default:
				cerr << "ERROR: Unknown config parameter specified: " << param << " (" << val << ")" << endl;
				cerr << "Known config parameters:" << endl;
				(*log) << "ERROR: Unknown config parameter specified: " << param << " (" << val << ")" << endl;
				(*log) << "Known config parameters:" << endl;
				for(i=0; i < CONF_PAR_CNT; i++){
					cerr << "\t" << config_params_str[i] << endl;
					(*log) << "\t" << config_params_str[i] << endl;
				}
				exit(1);
				break;
		}
	}
	cerr << "All done reading config" << endl;
	(*log) << "All done reading config" << endl;

	if(*mode == UNKNOWN_MODE){
		cerr << "ERROR! No mode specified, the mica.conf file should specify the \"analysis_type\" config parameter." << endl;
		(*log) << "ERROR! No mode specified, the mica.conf file should specify the \"analysis_type\" config parameter." << endl;
		exit(1);
	}

	if(*mode == MODE_ILP_ONE && *_ilp_win_size == 0){
		cerr << "ERROR! \"ilp_one\" mode was specified, but no window size (ilp_size) was found along with it!" << endl;
		(*log) << "ERROR! ERROR! \"ilp_one\" mode was specified, but no window size (ilp_size) was found along with it!" << endl;
		exit(1);
	}

	(*log).close();

	free(param);
	free(val);

}
