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

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <assert.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>

#include "ganesha_list.h"
#include "tc_impl_posix.h"
#include "tc_helper.h"
#include "log.h"
#include "splice_copy.h"

/*
 * open routine for POSIX files
 * path - file path
 * flags - open flags
 */

void* posix_init(const char* config_file, const char* log_file) {
	SetNamePgm("TC-POSIX");
	SetNameFunction("posix");
	SetNameHost("localhost");
	init_logging(log_file, NIV_EVENT);
	return (void*) log_file;
}

vfile *posix_openv(const char **paths, int count, int *flags, mode_t *modes)
{
	vfile *tcfs;
	int fd;
	int i;

	tcfs = calloc(count, sizeof(*tcfs));
	if (tcfs) {
		for (i = 0; i < count; ++i) {
			tcfs[i].type = TC_FILE_DESCRIPTOR;
			if (modes) {
				fd = open(paths[i], flags[i], modes[i]);
			} else {
				fd = open(paths[i], flags[i]);
			}
			tcfs[i].fd = fd >= 0 ? fd : -errno;
		}
	}

	return tcfs;
}

vfile *posix_open(const char *path, int flags, mode_t mode)
{
	return posix_openv(&path, 1, &flags, &mode);
}

vres posix_closev(vfile *tcfs, int count)
{
	int i;
	vres tcres = { .err_no = 0 };

	for (i = 0; i < count; ++i) {
		assert(tcfs[i].type == TC_FILE_DESCRIPTOR);
		/* return error no in case of failure */
		if (close(tcfs[i].fd) < 0) {
			tcres = vfailure(i, errno);
			break;
		} else {
			tcfs[i].fd = INT_MIN;
		}
	}

	if (vokay(tcres)) {
		free(tcfs);
	}

	return tcres;
}

/*
 * close routine for POSIX files
 * file - vfile structure with file
 * descriptor value.
 */
int posix_close(vfile *tcf)
{
	vres tcres;
	tcres = posix_closev(tcf, 1);
	if (vokay(tcres)) {
		free(tcf);
		return 0;
	} else {
		return -tcres.err_no;
	}
}

off_t posix_fseek(vfile *tcf, off_t offset, int whence)
{
	assert(tcf->type == TC_FILE_DESCRIPTOR);
	return lseek(tcf->fd, offset, whence);
}

static int posix_stat(const vfile *tcf, struct stat *st)
{
	int rc;
	if (tcf->type == TC_FILE_PATH) {
		rc = stat(tcf->path, st);
	} else if (tcf->type == TC_FILE_DESCRIPTOR) {
		rc = fstat(tcf->fd, st);
	} else {
		rc = -1;
	}
	return rc;
}

/*
 * arg - Array of reads for one or more files
 *       Contains file-path, read length, offset, etc.
 * read_count - Length of the above array
 *              (Or number of reads)
 */
vres posix_readv(struct viovec *arg, int read_count, bool is_transaction)
{
	int fd, i = 0;
	ssize_t amount_read;
	vfile file = { 0 };
	struct viovec *iov = NULL;
	vres result = { .index = -1, .err_no = 0 };
	struct stat st;

	for (i = 0; i < read_count; ++i) {
		iov = arg + i;
		/*
		 * if the user specified the path and not file descriptor
		 * then call open to obtain the file descriptor else
		 * go ahead with the file descriptor specified by the user
		 */
		if (iov->file.type == TC_FILE_PATH) {
			fd = open(iov->file.path, O_RDONLY);
		} else if (iov->file.type == TC_FILE_DESCRIPTOR) {
			fd = iov->file.fd;
		} else {
			POSIX_ERR("unsupported type: %d", iov->file.type);
		}

		if (fd < 0) {
			result = vfailure(i, errno);
			POSIX_ERR("failed in readv: %s\n", strerror(errno));
			break;
		}

		/* Read data */
		if (iov->offset == TC_OFFSET_CUR) {
			amount_read = read(fd, iov->data, iov->length);
		} else {
			amount_read =
			    pread(fd, iov->data, iov->length, iov->offset);
		}
		if (amount_read < 0) {
			if (iov->file.type == TC_FILE_PATH) {
				close(fd);
			}
			result = vfailure(i, errno);
			break;
		}

		/* set the length to number of bytes successfully read */
		iov->length = amount_read;

		if (fstat(fd, &st) != 0) {
			POSIX_ERR("failed to stat file");
			result = vfailure(i, errno);
			break;
		}

		if (iov->offset == TC_OFFSET_CUR) {
			iov->is_eof = lseek(fd, 0, SEEK_CUR) == st.st_size;
		} else {
			iov->is_eof = (iov->offset + iov->length) == st.st_size;
		}

		if (iov->file.type == TC_FILE_PATH && close(fd) < 0) {
			result = vfailure(i, errno);
			break;
		}
	}

	return result;
}

/*
 * arg - Array of writes for one or more files
 *       Contains file-path, write length, offset, etc.
 * read_count - Length of the above array
 *              (Or number of reads)
 */
vres posix_writev(struct viovec *arg, int write_count, bool is_transaction)
{
	int fd, i = 0;
	ssize_t written = 0;
	struct viovec *iov = NULL;
	vres result = { .index = -1, .err_no = 0 };
	int flags;
	off_t offset;

	for (i = 0; i < write_count; ++i) {
		iov = arg + i;

		/* open the requested file */
		flags = O_WRONLY;
		if (iov->is_creation) {	/* create */
			flags |= O_CREAT;
		}
		if (iov->offset == TC_OFFSET_END) {  /* append */
			flags |= O_APPEND;
		}
		if (iov->is_write_stable) {
			flags |= O_SYNC;
		}

		if (iov->file.type == TC_FILE_PATH) {
			fd = open(iov->file.path, flags);
		} else if (iov->file.type == TC_FILE_DESCRIPTOR) {
			fd = iov->file.fd;
		} else {
			POSIX_ERR("unsupported type: %d", iov->file.type);
		}

		if (fd < 0) {
			result = vfailure(i, errno);
			break;
		}

		offset = iov->offset;
		/* When appending to file we did not open, we need to use lseek
		 * to set the offset to file size. */
		if (offset == TC_OFFSET_END) {
			if (iov->file.type == TC_FILE_PATH) {
				/* Will be ignored because the file was opened
				 * with O_APPEND, but it cannot be
				 * TC_OFFSET_END which is negative when
				 * casted to off_t. */
				offset = 0;
			} else {
				offset = lseek(fd, 0, SEEK_END);
			}
		}

		/* Write data */
		if (iov->length > 0) {
			if (offset == TC_OFFSET_CUR) {
				written = write(fd, iov->data, iov->length);
			} else {
				written =
				    pwrite(fd, iov->data, iov->length, offset);
			}

			if (written < 0) {
				if (iov->file.type == TC_FILE_PATH) {
					close(fd);
				}
				result = vfailure(i, errno);
				break;
			}
		}

		/* set the length to number of bytes successfully written */
		iov->length = written;
		if (iov->file.type == TC_FILE_PATH && close(fd) < 0) {
			result = vfailure(i, errno);
			break;
		}
	}

	return result;
}

/**
 * Get attributes of files
 *
 * @attrs: array of attributes to get
 * @count: the count of vattrs in the preceding array
 * @is_transaction: whether to execute the compound as a transaction
 */

vres posix_lgetattrsv(struct vattrs *attrs, int count, bool is_transaction)
{
	int fd = -1, i = 0, res = 0;
	struct vattrs *cur_attr = NULL;
	vres result = { .index = -1, .err_no = 0 };
	struct stat st;

	while (i < count) {
		cur_attr = attrs + i;

		/* get attributes */
		if (cur_attr->file.type == TC_FILE_PATH)
			res = lstat(cur_attr->file.path, &st);
		else
			res = fstat(cur_attr->file.fd, &st);

		if (res < 0) {
			result = vfailure(i, errno);
			POSIX_DEBUG("posix_lgetattrsv() failed at index : %d\n",
				    result.index);
			break;
		}

		/* copy stat output */
		vstat2attrs(&st, cur_attr);

		i++;
	}

	return result;
}

static int helper_set_attrs(struct vattrs *attrs)
{
	int res = 0;
	struct stat s;
	struct timeval times[2] = {};

	/* check if nlink bit is set, if set return with error */
	if (attrs->masks.has_nlink) {
		POSIX_WARN("set_attrs() failed : nlink attribute bit"
			   " should not be set \n");
		return -1;
	}

	/* check if rdev bit is set, if set return with error */
	if (attrs->masks.has_rdev) {
		POSIX_WARN("set_attrs() failed : rdev attribute bit"
			   " should not be set \n");
		return -1;
	}

	/* set the mode */
	if (attrs->masks.has_mode) {
		if (S_ISLNK(attrs->mode)) {
			POSIX_WARN("set_attrs() failed : cannot chmod symlink\n");
			return -1;
		}
		if (attrs->file.type == TC_FILE_PATH)
			res = chmod(attrs->file.path, attrs->mode);
		else
			res = fchmod(attrs->file.fd, attrs->mode);

		if (res < 0)
			goto exit;
	}

	/* set the file size */
	if (attrs->masks.has_size) {
		if (attrs->file.type == TC_FILE_PATH)
			res = truncate(attrs->file.path, attrs->size);
		else
			res = ftruncate(attrs->file.fd, attrs->size);

		if (res < 0)
			goto exit;
	}

	/* set the UID and GID */
	if (attrs->masks.has_uid || attrs->masks.has_gid) {
		if (attrs->file.type == TC_FILE_PATH)
			res = lchown(attrs->file.path, attrs->uid, attrs->gid);
		else
			res = fchown(attrs->file.fd, attrs->uid, attrs->gid);

		if (res < 0)
			goto exit;
	}

	/* set the atime and mtime */
	if (attrs->masks.has_atime || attrs->masks.has_mtime) {

		if (attrs->file.type == TC_FILE_PATH)
			lstat(attrs->file.path, &s);
		else
			fstat(attrs->file.fd, &s);

		times[0].tv_sec = s.st_atime;
		times[1].tv_sec = s.st_mtime;

		if (attrs->masks.has_atime)
			TIMESPEC_TO_TIMEVAL(&times[0], &attrs->atime);

		if (attrs->masks.has_mtime)
			TIMEVAL_TO_TIMESPEC(&times[1], &attrs->mtime);

		if (attrs->file.type == TC_FILE_PATH)
			res = lutimes(attrs->file.path, times);
		else
			res = futimes(attrs->file.fd, times);

		if (res < 0)
			goto exit;
	}

exit:
	return res;
}

/**
 * Set attributes of files.
 *
 * @attrs: array of attributes to set
 * @count: the count of vattrs in the preceding array
 * @is_transaction: whether to execute the compound as a transaction
 */
vres posix_lsetattrsv(struct vattrs *attrs, int count, bool is_transaction)
{
	int fd = -1, i = 0;
	struct vattrs *cur_attr = NULL;
	vres result = { .index = -1, .err_no = 0 };

	while (i < count) {
		cur_attr = attrs + i;

		/*
		 * Set the attributes if corrseponding mask bit is set
		 */
		if (helper_set_attrs(cur_attr) < 0) {
			result = vfailure(i, errno);
			POSIX_WARN(
			    "posix_lsetattrsv() failed at index : %d (%s)\n",
			    result.index, strerror(errno));
			break;
		}

		i++;
	}

	return result;
}

/*
 * Rename File(s)
 *
 * @pairs[IN] - vfile_pair structure containing the
 * old and new path of the file
 * @count[IN]: the count of vfile_pair in the preceding array
 * @is_transaction[IN]: whether to execute the compound as a transaction
 */

vres posix_renamev(struct vfile_pair *pairs, int count, bool is_transaction)
{
	int i = 0;
	vfile_pair *cur_pair = NULL;
	vres result = { .index = -1, .err_no = 0 };

	while (i < count) {
		cur_pair = pairs + i;

		assert(cur_pair->src_file.type == TC_FILE_PATH &&
		       cur_pair->dst_file.type == TC_FILE_PATH &&
		       cur_pair->src_file.path != NULL &&
		       cur_pair->src_file.path != NULL);

		if (rename(cur_pair->src_file.path, cur_pair->dst_file.path) <
		    0) {
			result = vfailure(i, errno);
			POSIX_WARN("posix_renamev() failed at index %d: %s\n",
				   i, strerror(errno));
			break;
		}

		i++;
	}

	return result;
}

/*
 * Remove File(s)
 *
 * @files[IN] - vfile structure containing the
 * path of the directory to be removed
 * @count[IN]: the count of tc_target_file in the preceding array
 * @is_transaction[IN]: whether to execute the compound as a transaction
 */

vres posix_removev(vfile *files, int count, bool is_transaction)
{
	int i = 0;
	vfile *cur_file = NULL;
	vres result = { .index = -1, .err_no = 0 };

	while (i < count) {
		cur_file = files + i;

		assert(cur_file->path != NULL);

		if (remove(cur_file->path) < 0) {
			result = vfailure(i, errno);
			POSIX_WARN("posix_removev() failed at index %d for "
				   "path %s: %s\n",
				   result.index, cur_file->path,
				   strerror(errno));
			return result;
		}

		i++;
	}

	return result;
}

/*
 * Remove Directory
 *
 * @dir[IN] - vfile structure containing the
 * path of the directory to be removed
 * @count: the count of tc_target_file in the preceding array
 * @is_transaction: whether to execute the compound as a transaction
 */

vres posix_remove_dirv(vfile *dir, int count, bool is_transaction)
{
	int i = 0;
	vfile *cur_dir = NULL;
	vres result = { .index = -1, .err_no = 0 };

	while (i < count) {
		cur_dir = dir + i;

		assert(cur_dir->path != NULL);

		if (rmdir(cur_dir->path) < 0) {
			result = vfailure(i, errno);
			POSIX_WARN("posix_remove_dirv() failed at index : %d\n",
				   result.index);

			return result;
		}

		i++;
	}

	return result;
}

/*
 * Create Directory
 *
 * @dir[IN] - vfile structure containing the
 * path of the directory to be created
 * @count: the count of tc_target_file in the preceding array
 * @is_transaction: whether to execute the compound as a transaction
 */

vres posix_mkdirv(struct vattrs *dirs, int count, bool is_transaction)
{
	int i = 0;
	vfile *cur_dir = NULL;
	vres result = { .index = -1, .err_no = 0 };

	while (i < count) {
		cur_dir = &dirs[i].file;

		assert(cur_dir->path != NULL);

		if (mkdir(cur_dir->path, dirs[i].mode) < 0) {
			result = vfailure(i, errno);
			POSIX_WARN("posix_mkdirv() failed at index : %d (%s)\n",
				   result.index, strerror(errno));
			return result;
		}

		i++;
	}

	return result;
}

struct tc_posix_dir_to_list {
	struct glist_head list;
	const char *path;
	int origin_index;
	bool need_free_path;
};

static inline struct tc_posix_dir_to_list *
enqueue_dir_to_list(struct glist_head *dir_queue, const char *path,
		    bool own_path, int index)
{
	struct tc_posix_dir_to_list *dle;

	dle = malloc(sizeof(*dle));
	if (!dle) {
		return NULL;
	}
	dle->path = path;
	dle->need_free_path = own_path;
	dle->origin_index = index;
	glist_add_tail(dir_queue, &dle->list);

	return dle;
}

static int posix_listdir(struct glist_head *dir_queue, const char *dir,
			 struct vattrs_masks masks, int index, int *limit,
			 bool recursive, vec_listdir_cb cb, void *cbarg)
{
	DIR *dir_fd;
	struct vattrs cur_attr;
	struct dirent *dp;
	int path_len;
	char *path;
	struct stat st;
	struct tc_posix_dir_to_list *dle;
	int ret = 0;

	assert(dir != NULL);

	dir_fd = opendir(dir);
	if (!dir_fd) {
		return -errno;
	}

	while ((dp = readdir(dir_fd)) != NULL && (*limit == -1 || *limit > 0)) {
		POSIX_DEBUG("DirEntry  : %s\n", dp->d_name);
		/* Skip the current and parent directory entry */
		if (!strncmp(dp->d_name, ".", strlen(dp->d_name)) ||
		    !strncmp(dp->d_name, "..", strlen(dp->d_name)))
			continue;

		path_len = strlen(dp->d_name) + strlen(dir) + 2;
		char *path = (char *)malloc(path_len);
		tc_path_join(dir, dp->d_name, path, path_len);

		cur_attr.file = vfile_from_path(path);
		cur_attr.masks = masks;

		if (lstat(path, &st) < 0) {
			POSIX_WARN("stat failed for file : %s/%s", dir,
				   dp->d_name);
			ret = -errno;
			goto exit;
		}

		/* copy the attributes */
		vstat2attrs(&st, &cur_attr);
		if (!cb(&cur_attr, path, cbarg)) {
			ret = 0;
			goto exit;
		}

		if (recursive && S_ISDIR(st.st_mode)) {
			if (!enqueue_dir_to_list(dir_queue, path, true,
						 index)) {
				free(path);
				ret = -ENOMEM;
				goto exit;
			}
		} else {
			free(path);
		}

		if (*limit != -1) {
			--(*limit);
		}
	}

exit:
	closedir(dir_fd);
	return ret;
}

vres posix_listdirv(const char **dirs, int count, struct vattrs_masks masks,
		      int max_entries, bool recursive, vec_listdir_cb cb,
		      void *cbarg, bool istxn)
{
	vres tcres = { .err_no = 0 };
	GLIST_HEAD(dir_queue);
	struct tc_posix_dir_to_list *dle;
	int i;
	int ret = 0;

	if (max_entries == 0) {  /* no limit */
		max_entries = -1;
	}

	for (i = 0; i < count; ++i) {
		enqueue_dir_to_list(&dir_queue, dirs[i], false, i);
	}

	while (!glist_empty(&dir_queue)) {
		dle = glist_first_entry(&dir_queue, struct tc_posix_dir_to_list,
					list);
		if (ret == 0 && (max_entries == -1 || max_entries > 0)) {
			ret = posix_listdir(&dir_queue, dle->path, masks,
					    dle->origin_index, &max_entries,
					    recursive, cb, cbarg);
			if (ret < 0) {
				tcres = vfailure(dle->origin_index, ret);
			}
		}
		if (dle->need_free_path) {
			free((char *)dle->path);
		}
		glist_del(&dle->list);
		free(dle);
	}

	return tcres;
}

vres posix_lcopyv(struct vextent_pair *pairs, int count, bool is_transaction)
{
	int i;
	ssize_t ret;
	vres tcres = { .err_no = 0 };

	for (i = 0; i < count; ++i) {
		ret = splice_copy(pairs[i].src_path, pairs[i].src_offset,
				  pairs[i].dst_path, pairs[i].dst_offset,
				  pairs[i].length);
		if (ret < 0) {
			tcres = vfailure(i, -ret);
			return tcres;
		}
		pairs[i].length = ret;
	}

	return tcres;
}

vres posix_hardlinkv(const char **oldpaths, const char **newpaths, int count,
		       bool istxn)
{
	int i;
	vres tcres = { .err_no = 0 };

	for (i = 0; i < count; ++i) {
		if (link(oldpaths[i], newpaths[i]) < 0) {
			tcres = vfailure(i, errno);
			POSIX_ERR("posix_hardlinkv-%d hardlink %s to %s: %s", i,
				  oldpaths[i], newpaths[i], strerror(errno));
			break;
		}
	}

	return tcres;
}

vres posix_symlinkv(const char **oldpaths, const char **newpaths, int count,
		      bool istxn)
{
	int i;
	vres tcres = { .err_no = 0 };

	for (i = 0; i < count; ++i) {
		if (symlink(oldpaths[i], newpaths[i]) < 0) {
			tcres = vfailure(i, errno);
			POSIX_ERR("posix_symlinkv-%d symlink %s to %s: %s", i,
				  oldpaths[i], newpaths[i], strerror(errno));
			break;
		}
	}

	return tcres;
}

vres posix_readlinkv(const char **paths, char **bufs, size_t *bufsizes,
		       int count, bool istxn)
{
	int i;
	ssize_t sz;
	vres tcres = { .err_no = 0 };

	for (i = 0; i < count; ++i) {
		if ((sz = readlink(paths[i], bufs[i], bufsizes[i])) < 0) {
			tcres = vfailure(i, errno);
			POSIX_ERR("posix_readlinkv-%d readlink at %s: %s",
				  i, paths[i], strerror(errno));
			break;
		}
		if (sz < bufsizes[i]) {
			bufs[i][sz] = '\0';
		}
		bufsizes[i] = sz;
	}

	return tcres;
}

int posix_chdir(const char *path)
{
	int ret;

	ret = chdir(path);
	if (ret == -1) {
		ret = -errno;
	}

	return ret;
}

char *posix_getcwd()
{
	return get_current_dir_name();
}
