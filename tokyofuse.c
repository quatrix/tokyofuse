/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall `pkg-config fuse --cflags --libs` fusexmp.c -o fusexmp
*/

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#include <stdlib.h>
#include <pthread.h>
#include "utils.h"
#include "tc.h"
#include "metadata.h"


pthread_rwlock_t tc_iter_lock;


static int xmp_getattr(const char *path, struct stat *stbuf)
{
	if (is_tc(path))
		tc_dir_stat(stbuf);
	else if (is_parent_tc(path)) {
		fprintf(stderr, "%s has tc parent\n", path);
		tc_file_stat(stbuf, meta_filesize(path));
	}
	else if (has_suffix(path, ".tc"))
		tc_dir_stat(stbuf);
	else 
		if (lstat(path, stbuf) == -1)
			return -errno;

	return 0;
}

static int xmp_access(const char *path, int mask)
{
	int res;

	res = access(path, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
	int res;

	res = readlink(path, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}


static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;
	

	if (is_tc(path)) {
		tc_dir_meta_t *tc_dir;
		tc_file_meta_t *tc_file;

		fprintf(stderr, "'%s' is tc\n", path); 
		
		tc_dir = add_path(path);

		if (tc_dir == NULL) {
			fprintf(stderr, "failed to open tc metadata for %s\n", path);
			return -errno;
		}

/*
		if (pthread_rwlock_wrlock(&tc_iter_lock) != 0) {
			fprintf(stderr, "can't get writelock\n");
			return -errno;
		}
*/
		for(tc_file=tc_dir->files; tc_file != NULL; tc_file=tc_file->hh.next) {
			struct stat st;
			tc_file_stat(&st, 5);

			if (filler(buf, tc_file->path, &st, 0))
				break;
		}

//		pthread_rwlock_unlock(&tc_iter_lock);
	
	}
	else {
		dp = opendir(path);
		if (dp == NULL)
			return -errno;

		while ((de = readdir(dp)) != NULL) {
			struct stat st;

			// FIXME unhardcode the suffix
			if (has_suffix(de->d_name, ".tc")) {
				remove_suffix(de->d_name, ".tc");

				fprintf(stderr,"readdir file is tc: %s\n",de->d_name);
				tc_dir_stat(&st);
			}
			else {
				memset(&st, 0, sizeof(st));
				st.st_ino = de->d_ino;
				st.st_mode = de->d_type << 12;
			}

			if (filler(buf, de->d_name, &st, 0))
				break;
		}

		closedir(dp);
	}
	return 0;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(path, mode);
	else
		res = mknod(path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_mkdir(const char *path, mode_t mode)
{
	int res;

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_unlink(const char *path)
{
	int res;

	res = unlink(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rmdir(const char *path)
{
	int res;

	res = rmdir(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_symlink(const char *from, const char *to)
{
	int res;

	res = symlink(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rename(const char *from, const char *to)
{
	int res;

	res = rename(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_link(const char *from, const char *to)
{
	int res;

	res = link(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chmod(const char *path, mode_t mode)
{
	int res;

	res = chmod(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;

	res = lchown(path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_truncate(const char *path, off_t size)
{
	int res;

	res = truncate(path, size);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_utimens(const char *path, const struct timespec ts[2])
{
	int res;
	struct timeval tv[2];

	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;
	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;

	res = utimes(path, tv);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi)
{
	int res;
	char *value;

	if (is_parent_tc(path)) { 
		value = tc_value(path);

		if (value == NULL)
			return -errno;

		fi->fh = (uintptr_t)value;
	}
	else {
		res = open(path, fi->flags);
		if (res == -1)
			return -errno;

		close(res);
	}
	return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res;
	char *value;
	size_t value_len;

	fprintf(stderr, "wants to read %d from %s (offset: %d)\n", size, path, offset);

	if (is_parent_tc(path)) { 
		value = (char *)(uintptr_t)fi->fh;

		if (value == NULL)
			return -errno;

		value += offset;

		value_len = strlen(value);

		if (value_len < size) {
			strncpy(buf, value, value_len);
			res = value_len;
		}
		else {
			strncpy(buf, value, size);
			res = size;
		}
	}
	else {
		(void) fi;
		fd = open(path, O_RDONLY);
		if (fd == -1)
			return -errno;

		res = pread(fd, buf, size, offset);
		if (res == -1)
			res = -errno;

		close(fd);
	}
	return res;
}

static int xmp_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	int fd;
	int res;

	(void) fi;
	fd = open(path, O_WRONLY);
	if (fd == -1)
		return -errno;

	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	close(fd);
	return res;
}

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
	int res;

	res = statvfs(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi)
{
	fprintf(stderr, "releasing %s\n",path);

	(void) path;
	(void) fi;
	char *value = NULL;

	if (is_parent_tc(path)) {
		value = (char *)(uintptr_t)fi->fh;
		
		if (value != NULL)
			free(value);

	}


	return 0;
}


static int xmp_releasedir(const char *path, struct fuse_file_info *fi)
{
	fprintf(stderr, "releasing dir %s\n",path);

	remove_path(path);

	(void) path;
	(void) fi;
	return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
	int res = lsetxattr(path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
	int res = lgetxattr(path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
	int res = llistxattr(path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_removexattr(const char *path, const char *name)
{
	int res = lremovexattr(path, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations xmp_oper = {
	.getattr	= xmp_getattr,
	.access		= xmp_access,
	.readlink	= xmp_readlink,
	.readdir	= xmp_readdir,
	.mknod		= xmp_mknod,
	.mkdir		= xmp_mkdir,
	.symlink	= xmp_symlink,
	.unlink		= xmp_unlink,
	.rmdir		= xmp_rmdir,
	.rename		= xmp_rename,
	.link		= xmp_link,
	.chmod		= xmp_chmod,
	.chown		= xmp_chown,
	.truncate	= xmp_truncate,
	.utimens	= xmp_utimens,
	.open		= xmp_open,
	.read		= xmp_read,
	.write		= xmp_write,
	.statfs		= xmp_statfs,
	.release	= xmp_release,
	.releasedir	= xmp_releasedir,
	.fsync		= xmp_fsync,
#ifdef HAVE_SETXATTR
	.setxattr	= xmp_setxattr,
	.getxattr	= xmp_getxattr,
	.listxattr	= xmp_listxattr,
	.removexattr	= xmp_removexattr,
#endif
};

int main(int argc, char *argv[])
{
	umask(0);

	if (pthread_rwlock_init(&tc_iter_lock,NULL) != 0) {
		fprintf(stderr, "can't init rwlock\n");
		return -errno;
	}

	init_metadata();
	return fuse_main(argc, argv, &xmp_oper, NULL);
}
