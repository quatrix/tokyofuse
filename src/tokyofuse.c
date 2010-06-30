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
#include "tc_gc.h"
#include "tc.h"
#include "metadata.h"


#define TC_GC_SLEEP 5 // 5 seconds between every gc run
//#define CACHEGRIND 99999


static inline tc_dir_meta_t *tokyofuse_get_tc_dir(struct fuse_file_info *fi)
{
	return (tc_dir_meta_t *)(uintptr_t)fi->fh;
}

static inline tc_filehandle_t *tokyofuse_get_tc_fh(struct fuse_file_info *fi)
{
	return (tc_filehandle_t *)(uintptr_t)fi->fh;
}

static int xmp_getattr(const char *path, struct stat *stbuf)
{
#ifdef CACHEGRIND
	static int i = 0;

	if (i % 1000 == 0)
		error("request number %d", i);
	

	if (++i == CACHEGRIND)
		exit(0);
#endif


	size_t path_len = strlen(path);
//	double t0, td;
	

//	t0 = get_time();

	if (is_parent_tc(path, path_len)) {
		debug("%s has tc parent", path);

/* 
		td = (get_time() - t0) * 1000;
		if (td > 100) 
			error("getattr (is_parent_tc) took: %0.3f msec", td);
		t0 = get_time();
*/

		tc_file_stat(stbuf, metadata_get_filesize(path, path_len));
/* 
		td = (get_time() - t0) * 1000;
		if (td > 100) 
			error("getattr (metadata_get_filesize) took: %0.3f msec", td);
		t0 = get_time();
*/
	}
	else if (is_tc(path, path_len)) {
		tc_dir_stat(stbuf);
/*  
		td = (get_time() - t0) * 1000;
		if (td > 100) 
			error("getattr (is_tc) took: %0.3f msec", td);
		t0 = get_time();
*/
	}
	else if (has_suffix(path, ".tc")) {
		tc_dir_stat(stbuf);
/* 
		td = (get_time() - t0) * 1000;
		if (td > 100) 
			error("getattr (has_suffix) took: %0.3f msec", td);
		t0 = get_time();
*/
	}
	else 
		if (lstat(path, stbuf) == -1)
			return -errno;


	return 0;
}

static int xmp_access(const char *path, int mask)
{
	debug("access called on %s", path);

	size_t path_len = strlen(path);

	if (is_tc(path, path_len))
		debug("access %s has a tc parent", path);
	else if (access(path, mask) == -1)
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


static int xmp_opendir(const char *path, struct fuse_file_info *fi)
{
	size_t path_len = strlen(path);

	if (is_tc(path, path_len)) {
		tc_dir_meta_t *tc_dir = NULL;

		debug("'%s' is tc", path); 
		
		debug("hey");
		tc_dir = metadata_get_tc(path, path_len);

		debug("ho");

		if (tc_dir == NULL) {
			debug("failed to open tc metadata for %s", path);
			return -errno;
		}

		fi->fh = (uintptr_t)tc_dir;
		return 0;
	}


	return 0;
}


static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;

	(void) offset;

	size_t path_len = strlen(path);

	if (is_tc(path, path_len)) {
		tc_dir_meta_t *tc_dir = NULL;
		tc_file_meta_t *tc_file = NULL;

		tc_dir = tokyofuse_get_tc_dir(fi);

		if (tc_dir == NULL)
			return -1;

		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
/* 
		for(tc_file=tc_dir->files; tc_file != NULL; tc_file=tc_file->hh.next) {
			struct stat st;
			tc_file_stat(&st, tc_file->size);

			if (filler(buf, tc_file->path, &st, 0))
				break;
		}
*/

		fprintf(stderr, "DEBUG :: in readdir!!\n");

		for (tc_file=get_next_tc_file(tc_dir, tc_file); tc_file != NULL; tc_file = get_next_tc_file(tc_dir, tc_file)) {
			struct stat st;
			tc_file_stat(&st, tc_file->size);

			if (filler(buf, tc_file->path, &st, 0))
				break;
		}

		//tc_dir_unlock(tc_dir);

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

				debug("readdir file is tc: %s",de->d_name);
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

	debug("wants to open %s", path);

	size_t path_len = strlen(path);
//	double t0, td;
	


	//t0 = get_time();

	if (is_parent_tc(path, path_len)) { 
		debug("%s has a tc parent", path);

		tc_filehandle_t *fh	= (tc_filehandle_t *)malloc(sizeof(tc_filehandle_t));

		if (fh == NULL) {
			debug("can't allocate memory for tc_filehandle");
			return -errno;
		}

		//fh->value = metadata_get_value(path, &fh->value_len);

		if (!metadata_get_value(path, path_len, fh)) {
			debug("metadata_get_value failed");
			free(fh);
			return -errno;
		}


		fi->fh = (uintptr_t)fh;
	}
	else {
		res = open(path, fi->flags);
		if (res == -1)
			return -errno;

		close(res);
	}
/* 
	td = (get_time() - t0) * 1000;

	if (td > 100) 
		error("xmp_open took: %0.3f msec", td);
*/
	return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res;
	char *value;
	size_t value_len;
	tc_filehandle_t *fh;	

	debug("wants to read %d bytes from %s", size, path);
	//debug("wants to read %d from %s (offset: %d)", size, path, offset);

	fh = tokyofuse_get_tc_fh(fi);

	if (fh != NULL) {
		if (fh->value == NULL)
			return -errno;

		value = fh->value + offset;
		value_len = fh->value_len - offset;

		debug("value_len: %d", value_len);

		if (value_len <= size) {
			debug("request bigger or equals to value_len (: %d) [writing: %d | requested: %d]", fh->value_len, fh->value_len, size);
			memcpy(buf, value, value_len);
			res = value_len;
		}
		else {
			debug("requested smaller than value_len (: %d) [writing: %d | requested: %d]", fh->value_len, size, size);
			memcpy(buf, value, size);
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
	debug("releasing %s",path);
	(void) path;
	(void) fi;
	tc_filehandle_t *fh;	

	fh = tokyofuse_get_tc_fh(fi);

	if (fh != NULL) {
		if (fh->value != NULL)
			free(fh->value);

		metadata_release_path(fh->tc_dir);

		free(fh);
		
		fi->fh = (uintptr_t)NULL;
	}

	return 0;
}


static int xmp_releasedir(const char *path, struct fuse_file_info *fi)
{
	
	debug("releasing dir %s",path);

	tc_dir_meta_t *tc_dir = NULL;

	tc_dir = tokyofuse_get_tc_dir(fi);
	
	metadata_release_path(tc_dir);

	fi->fh = (uintptr_t)NULL;

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

static void *xmp_init(struct fuse_conn_info *conn)
{
	if (!tc_gc_init(TC_GC_SLEEP)) {
		debug("xmp_init: failed to init garbage collector");
		return (void *)-1;
	}

	return (void *)0;
}

static void xmp_destroy(void *userdata) 
{
	if (!tc_gc_destroy())
		debug("xmp_destroy: failed to destroy garbage collector");
}


static struct fuse_operations xmp_oper = {
	.init		= xmp_init, 
	.destroy	= xmp_destroy, 
	.getattr	= xmp_getattr,
	.access		= xmp_access,
	.readlink	= xmp_readlink,
	.opendir	= xmp_opendir,
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

	if (!metadata_init())
		return 1;

	return fuse_main(argc, argv, &xmp_oper, NULL);
}
