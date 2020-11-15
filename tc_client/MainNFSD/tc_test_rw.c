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

static char tc_config_path[PATH_MAX];

#define DEFAULT_LOG_FILE "/tmp/tc_test_writev.log"
#define PARENT_DIR "/vfs0/test"

int main(int argc, char** argv){
	
	int N = atoi(argv[1]);
	int i;
	void *context = NULL;
	struct viovec write_iovec[N], read_iovec[N];
	vres wres, rres;
	const char *data = "hello world";

	context = vinit(get_tc_config_file(tc_config_path, PATH_MAX), DEFAULT_LOG_FILE, 77);

	if(context == NULL){
		NFS4_ERR("Error while initializing tc_client using config "
			"file: %s; see log at %s",
			tc_config_path, DEFAULT_LOG_FILE);
		return EIO;
	}

	for(i = 0; i < N; i++){
		char base = (char) i + 'a';
		char* path = (char*) calloc(1, sizeof(PARENT_DIR) + sizeof(char)*10 + 1);
		sprintf(path, "%s%d", PARENT_DIR, i);

		write_iovec[i].file = vfile_from_path(path);
		write_iovec[i].is_creation = true;
		write_iovec[i].offset = 0;
		write_iovec[i].length = strlen(data);
		write_iovec[i].data = (char*) data;
	
		read_iovec[i].file = vfile_from_path(path);
		read_iovec[i].offset = 0;
		read_iovec[i].length = strlen(data);
		read_iovec[i].data = calloc(1, strlen(data) + 1);
	}

	struct timeval start, end;
	gettimeofday(&start, NULL);
	for(i = 0; i < N; i++){
		wres = vec_write(write_iovec+i, 1, true);
		rres = vec_read(read_iovec+i, 1, false);
	}
	gettimeofday(&end, NULL);

	long elapsed = ((end.tv_sec - start.tv_sec) * 1000000) + (end.tv_usec - start.tv_usec);
	fprintf(stdout, "Elapsed Time: %ld\n", elapsed);

	if(vokay(wres) && vokay(rres)){
		fprintf(stderr, "Successfully wrote/read all %d files\n", N);
	}else{
		fprintf(stderr, "Failed to write/read %d files\n", N);
	}

	vdeinit(context);
	return wres.err_no;
}
