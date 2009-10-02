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

	int cnt;
	char name[20];
	sprintf(name, "mica.log");
	FILE* test = fopen(name,"r");

	cnt = 0;
	while(test != (FILE*)NULL){
		sprintf(name, "mica.log.%d", ++cnt);
		test = fopen(name,"r");
	}

	*log = fopen(name,"w");
	if( *log == (FILE*)NULL ){
		fprintf(stderr,"Could not create mica.log, aborting!\n");
		exit(1);
	}
	else{
		if( strcmp(name,"mica.log") != 0)
			fprintf(stderr,"\n   WARNING: Writing log messages to %s, because a file named \'mica.log\' already exists.\n", name);
	}
}

/*
 * Read mica.conf config file for MICA.
 *
 * analysis_type: 'all' | 'ilp' | 'ilp_one' | 'itypes' | 'ppm' | 'reg' | 'stride' | 'workingset'
 * interval_size: 'full' | <integer>
 * ilp_size: <integer>
 */

void read_config(FILE* log, INT64* interval_size, MODE* mode, UINT32* _win_size){

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
										if(strcmp(string,"mytype") == 0){
											*mode = MODE_MYTYPE;
											LOG_MSG("Measuring MYTYPE characteristics...\n");
										}
										else{
											LOG_MSG("\nERROR: Unknown set of characteristics chosen!\n");
											LOG_MSG("   Available characteristics include: 'all', 'ilp', 'ilp_one', 'itypes', 'ppm', 'reg', 'stride', 'memfootprint', 'memreusedist'\n");
											ERROR("\nERROR: Unknown set of characteristics chosen!\n");
											ERROR("   Available characteristics include: 'all', 'ilp', 'ilp_one', 'itypes', 'ppm', 'reg', 'stride', 'memfootprint', 'memreusedist'\n");
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

	if(*mode == MODE_ILP_ONE){
		fscanf(config_file,"ilp_size: %s\n", string);
		*_win_size = (UINT32)atoi(string);
		LOG_MSG("ILP window size: %d\n", *_win_size);
	}

	DEBUG("All done reading config\n");

}
