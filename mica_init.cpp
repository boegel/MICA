/* 
 * This file is part of MICA, a Pin tool to collect
 * microarchitecture-independent program characteristics using the Pin
 * instrumentation framework. 
 *
 * Please see the README.txt file distributed with the MICA release for more
 * information.
 */

/* MICA includes */
#include "mica_init.h"

/*
 * Setup MICA log file.
 */
void setup_mica_log(FILE* *log){

	char name[20];
	sprintf(name, "mica.log");

	*log = fopen(name,"w");
	if( *log == (FILE*)NULL ){
		fprintf(stderr,"Could not create mica.log, aborting!\n");
		exit(1);
	}
}

/*
 * Read mica.conf config file for MICA.
 *
 * analysis_type: 'all' | 'ilp' | 'ilp_one' | 'itypes' | 'ppm' | 'reg' | 'stride' | 'workingset' | 'custom'
 * interval_size: 'full' | <integer>
 * ilp_size: <integer>
 * itypes_spec_file: <string>
 */

void read_config(FILE* log, INT64* interval_size, MODE* mode, UINT32* _ilp_win_size, char** _itypes_spec_file){

	char* string;
	FILE* config_file = fopen("mica.conf","r");

	/* a config file named 'mica.conf' is required */
	if(config_file == (FILE*)NULL){
		ERROR("ERROR: No config file 'mica.conf' found, please create one!\n");
		LOG_MSG("ERROR: No config file 'mica.conf' found, please create one!\n");
		exit(1);
	}

	LOG_MSG("Reading config file ...\n");

	if((string = (char*)malloc(100*sizeof(char))) == (char*)NULL){
		ERROR("ERROR: Could not allocate memory for string\n");
		LOG_MSG("ERROR: Could not allocate memory for string\n");
		exit(1);	
	}

	fscanf(config_file,"analysis_type: %s\n",string);
	DEBUG_MSG("Analysis type: %s\n",string);

	// figure out mode we are running in
	if(strcmp(string,"all") == 0){
		*mode = MODE_ALL;
		LOG_MSG("Measuring ALL characteristics...\n");
	}
	else{
		if(strcmp(string,"ilp") == 0){
			*mode = MODE_ILP;
			LOG_MSG("Measuring ILP characteristics...\n");
		}
		else{
			if(strcmp(string,"ilp_one") == 0){
				*mode = MODE_ILP_ONE;
				LOG_MSG("Measuring ILP characteristics for a given window size...\n");
			}
			else{
				if(strcmp(string,"itypes") == 0){
					*mode = MODE_ITYPES;
					LOG_MSG("Measuring ITYPES characteristics...\n");
				}
				else{
					if(strcmp(string,"ppm") == 0){
						*mode = MODE_PPM;
						LOG_MSG("Measuring PPM characteristics...\n");
					}
					else{
						if(strcmp(string,"reg") == 0){
							*mode = MODE_REG;
							LOG_MSG("Measuring REG characteristics...\n");
						}
						else{
							if(strcmp(string,"stride") == 0){
								*mode = MODE_STRIDE;
								LOG_MSG("Measuring STRIDE characteristics...\n");
							}
							else{
								if(strcmp(string,"memfootprint") == 0){
									*mode = MODE_MEMFOOTPRINT;
									LOG_MSG("Measuring MEMFOOTPRINT characteristics...\n");
								}
								else{
									if(strcmp(string,"memreusedist") == 0){
										*mode = MODE_MEMREUSEDIST;
										LOG_MSG("Measuring MEMREUSEDIST characteristics...\n");
									}
									else{
										if(strcmp(string,"custom") == 0){
											*mode = MODE_CUSTOM;
											LOG_MSG("Measuring CUSTOM characteristics...\n");
										}
										else{
											LOG_MSG("\nERROR: Unknown set of characteristics chosen!\n");
											LOG_MSG("   Available characteristics include: 'all', 'ilp', 'ilp_one', 'itypes', 'ppm', 'reg', 'stride', 'memfootprint', 'memreusedist', 'custom'\n");
											ERROR("\nERROR: Unknown set of characteristics chosen!\n");
											ERROR("   Available characteristics include: 'all', 'ilp', 'ilp_one', 'itypes', 'ppm', 'reg', 'stride', 'memfootprint', 'memreusedist', 'custom'\n");
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	fscanf(config_file,"interval_size: %s\n", string);

	LOG_MSG("Interval size: %s\n", string);

	if(strcmp(string, "full") == 0){
		*interval_size = -1;
		LOG_MSG("Returning data for full execution...\n");
	}
	else{
		*interval_size = (INT64) atoll(string);
		LOG_MSG("Returning data for each interval of %lld instructions...\n", (INT64)*interval_size);
	}

        // read window size for ILP_ONE
	if(*mode == MODE_ILP_ONE){
                if(fscanf(config_file,"ilp_size: %s\n", string) == 1){
                        *_ilp_win_size = (UINT32)atoi(string);
                        LOG_MSG("ILP window size: %d\n", *_ilp_win_size);
                }
                else{
                        fprintf(stderr, "ERROR! ILP_ONE mode was specified, but no window size was found along with it!\n");
                        exit(-1);
                }
	}

        // possibly read itypes specification filename
        *_itypes_spec_file = NULL;
        if(*mode == MODE_ITYPES || *mode == MODE_ALL){
                if(fscanf(config_file,"itypes_spec_file: %s\n", string) == 1){
                        *_itypes_spec_file = (char*)malloc((strlen(string)+1)*sizeof(char));
                        strcpy(*_itypes_spec_file, string);
                        fprintf(stdout,"ITYPES spec file: %s\n", *_itypes_spec_file);
                        LOG_MSG("ITYPES spec file: %s\n", *_itypes_spec_file);
                }
        }

	DEBUG_MSG("All done reading config\n");

}
