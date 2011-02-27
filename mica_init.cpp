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
void setup_mica_log(ofstream *log){

	char name[20];
	sprintf(name, "mica.log");

	//*log = fopen(name,"w");
        (*log).open(name, ios::out|ios::trunc);
	//if( *log == (FILE*)NULL ){
	if( ! (*log).is_open() ){
		cerr << "Could not create mica.log, aborting!" << endl;
		exit(1);
	}
}

/*
 * Read mica.conf config file for MICA.
 *
 * analysis_type: 'all' | 'ilp' | 'ilp_one' | 'itypes' | 'ppm' | 'reg' | 'stride' | 'memfootprint' | 'memreusedist' | 'custom'
 * interval_size: 'full' | <integer>
 * ilp_size: <integer>
 * itypes_spec_file: <string>
 */

void read_config(ofstream* log, INT64* interval_size, MODE* mode, UINT32* _ilp_win_size, UINT32* _block_size, UINT32* _page_size, char** _itypes_spec_file){

	char* string;
	FILE* config_file = fopen("mica.conf","r");

	/* a config file named 'mica.conf' is required */
	if(config_file == (FILE*)NULL){
		cerr << "ERROR: No config file 'mica.conf' found, please create one!" << endl;
		(*log) << "ERROR: No config file 'mica.conf' found, please create one!" << endl;
		exit(1);
	}

	(*log) << "Reading config file ..." << endl;

	if((string = (char*)malloc(100*sizeof(char))) == (char*)NULL){
		cerr << "ERROR: Could not allocate memory for string" << endl;
		(*log) << "ERROR: Could not allocate memory for string" << endl;
		exit(1);	
	}

	fscanf(config_file,"analysis_type: %s\n",string);
	cerr << "Analysis type: " << string << endl;

	// figure out mode we are running in
	if(strcmp(string,"all") == 0){
		*mode = MODE_ALL;
		(*log) << "Measuring ALL characteristics..." << endl;
	}
	else{
		if(strcmp(string,"ilp") == 0){
			*mode = MODE_ILP;
			(*log) << "Measuring ILP characteristics..." << endl;
		}
		else{
			if(strcmp(string,"ilp_one") == 0){
				*mode = MODE_ILP_ONE;
				(*log) << "Measuring ILP characteristics for a given window size..." << endl;
			}
			else{
				if(strcmp(string,"itypes") == 0){
					*mode = MODE_ITYPES;
					(*log) << "Measuring ITYPES characteristics..." << endl;
				}
				else{
					if(strcmp(string,"ppm") == 0){
						*mode = MODE_PPM;
						(*log) << "Measuring PPM characteristics..." << endl;
					}
					else{
						if(strcmp(string,"reg") == 0){
							*mode = MODE_REG;
							(*log) << "Measuring REG characteristics..." << endl;
						}
						else{
							if(strcmp(string,"stride") == 0){
								*mode = MODE_STRIDE;
								(*log) << "Measuring STRIDE characteristics..." << endl;
							}
							else{
								if(strcmp(string,"memfootprint") == 0){
									*mode = MODE_MEMFOOTPRINT;
									(*log) << "Measuring MEMFOOTPRINT characteristics..." << endl;
								}
								else{
									if(strcmp(string,"memreusedist") == 0){
										*mode = MODE_MEMREUSEDIST;
										(*log) << "Measuring MEMREUSEDIST characteristics..." << endl;
									}
									else{
										if(strcmp(string,"custom") == 0){
											*mode = MODE_CUSTOM;
											(*log) << "Measuring CUSTOM characteristics..." << endl;
										}
										else{
											(*log) << endl << "ERROR: Unknown set of characteristics chosen!" << endl;
											(*log) << "   Available characteristics include: 'all', 'ilp', 'ilp_one', 'itypes', 'ppm', 'reg', 'stride', 'memfootprint', 'memreusedist', 'custom'" << endl;
											cerr << endl << "ERROR: Unknown set of characteristics chosen!" << endl;
											cerr << endl << "   Available characteristics include: 'all', 'ilp', 'ilp_one', 'itypes', 'ppm', 'reg', 'stride', 'memfootprint', 'memreusedist', 'custom'" << endl;
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

	(*log) << "Interval size: " << string << endl;

	if(strcmp(string, "full") == 0){
		*interval_size = -1;
		(*log) << "Returning data for full execution..." << endl;
	}
	else{
		*interval_size = (INT64) atoll(string);
		(*log) << "Returning data for each interval of " << *interval_size << " instructions..." << endl;
	}

        // read window size for ILP_ONE
	if(*mode == MODE_ILP_ONE){
                if(fscanf(config_file,"ilp_size: %s\n", string) == 1){
                        *_ilp_win_size = (UINT32)atoi(string);
                        (*log) << "ILP window size: " << *_ilp_win_size << endl;
                }
                else{
                        cerr << "ERROR! ILP_ONE mode was specified, but no window size was found along with it!" << endl;
                        exit(-1);
                }
	}

        // read block size
        *_block_size = 6; // default block size = 64 bytes (2^6)
	if(*mode == MODE_ILP_ONE || *mode == MODE_ILP || *mode == MODE_MEMFOOTPRINT || *mode == MODE_MEMREUSEDIST || *mode == MODE_ALL){
                if(fscanf(config_file,"block_size: %s\n", string) == 1){
                        *_block_size = (UINT32)atoi(string);
                        (*log) << "block size: 2^" << *_block_size << endl;
                }
	}

        // read page size
        *_page_size = 12; // default page size = 4KB (2^12)
	if(*mode == MODE_MEMFOOTPRINT || *mode == MODE_ALL){
                if(fscanf(config_file,"page_size: %s\n", string) == 1){
                        *_page_size = (UINT32)atoi(string);
                        (*log) << "page size: 2^" << *_page_size << endl;
                }
	}

        // possibly read itypes specification filename
        *_itypes_spec_file = NULL;
        if(*mode == MODE_ITYPES || *mode == MODE_ALL){
                if(fscanf(config_file,"itypes_spec_file: %s\n", string) == 1){
                        *_itypes_spec_file = (char*)malloc((strlen(string)+1)*sizeof(char));
                        strcpy(*_itypes_spec_file, string);
                        (*log) << "ITYPES spec file: " << *_itypes_spec_file << endl;
                }
        }

	cerr << "All done reading config" << endl;

        (*log).close();

}
