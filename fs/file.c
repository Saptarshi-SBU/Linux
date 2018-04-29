/*-------------------------------------------------------------
 *
 * Copyright(C) 2016-2017, Saptarshi Sen
 *
 * LUCI File operations
 *
 * -----------------------------------------------------------*/
#include "luci.h"
#include "kern_feature.h"

#include <linux/fs.h>
#include <linux/uio.h>
#include <linux/types.h>
#include <linux/ktime.h>

loff_t
luci_llseek(struct file * file, loff_t off, int whence) {
   loff_t ret;
   ret = generic_file_llseek(file, off, whence);
   luci_dbg("offset :%llu whence :%d return :%llu", off, whence, ret);
   return ret;
}

#ifdef HAVE_NEW_SYNC_WRITE
ssize_t luci_read(struct file * file, char *buf, size_t size, loff_t *pos) {
   ssize_t ret;
   ktime_t start = ktime_get();
   struct inode *inode = file->f_mapping->host;
   ret = new_sync_read(file, buf, size, pos); // use read_iter not aio_read
   luci_inode_latency(inode, "pos :%llu size :%lu latency(usec) :%llu", *pos,
       size, ktime_us_delta(ktime_get(), start));
   return ret;
}
#endif

#ifdef HAVE_READWRITE_ITER
ssize_t luci_read_iter(struct kiocb*iocb, struct iov_iter *iter) {
   ssize_t ret;
   ret = generic_file_read_iter(iocb, iter);
   luci_dbg("off %llu count %lu size %lu", iocb->ki_pos, iter->count, ret);
   return ret;
}
#endif

#ifdef HAVE_NEW_SYNC_WRITE
ssize_t luci_write(struct file * file, const char *buf, size_t size, loff_t *pos) {
   ssize_t ret;
   ktime_t start = ktime_get();
   struct inode *inode = file->f_mapping->host;
   ret = new_sync_write(file, buf, size, pos); // use write_iter, not aio_write
   luci_inode_latency(inode, "pos :%llu size :%lu latency(usec) :%llu", *pos,
       size, ktime_us_delta(ktime_get(), start));
   return ret;
}
#endif

#ifdef HAVE_READWRITE_ITER
ssize_t luci_write_iter(struct kiocb * iocb, struct iov_iter *iter) {
   ssize_t ret;
   ret = generic_file_write_iter(iocb, iter);
   luci_dbg("off %llu count %lu size %lu", iocb->ki_pos, iter->count, ret);
   return ret;
}
#endif

int luci_open(struct inode * inode, struct file * file) {
   luci_dbg("opening file");
   BUG(); // TBD
   return 0;
}

int luci_iterate(struct file *file, struct dir_context *dir) {
   luci_dbg("iterate");
   BUG(); // TBD
   return 0;
}

int luci_iterate_shared(struct file *file, struct dir_context *dir) {
   luci_dbg("iterate shared");
   BUG(); // TBD
   return 0;
}

int luci_mmap (struct file * file, struct vm_area_struct *vma) {
   luci_dbg("mmap file");
   return generic_file_mmap(file, vma);
}

const struct file_operations luci_file_operations = {
        .llseek         = luci_llseek,
#if defined(HAVE_NEW_SYNC_WRITE)
        .read           = luci_read,
        .write          = luci_write,
#elif defined(HAVE_DO_SYNC_WRITE)
        .read           = do_sync_read,
        .write          = do_sync_write,
#endif

#ifdef HAVE_READWRITE_ITER
        .read_iter      = luci_read_iter,
        .write_iter     = luci_write_iter,
#else
        .aio_read       = generic_file_aio_read,
        .aio_write      = generic_file_aio_write,
#endif
        .mmap           = luci_mmap,
        .fsync          = generic_file_fsync,
        .splice_read    = generic_file_splice_read,
};