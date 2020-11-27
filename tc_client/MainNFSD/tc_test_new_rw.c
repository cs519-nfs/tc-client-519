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
// #define DEBUG
static char tc_config_path[PATH_MAX];

#define DEFAULT_LOG_FILE "/tmp/tc_test_writev.log"
#define PARENT_DIR "/vfs0/new_test/"

int main(int argc, char** argv){
	
	int N = atoi(argv[1]);
	int i;
	void *context = NULL;
	struct viovec iovec[N];
	vres wres, rres, res;
	const char *data = "hello world";

	context = vinit(get_tc_config_file(tc_config_path, PATH_MAX), DEFAULT_LOG_FILE, 77);

	if(context == NULL){
		NFS4_ERR("Error while initializing tc_client using config "
			"file: %s; see log at %s",
			tc_config_path, DEFAULT_LOG_FILE);
		return EIO;
	}
	char* prior_path;
	for(i = 0; i < N; i++){
		char base = (char) i/2 + 'a';
		char* path = (char*) calloc(1, sizeof(PARENT_DIR) + sizeof(char)*10 + 1);
		sprintf(path, "%s%d", PARENT_DIR, i);
#ifdef DEBUG
		puts(path);
#endif
		if(i%2 == 0){
			//create the file
			// puts(path);
			iovec[i].file = vfile_from_path(path);
			iovec[i].is_creation = true;
			iovec[i].offset = 0;
			iovec[i].length = strlen(data);
			iovec[i].data = (char*) data;
			iovec[i].type = VECTOR_WRITE_OP;
			prior_path = path;
		}else{
			//read from it
			iovec[i].file = vfile_from_path(prior_path);
			iovec[i].offset = 0;
			iovec[i].length = strlen(data);
			iovec[i].data = calloc(1, strlen(data) + 1);
			iovec[i].type = VECTOR_READ_OP;
		}
	}

	struct timeval start, end;
	gettimeofday(&start, NULL);

// 	for(i = 0; i < N; i++){
// 		res = vec_read_write(iovec+i, 1, true);
// #ifdef DEBUG
// 		if(!vokay(wres)){
// 			printf("An error has occured with operation: %d", i);
// 		}else if(i%2 != 0){
// 			//If it is both okay and an odd operaiton print the read result
// 			printf("This is the data that was read in: %s\n", iovec[i].data);
// 		}
// #endif
// 	}
	res = vec_read_write(iovec, N, true);
	// if(!vokay(res)){

	// }
	gettimeofday(&end, NULL);
#ifdef DEBUG
	for(i = 0; i < N; i++){
		if(i%2 != 0){
			//Print the read result
			printf("This is the read result: %s\n", iovec[i].data);
		}
	} 
#endif
	long elapsed = ((end.tv_sec - start.tv_sec) * 1000000) + (end.tv_usec - start.tv_usec);
	fprintf(stdout, "Elapsed Time: %ld\n", elapsed);

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

	vdeinit(context);
	return wres.err_no;
}
