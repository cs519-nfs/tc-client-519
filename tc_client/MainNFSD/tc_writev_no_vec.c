/**
 * Copyright (C) Stony Brook University 2016
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

/**
 * This is an example creating and writing to 2 files with one RPC.
 * It has the same effect as the bash command:
 *
 *  $ echo "hello world" > /vfs0/test/abcd0
 *  $ echo "hello world" > /vfs0/test/abcd1
 *
 * @file tc_test_writev.c
 * @brief Test create and write to 2 small files from NFS using TC.
 *
 */
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

#define TC_TEST_NFS_FILE0 "/vfs0/test/abcd0"
#define TC_TEST_NFS_FILE1 "/vfs0/test/abcd1"

#define PARENT_DIR "/vfs0/test/"

int main(int argc, char *argv[])
{
	int N = atoi(argv[1]);
	int i;
	void *context = NULL;
	struct viovec write_iovec[N], read_iovec[N];
	struct viovec write_iovec_multi;
	struct timeval start, end;
	vres res;
	const char *data = "hello world";
	
	if(atoi(argv[3]) == 1){
		/* Initialize TC services and daemons */
		context = vinit(get_tc_config_file(tc_config_path, PATH_MAX),
				DEFAULT_LOG_FILE, 77);
	}else{
		context = vinit(NULL, DEFAULT_LOG_FILE, 0);
	}


	if (context == NULL) {
		NFS4_ERR("Error while initializing tc_client using config "
			 "file: %s; see log at %s",
			 tc_config_path, DEFAULT_LOG_FILE);
		return EIO;
	}

	char* paths[N];
	for(i = 0; i < N; i++){
		/*char* path = (char*) calloc(1, sizeof(PARENT_DIR) + sizeof(char)*10 + 1);*/
		/*sprintf(path, "%s%d", PARENT_DIR, i);*/
		/*paths[i] = path;*/
		char base = (char) i + '1';
		char* path = (char*) malloc(sizeof(PARENT_DIR) + sizeof(char) + 1);
		strcpy(path, PARENT_DIR);
		strcat(path, &base);
		fprintf(stdout, "%s\n", path);
	}

	vfile* files = vec_open_simple(paths, N, O_CREAT | O_WRONLY, 0644);
	for(i = 0; i < N; i++){
		write_iovec[i].file = files[i];
		write_iovec[i].offset = 0;
		write_iovec[i].length = strlen(data);
		write_iovec[i].data = (char*) data;
		write_iovec[i].is_write_stable = true;
		fprintf(stdout, "%s\n", write_iovec[i].file.path);
	}
	
	if(atoi(argv[2]) == 1){
		gettimeofday(&start, NULL);
		res = vec_write(write_iovec, N, false);
		gettimeofday(&end, NULL);
	}else{
		gettimeofday(&start, NULL);
		for(i = 0; i < N; i++){
			res = vec_write(write_iovec+i, 1, false);
		}
		gettimeofday(&end, NULL);
	}

	/*for(i = 0; i < N; i++){*/
		/*read_iovec[i].file = files[i];*/
		/*read_iovec[i].offset = 0;*/
		/*read_iovec[i].length = strlen(data);*/
		/*read_iovec[i].data = calloc(1, strlen(data) + 1);*/
	/*}*/

	/*if(atoi(argv[2]) == 1){*/
                /*gettimeofday(&start, NULL);*/
                /*res = vec_read(read_iovec, N, false);*/
                /*gettimeofday(&end, NULL);*/
    /*}else{*/
                /*gettimeofday(&start, NULL);*/
                /*for(i = 0; i < N; i++){*/
                        /*res = vec_read(read_iovec+i, 1, false);*/
			/*fprintf(stdout, "%s", read_iovec[i].data);*/
				/*}*/
                /*gettimeofday(&end, NULL);*/
    /*}*/

	vec_close(files, N);

	/* Write the file using NFS compounds; nfs4_writev() will open the file
	 * with CREATION flag, write to it, and then close it. */
	long elapsed = ((end.tv_sec - start.tv_sec) * 1000000) + (end.tv_usec - start.tv_usec);
	fprintf(stdout, "Elapsed Time: %ld\n", elapsed);


	/* Check results. */
	if (vokay(res)) {
		fprintf(stderr,
			"Successfully write the first %zu bytes of file \"%s\" "
			"via NFS.\n",
			write_iovec[0].length, TC_TEST_NFS_FILE0);
		fprintf(stderr,
			"Successfully write the first %zu bytes of file \"%s\" "
			"via NFS.\n",
			write_iovec[1].length, TC_TEST_NFS_FILE1);
	} else {
		fprintf(stderr,
			"Failed to write file at the %d-th operation "
			"with error code %d (%s). See log file for details: "
			"%s\n",
			res.index, res.err_no,
			strerror(res.err_no),
			DEFAULT_LOG_FILE);
	}

	vdeinit(context);

	return res.err_no;
}
