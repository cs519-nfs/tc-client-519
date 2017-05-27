/**
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
#include "tc_helper.h"

static char tc_config_path[PATH_MAX];

#define DEFAULT_LOG_FILE "/tmp/tc_test_open.log"

#define TC_TEST_NFS_FILE0 "/vfs0/test/abcd0"
#define TC_TEST_NFS_FILE1 "/vfs0/test/abcd1"

int main(int argc, char *argv[])
{
	void *context = NULL;
	int rc = -1;
	vfile *tcf;

	/* Initialize TC services and daemons */
	context = vinit(get_tc_config_file(tc_config_path, PATH_MAX),
			DEFAULT_LOG_FILE, 77);
	if (context == NULL) {
		NFS4_ERR("Error while initializing tc_client using config "
			 "file: %s; see log at %s",
			 tc_config_path, DEFAULT_LOG_FILE);
		return EIO;
	}

	/* Read the file; nfs4_readv() will open it first if needed. */
	tcf = sca_open(TC_TEST_NFS_FILE0, O_RDWR, 0);
	if (tcf) {
		if (tcf->fd < 0) {
			NFS4_DEBUG("Cannot open %s", TC_TEST_NFS_FILE0);
		}

		rc = sca_close(tcf);
		if (rc < 0) {
			NFS4_DEBUG("Cannot close %d", tcf->fd);
		}
	}
	tcf = sca_open(TC_TEST_NFS_FILE1, O_WRONLY | O_CREAT, 0);
	if (tcf) {
		if (tcf->fd < 0) {
			NFS4_DEBUG("Cannot open %s", TC_TEST_NFS_FILE1);
		}

		rc = sca_close(tcf);
		if (rc < 0) {
			NFS4_DEBUG("Cannot close %d", tcf->fd);
		}
	}

	vdeinit(context);

	return tcf ? 0 : 1;
}
