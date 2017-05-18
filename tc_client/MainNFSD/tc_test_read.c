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
 * This is an example showing how tc_client/include/tc_api.h should be used.
 *
 * @file tc_test_read.c
 * @brief Test read a small file from NFS using TC.
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

static char exe_path[PATH_MAX];
static char tc_config_path[PATH_MAX];

#define DEFAULT_LOG_FILE "/tmp/tc_test_read.log"

#define TC_TEST_NFS_FILE0 "/vfs0/test/abcd0"
#define TC_TEST_NFS_FILE1 "/vfs0/test/abcd1"

int main(int argc, char *argv[])
{
	void *context = NULL;
	struct tc_iovec read_iovec[4];
	tc_res res;

	/* Locate and use the default config file in the repo.  Before running
	 * this example, please update the config file to a correct NFS server.
	 */
	if (readlink("/proc/self/exe", exe_path, PATH_MAX) < 0) {
		perror("readlink");
		exit(EXIT_FAILURE);
	}
	snprintf(tc_config_path, PATH_MAX,
		 "%s/../../../config/vfs.proxy.conf", dirname(exe_path));
	fprintf(stderr, "using config file: %s\n", tc_config_path);

	/* Initialize TC services and daemons */
	context = tc_init(tc_config_path, DEFAULT_LOG_FILE, 77);
	if (context == NULL) {
		NFS4_ERR("Error while initializing tc_client using config "
			 "file: %s; see log at %s",
			 tc_config_path, DEFAULT_LOG_FILE);
		return EIO;
	}

	/* Setup I/O request */
	read_iovec[0].file = tc_file_from_path(TC_TEST_NFS_FILE0);
	read_iovec[0].offset = 0;
	read_iovec[0].length = 16384;
	read_iovec[0].data = malloc(16384);
	assert(read_iovec[0].data);
	read_iovec[1].file = tc_file_current();
	read_iovec[1].offset = 16384;
	read_iovec[1].length = 16384;
	read_iovec[1].data = malloc(16384);
	assert(read_iovec[1].data);

	read_iovec[2].file = tc_file_from_path(TC_TEST_NFS_FILE1);
	read_iovec[2].offset = 0;
	read_iovec[2].length = 16384;
	read_iovec[2].data = malloc(16384);
	assert(read_iovec[2].data);
	read_iovec[3].file = tc_file_current();
	read_iovec[3].offset = 16384;
	read_iovec[3].length = 16384;
	read_iovec[3].data = malloc(16384);
	assert(read_iovec[3].data);

	/* Read the file; nfs4_readv() will open it first if needed. */
	res = tc_readv(read_iovec, 4, false);

	/* Check results. */
	if (tc_okay(res)) {
		fprintf(stderr,
			"Successfully read the first %zu bytes of file \"%s\" "
			"via NFS.\n",
			read_iovec[0].length, TC_TEST_NFS_FILE0);
	} else {
		fprintf(stderr,
			"Failed to read file \"%s\" at the %d-th operation "
			"with error code %d (%s). See log file for details: "
			"%s\n",
			TC_TEST_NFS_FILE0, res.index, res.err_no,
			strerror(res.err_no),
			DEFAULT_LOG_FILE);
	}

	tc_deinit(context);

	return res.err_no;
}
