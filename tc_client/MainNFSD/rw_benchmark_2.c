#include "config.h"
#include "nfs_init.h"
#include "fsal.h"
#include "log.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>		/* for sigaction */
#include <errno.h>
#include <libgen.h>		/* used for 'dirname' */
#include "../nfs4/nfs4_util.h"
#include "tc_helper.h"
#define MINIMAL_OUTPUT
// #define DEBUG
static char tc_config_path[PATH_MAX];

#define DEFAULT_LOG_FILE "/tmp/rw_benchmark_2.log"
#define PARENT_DIR "/vfs0/new_test/"

int main(int argc, char** argv){

	if(argc < 4){
		puts("Invalid amount of arguments.");
		puts("Please provide: N (number of iterations as a number) Type (Seperate vectors or single vector)");
		puts("Spacing (number of operations packed into generic vector)");
		return EXIT_FAILURE;
	}

	int N = atoi(argv[1]);
	int type = atoi(argv[2]);
	int spacing = atoi(argv[3]);
	long i, j;
	void *context = NULL;
	long vector_count = N*2*spacing;

	struct viovec iovec[vector_count];

#ifdef DEBUG
	printf("Amount I am using: %ld\n", vector_count);
#endif

	vres wres, rres, res;
	const char *data = "hello world";

	context = vinit(get_tc_config_file(tc_config_path, PATH_MAX), DEFAULT_LOG_FILE, 77);

	if(context == NULL){
		NFS4_ERR("Error while initializing tc_client using config "
			"file: %s; see log at %s",
			tc_config_path, DEFAULT_LOG_FILE);
		return EIO;
	}

	char * prior_paths[spacing];
	for(i = 0; i < vector_count;){
		int loc_to_consider = i/2;
#ifdef DEBUG
		printf("Loop: %ld\n", i);
#endif
		//Create the write vector
		for(j = 0; j<spacing; j++){
			int location = loc_to_consider+j;
			//char base = (char) location + 'a';
			char* path = (char*) calloc(1, sizeof(PARENT_DIR) + sizeof(char)*10 + 1);
			sprintf(path, "%s%d", PARENT_DIR, location);
#ifdef DEBUG
			puts(path);
#endif
			iovec[i].file = vfile_from_path(path);
			iovec[i].is_creation = true;
			iovec[i].offset = 0;
			iovec[i].length = strlen(data);
			iovec[i].data = (char*) data;
			iovec[i].type = VECTOR_WRITE_OP;
			prior_paths[j] = path;
			i++;
#ifdef DEBUG
			printf("i: %ld \n", i);
#endif
		}

#ifdef DEBUG
		puts("Write done.");
#endif
		//and the read vector
		for(j = 0; j<spacing; j++){
			char* path = prior_paths[j];
#ifdef DEBUG
			puts(path);
#endif
			iovec[i].file = vfile_from_path(path);
			iovec[i].offset = 0;
			iovec[i].length = strlen(data);
			iovec[i].data = calloc(1, strlen(data) + 1);
			iovec[i].type = VECTOR_READ_OP;
			i++;
#ifdef DEBUG
			printf("i: %ld \n", i);
#endif
		}
#ifdef DEBUG
		puts("Read done.");
#endif
	}
	struct timeval start, end;
	if(type){
		gettimeofday(&start, NULL);
		res = vec_read_write(iovec, vector_count, true);
		gettimeofday(&end, NULL);
	}else{
		//Test basic seperate vectors
		gettimeofday(&start, NULL);
		for(i = 0; i < vector_count; ){
			wres = vec_write(iovec+i, spacing, true);
			i += spacing;
			rres = vec_read(iovec+i, spacing, true);
			i += spacing;
		}
		gettimeofday(&end, NULL);
	}

	long elapsed = ((end.tv_sec - start.tv_sec) * 1000000) + (end.tv_usec - start.tv_usec);


#ifndef MINIMAL_OUTPUT
	fprintf(stdout, "Elapsed Time: %ld\n", elapsed);
#else
	fprintf(stdout, "%ld\n", elapsed);
#endif

#ifndef MINIMAL_OUTPUT
//Tests to see if it was successful for our general new rw vector
	if(vokay(res)){
		fprintf(stderr, "Successfully wrote/read all %d files\n", N);
	}else{
		// fprintf(stderr, "Failed to write/read %d files\n", N);
		fprintf(stderr,
			"Failed to write file at the %d-th operation "
			"with error code %d (%s). See log file for details: "
			"%s\n",
			res.index, res.err_no,
			strerror(res.err_no),
			DEFAULT_LOG_FILE);
	}
#endif

	vdeinit(context);
	return wres.err_no;
}
