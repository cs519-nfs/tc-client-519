/*
 * vim:noexpandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright CEA/DAM/DIF  (2008)
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
 *
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * ---------------------------------------
 */

/**
 * @file nfs_main.c
 * @brief The file that contain the 'main' routine for the nfsd.
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
#include "fsal_types.h"
#include "tc_api.h"
#include "fsal_api.h"
#include "fsal.h"
#include "FSAL/fsal_init.h"
#include "../../util/bench.h"
#include "../nfs4/nfs4_util.h"

/* parameters for NFSd startup and default values */

nfs_start_info_t my_nfs_start_info = {
	.dump_default_config = false,
	.lw_mark_trigger = false
};

config_file_t config_struct;
char *log_path = NULL;
char *exec_name = "nfs-ganesha";
char *host_name = "localhost";
int debug_level = -1;
int detach_flag = false;

int tc_singlefile(char *input_path, unsigned int block_size,
		  unsigned int num_files, unsigned int file_size,
		  int ops_per_comp, double dist, int rw)
{
	struct viovec *user_arg = NULL;
	struct viovec *cur_arg = NULL;
	char *temp_path[MAX_READ_COUNT];
	int input_len = 0;
	int i = 0;
	int j = 0;
	int k = 0;
	// clock_t t;
	float time_taken;
	struct timeval tv1, tv2;

	srand(time(NULL));

	LogDebug(COMPONENT_FSAL, "vinit() success");
	input_len = strlen(input_path);
	k = 0;
	while (k < MAX_READ_COUNT) {
		temp_path[i] = NULL;
		k++;
	}

	user_arg = malloc(ops_per_comp * (sizeof(struct viovec)));
	k = 0;
	while (k < ops_per_comp) {
		cur_arg = user_arg + k;
		cur_arg->data = malloc(block_size);
		temp_path[k] = malloc(input_len + 8);
		k++;
	}

	//t = clock();
	gettimeofday(&tv1, NULL);

	j = 0;
	while (j <= (file_size / block_size)) {

		k = 0;
		while (k < ops_per_comp) {
			cur_arg = user_arg + k;
			cur_arg->file = vfile_from_cfh(".");
			cur_arg->offset = j*block_size + k*block_size;
			cur_arg->length = block_size;
			k++;
		}

		snprintf(temp_path[0], input_len + 8, "%s%d", input_path, 0);
		user_arg->file = vfile_from_path(temp_path[0]);

		if (rw == 0) {
			nfs4_readv(user_arg, ops_per_comp, FALSE, NULL);
		} else {
			nfs4_writev(user_arg, ops_per_comp, FALSE, NULL, NULL);
		}

		j = j + ops_per_comp;
	}

	//t = clock() - t;
	//time_taken = ((double)t)/CLOCKS_PER_SEC;
	//time_taken = (float) t;
	gettimeofday(&tv2, NULL);
	time_taken = ((double)(tv2.tv_usec - tv1.tv_usec) / 1000000) +
		     (double)(tv2.tv_sec - tv1.tv_sec);
	LogFatal(COMPONENT_FSAL, "Took %f seconds\n", time_taken);

	k = 0;
	while (k < ops_per_comp) {
		free(temp_path[i]);
		k++;
	}

	i = 0;
	while (i < ops_per_comp) {
		cur_arg = user_arg + i;
		free(cur_arg->data);
		i++;
	}

	free(user_arg);

	return 0;
}
/* command line syntax */

char options[] = "h@RTdS:F:S:P:f:L:N:E:p:b:n:m:c:l:z:rw";
char usage[] =
	"Usage: %s [-hd][-L <logfile>][-N <dbg_lvl>][-f <config_file>]\n"
	"\t[-h]                display this help\n"
	"\t[-L <logfile>]      set the default logfile for the daemon\n"
	"\t[-N <dbg_lvl>]      set the verbosity level\n"
	"\t[-f <config_file>]  set the config file to be used\n"
	"\t[-p <pid_file>]     set the pid file\n"
	"\t[-d]                the daemon starts in background, in a new process group\n"
	"\t[-R]                daemon will manage RPCSEC_GSS (default is no RPCSEC_GSS)\n"
	"\t[-T]                dump the default configuration on stdout\n"
	"\t[-E] <epoch<]       overrides ServerBootTime for ServerEpoch\n"
	"----------------- Signals ----------------\n"
	"SIGUSR1    : Enable/Disable File Content Cache forced flush\n"
	"SIGTERM    : Cleanly terminate the program\n"
	"------------- Default Values -------------\n"
	"LogFile    : /tmp/nfs-ganesha.log\n"
	"PidFile    : /var/run/ganesha.pid\n"
	"DebugLevel : NIV_EVENT\n" "ConfigFile : /etc/ganesha/ganesha.conf\n";

/**
 * main: simply the main function.
 *
 * The 'main' function as in every C program.
 *
 * @param argc number of arguments
 * @param argv array of arguments
 *
 * @return status to calling program by calling the exit(3C) function.
 *
 */

int main(int argc, char *argv[])
{
	char *tempo_exec_name = NULL;
	char localmachine[MAXHOSTNAMELEN + 1];
	int block_size = 0;
	char *input_path = NULL;
	int input_len = 0;
	unsigned int num_files = 0;
	unsigned int ops_per_comp = 0;
	double dist = 0.0;
	unsigned int file_size = 0;
	int rw = 0;
	int c, rc;
	int pidfile;
#ifndef HAVE_DAEMON
	int dev_null_fd = 0;
	pid_t son_pid;
#endif
	sigset_t signals_to_block;
	struct config_error_type err_type;

	/* Set the server's boot time and epoch */
	now(&ServerBootTime);
	ServerEpoch = (time_t) ServerBootTime.tv_sec;

	tempo_exec_name = strrchr(argv[0], '/');
	if (tempo_exec_name != NULL) {
		exec_name = gsh_strdup(tempo_exec_name + 1);
		if (!exec_name) {
			fprintf(stderr,
				"Unable to allocate memory for exec name, exiting...\n");
			exit(1);
		}
	}

	if (*exec_name == '\0')
		exec_name = argv[0];

	/* get host name */
	if (gethostname(localmachine, sizeof(localmachine)) != 0) {
		fprintf(stderr, "Could not get local host name, exiting...\n");
		exit(1);
	} else {
		host_name = gsh_strdup(localmachine);
		if (!host_name) {
			fprintf(stderr,
				"Unable to allocate memory for hostname, exiting...\n");
			exit(1);
		}
	}

	/* now parsing options with getopt */
	while ((c = getopt(argc, argv, options)) != EOF) {
		switch (c) {
		case '@':
			/* A litlle backdoor to keep track of binary versions */
			printf("%s compiled on %s at %s\n", exec_name, __DATE__,
			       __TIME__);
			printf("Release = %s\n", VERSION);
			printf("Release comment = %s\n", VERSION_COMMENT);
			printf("Git HEAD = %s\n", _GIT_HEAD_COMMIT);
			printf("Git Describe = %s\n", _GIT_DESCRIBE);
			exit(0);
			break;

		case 'L':
			/* Default Log */
			log_path = gsh_strdup(optarg);
			if (!log_path) {
				fprintf(stderr,
					"Unable to allocate memory for log path.\n");
				exit(1);
			}
			break;

		case 'N':
			/* debug level */
			debug_level = ReturnLevelAscii(optarg);
			if (debug_level == -1) {
				fprintf(stderr,
					"Invalid value for option 'N': NIV_NULL, NIV_MAJ, NIV_CRIT, NIV_EVENT, NIV_DEBUG, NIV_MID_DEBUG or NIV_FULL_DEBUG expected.\n");
				exit(1);
			}
			break;

		case 'f':
			/* config file */

			config_path = gsh_strdup(optarg);
			if (!config_path) {
				fprintf(stderr,
					"Unable to allocate memory for config path.\n");
				exit(1);
			}
			break;

		case 'p':
			/* PID file */
			pidfile_path = gsh_strdup(optarg);
			if (!pidfile_path) {
				fprintf(stderr,
					"Path %s too long for option 'f'.\n",
					optarg);
				exit(1);
			}
			break;

		case 'd':
			/* Detach or not detach ? */
			detach_flag = true;
			break;

		case 'R':
			/* Shall we manage  RPCSEC_GSS ? */
			fprintf(stderr,
				"\n\nThe -R flag is deprecated, use this syntax in the configuration file instead:\n\n");
			fprintf(stderr, "NFS_KRB5\n");
			fprintf(stderr, "{\n");
			fprintf(stderr,
				"\tPrincipalName = nfs@<your_host> ;\n");
			fprintf(stderr, "\tKeytabPath = /etc/krb5.keytab ;\n");
			fprintf(stderr, "\tActive_krb5 = true ;\n");
			fprintf(stderr, "}\n\n\n");
			exit(1);
			break;

		case 'T':
			/* Dump the default configuration on stdout */
			my_nfs_start_info.dump_default_config = true;
			break;

		case 'E':
			ServerEpoch = (time_t) atoll(optarg);
			break;

		case 'b':
			/* Block size per read/write */

			block_size = atoi((char *)optarg);

			if (block_size <= 0 || block_size > 1024 * 1024) {
				printf(
				    "Invalid block size or it exceeds 1024k\n");
				exit(-1);
			}

			break;
		case 'n':
			/*
			 * Total number of files, each file of the form
			 * /mnt/test/abcd0, /mnt/test/abcd1, .....
			 * if the path is specified as "/mnt/test/abcd"
			 */

			num_files = atoi((char *)optarg);

			if (num_files <= 0 || num_files > 10000) {
				printf(
				    "Number of files exceeds 200 or invalid\n");
				exit(-1);
			}

			break;

		case 'm':
			/*
			 * Total number of reads/writes
			 */

			file_size = atoi((char *)optarg);

			if (file_size <= 0) {
				printf("Invalid total number of reads/writes "
				       "or it exceeds 10000\n");
				exit(-1);
			}

			break;
		case 'c':
			/*
			 * Number of operations in a single compound, should not
			 * matter in normal reads
			 */

			ops_per_comp = atoi((char *)optarg);

			if (ops_per_comp <= 0 || ops_per_comp > 10) {
				printf(
				    "Invalid ops per comp or it exceeds 10\n");
				exit(-1);
			}

			break;

		case 'l':
			/*
			 * Path of the files
			 *
			 * If the files are of the form -
			 * /mnt/test/abcd0, /mnt/test/abcd1, ......
			 * Path should be "/mnt/test/abcd"
			 */

			input_path = (char *)optarg;
			input_len = strlen(input_path);
			if (input_len <= 0 || input_len > 50) {
				printf("Name is invalid or is too long, max 50 "
				       "chars \n");
				exit(-1);
			}

			break;
		case 'z':
			/* 
 			 * Distribution parameter
 			 */

			dist = atof((char *)optarg);

			if (dist < 0.0 || dist > 1.0) {
				printf("Invalid distribution parameter "
				       "specified\n");
				exit(-1);
			}
			break;
		case 'r':
			/* Read */

			rw = 0;

			break;
		case 'w':
			/* Write */

			rw = 1;

			break;

		case '?':
		case 'h':
		default:
			/* display the help */
			fprintf(stderr, usage, exec_name);
			exit(0);
			break;
		}
	}

	/* initialize memory and logging */
	nfs_prereq_init(exec_name, host_name, debug_level, log_path);
	LogEvent(COMPONENT_MAIN,
		 "%s Starting: Version %s, built at %s %s on %s",
		 exec_name, GANESHA_VERSION, __DATE__, __TIME__, BUILD_HOST);

	/* Start in background, if wanted */
	if (detach_flag) {
#ifdef HAVE_DAEMON
		/* daemonize the process (fork, close xterm fds,
		 * detach from parent process) */
		if (daemon(0, 0))
			LogFatal(COMPONENT_MAIN,
				 "Error detaching process from parent: %s",
				 strerror(errno));

		/* In the child process, change the log header
		 * if not, the header will contain the parent's pid */
		set_const_log_str();
#else
		/* Step 1: forking a service process */
		switch (son_pid = fork()) {
		case -1:
			/* Fork failed */
			LogFatal(COMPONENT_MAIN,
				 "Could not start nfs daemon (fork error %d (%s)",
				 errno, strerror(errno));
			break;

		case 0:
			/* This code is within the son (that will actually work)
			 * Let's make it the leader of its group of process */
			if (setsid() == -1) {
				LogFatal(COMPONENT_MAIN,
					 "Could not start nfs daemon (setsid error %d (%s)",
					 errno, strerror(errno));
			}

			/* stdin, stdout and stderr should not refer to a tty
			 * I close 0, 1 & 2  and redirect them to /dev/null */
			dev_null_fd = open("/dev/null", O_RDWR);
			if (dev_null_fd < 0)
				LogFatal(COMPONENT_MAIN,
					 "Could not open /dev/null: %d (%s)",
					 errno, strerror(errno));

			if (close(STDIN_FILENO) == -1)
				LogEvent(COMPONENT_MAIN,
					 "Error while closing stdin: %d (%s)",
					  errno, strerror(errno));
			else {
				LogEvent(COMPONENT_MAIN, "stdin closed");
				dup(dev_null_fd);
			}

			if (close(STDOUT_FILENO) == -1)
				LogEvent(COMPONENT_MAIN,
					 "Error while closing stdout: %d (%s)",
					  errno, strerror(errno));
			else {
				LogEvent(COMPONENT_MAIN, "stdout closed");
				dup(dev_null_fd);
			}

			if (close(STDERR_FILENO) == -1)
				LogEvent(COMPONENT_MAIN,
					 "Error while closing stderr: %d (%s)",
					  errno, strerror(errno));
			else {
				LogEvent(COMPONENT_MAIN, "stderr closed");
				dup(dev_null_fd);
			}

			if (close(dev_null_fd) == -1)
				LogFatal(COMPONENT_MAIN,
					 "Could not close tmp fd to /dev/null: %d (%s)",
					 errno, strerror(errno));

			/* In the child process, change the log header
			 * if not, the header will contain the parent's pid */
			set_const_log_str();
			break;

		default:
			/* This code is within the parent process,
			 * it is useless, it must die */
			LogFullDebug(COMPONENT_MAIN,
				     "Starting a child of pid %d",
				     son_pid);
			exit(0);
			break;
		}
#endif
	}

	/* Make sure Linux file i/o will return with error
	 * if file size is exceeded. */
#ifdef _LINUX
	signal(SIGXFSZ, SIG_IGN);
#endif

	/* Echo PID into pidfile */
	pidfile = open(pidfile_path, O_CREAT | O_RDWR, 0644);
	if (pidfile == -1) {
		LogFatal(COMPONENT_MAIN, "Can't open pid file %s for writing",
			 pidfile_path);
	} else {
		char linebuf[1024];
		struct flock lk;

		/* Try to obtain a lock on the file */
		lk.l_type = F_WRLCK;
		lk.l_whence = SEEK_SET;
		lk.l_start = (off_t) 0;
		lk.l_len = (off_t) 0;
		if (fcntl(pidfile, F_SETLK, &lk) == -1)
			LogFatal(COMPONENT_MAIN, "Ganesha already started");

		/* Put pid into file, then close it */
		(void)snprintf(linebuf, sizeof(linebuf), "%u\n", getpid());
		if (write(pidfile, linebuf, strlen(linebuf)) == -1)
			LogCrit(COMPONENT_MAIN, "Couldn't write pid to file %s",
				pidfile_path);
	}

	/* Set up for the signal handler.
	 * Blocks the signals the signal handler will handle.
	 */
	sigemptyset(&signals_to_block);
	sigaddset(&signals_to_block, SIGTERM);
	sigaddset(&signals_to_block, SIGHUP);
	sigaddset(&signals_to_block, SIGPIPE);
	if (pthread_sigmask(SIG_BLOCK, &signals_to_block, NULL) != 0)
		LogFatal(COMPONENT_MAIN,
			 "Could not start nfs daemon, pthread_sigmask failed");

	/* Parse the configuration file so we all know what is going on. */

	if (config_path == NULL) {
		LogFatal(COMPONENT_INIT,
			 "start_fsals: No configuration file named.");
		return 1;
	}
	config_struct = config_ParseFile(config_path, &err_type);

	if (!config_error_no_error(&err_type)) {
		char *errstr = err_type_str(&err_type);

		if (!config_error_is_harmless(&err_type))
			LogFatal(COMPONENT_INIT,
				 "Fatal error while parsing %s because of %s errors",
				 config_path,
				 errstr != NULL ? errstr : "unknown");
			/* NOT REACHED */
		LogCrit(COMPONENT_INIT,
			"Minor parse errors found %s in %s",
			errstr != NULL ? errstr : "unknown", config_path);
		if (errstr != NULL)
			gsh_free(errstr);
	}

	if (read_log_config(config_struct) < 0)
		LogFatal(COMPONENT_INIT,
			 "Error while parsing log configuration");
	/* We need all the fsal modules loaded so we can have
	 * the list available at exports parsing time.
	 */
	start_fsals();

	/* parse configuration file */

	if (nfs_set_param_from_conf(config_struct, &my_nfs_start_info)) {
		LogFatal(COMPONENT_INIT,
			 "Error setting parameters from configuration file.");
	}

	/* initialize core subsystems and data structures */
	if (init_server_pkgs() != 0)
		LogFatal(COMPONENT_INIT,
			 "Failed to initialize server packages");

	/* Load export entries from parsed file
	 * returns the number of export entries.
	 */
	rc = ReadExports(config_struct);
	if (rc < 0)
		LogFatal(COMPONENT_INIT,
			  "Error while parsing export entries");
	else if (rc == 0)
		LogWarn(COMPONENT_INIT,
			"No export entries found in configuration file !!!");

	/* freeing syntax tree : */

	config_Free(config_struct);

	/* Everything seems to be OK! We can now start service threads */
	//nfs_start(&my_nfs_start_info);
	tc_singlefile(input_path, block_size, num_files, file_size, ops_per_comp,
		      dist, rw);

	return 0;

}
