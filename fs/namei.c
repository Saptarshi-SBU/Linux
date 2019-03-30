/*--------------------------------------------------------------------
 * Copyright(C) 2016, Saptarshi Sen
 *
 * LUCI inode operaitons
 *
 * ------------------------------------------------------------------*/
#include <linux/fs.h>
#include <linux/mpage.h>
#include <linux/ktime.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/dcache.h>
#include <linux/version.h>
#include <linux/writeback.h>
#include <linux/buffer_head.h>

#include "kern_feature.h"
#include "luci.h"

// TBD
static inline void
luci_set_de_type(struct luci_dir_entry_2 *de, struct inode *inode)
{
    de->file_type  = 0;
}

static inline int
luci_match (int len, const char * const name,
        struct luci_dir_entry_2 * de)
{
    if ((len != de->name_len) || (!de->inode)) {
        return 0;
    }
    return !memcmp(name, de->name, len);
}

int
luci_prepare_chunk(struct page *page, loff_t pos, unsigned len)
{
    luci_dbg_inode(page->mapping->host, "pos :%llu len :%u", pos, len);
    return __block_write_begin(page,
                               pos,
                               len,
                               luci_get_block);
}

// is this specific to directory usage ?
int
luci_commit_chunk(struct page *page, loff_t pos, unsigned len)
{
    int err = 0;
    struct address_space *mapping = page->mapping;
    struct inode *dir = mapping->host;

    BUG_ON(!page_has_buffers(page));

    luci_dbg_inode(dir,"pos :%llu len :%u err :%d", pos, len, err);

    dir->i_version++;
    if (block_write_end(NULL,
                        mapping,
                        pos,
                        len,
                        len,
                        page, NULL) != len)
        BUG();

    if (pos + len > dir->i_size) {
        i_size_write(dir, pos + len);
        mark_inode_dirty(dir);
        luci_dbg_inode(dir, "updating dir inode size %llu", dir->i_size);
    }

    if (IS_DIRSYNC(dir) &&
#ifdef HAVE_WRITE_ONE_PAGE_NEW
		    ((err = write_one_page(page)) == 0))
#else
		    ((err = write_one_page(page, 1)) == 0))
#endif
        err = sync_inode_metadata(dir, 1);
    else
        unlock_page(page);

    return err;
}

int
luci_make_empty(struct inode *inode, struct inode *parent)
{
    int err;
    void *kaddr;
    struct page *page;
    unsigned chunk_size;
    struct luci_dir_entry_2 *de;

    page = grab_cache_page(inode->i_mapping, 0);
    if (!page)
        return -ENOMEM;

    chunk_size = luci_chunk_size(inode);

    err = luci_prepare_chunk(page, 0, chunk_size);
    if (err) {
        unlock_page(page);
        luci_err("failed to pepare chunk");
        goto fail;
    }

    kaddr = kmap_atomic(page);
    memset(kaddr, 0, chunk_size);

    // . entry
    de = (struct luci_dir_entry_2*)kaddr;
    de->name_len = 1;
    de->rec_len = luci_rec_len_to_disk(LUCI_DIR_REC_LEN(1));
    memcpy(de->name, ".\0\0", 4);
    de->inode = cpu_to_le32(inode->i_ino);
    luci_set_de_type(de, inode);

    // .. entry
    de = (struct luci_dir_entry_2*)(kaddr + LUCI_DIR_REC_LEN(1));
    de->name_len = 2;
    de->rec_len = luci_rec_len_to_disk(chunk_size - LUCI_DIR_REC_LEN(1));
    memcpy(de->name, "..\0", 4);
    de->inode = cpu_to_le32(parent->i_ino);
    luci_set_de_type(de, inode);

    // FIXED : do atomic, otherwize segfault in user land
    kunmap_atomic(kaddr);
    err = luci_commit_chunk(page, 0, chunk_size);
fail:
    put_page(page);
    return err;
}

static void
luci_track_size(struct inode * inode)
{
   loff_t size = inode->i_blocks * 512;
   BUG_ON(size < inode->i_size);
   luci_dbg_inode(inode, "size :%llu phy size :%llu blocks :%lu",
      inode->i_size, size, inode->i_blocks);
}

static int
luci_add_link(struct dentry *dentry, struct inode *inode) {
    loff_t pos;  // offset in page with empty dentry
    struct inode *dir;
    int err, rec_len = 0, new_dentry_len;
    struct page *page = NULL;
    unsigned long n, npages;
    struct luci_dir_entry_2 *de = NULL;  //dentry iterator
    unsigned chunk_size = luci_chunk_size(inode);

    BUG_ON(inode->i_ino == 0); // sanity check for new inode

    dir = DENTRY_INODE(dentry->d_parent);
    npages = dir_pages(dir);

    new_dentry_len = LUCI_DIR_REC_LEN(dentry->d_name.len);
    luci_dbg("dentry add, inode :%lu (%s) npages :%lu len :%d",
              dir->i_ino,
              dentry->d_name.name,
              npages,
              new_dentry_len);

    for (n = 0; n < npages; n++) {
        char *kaddr, *page_boundary;

        page = luci_get_page(dir, n);
        if (IS_ERR(page)) {
            err = PTR_ERR(page);
            luci_err_inode(inode, "error dentry page %lu :%d", n, err);
            return err;
        }

        lock_page(page);
        kaddr = page_address(page);
        page_boundary = kaddr + PAGE_SIZE - new_dentry_len; // dentry not cross page boundary
        de = (struct luci_dir_entry_2*)((char*)kaddr);
        while ((char*)de <= page_boundary) {
	    // dentry rolls over to next block, terminal dentry in this block
            if (de->rec_len == 0) {
                de->inode = 0;
                de->rec_len = luci_rec_len_to_disk(chunk_size);
                goto gotit;
            }

            // entry already exists
            if (luci_match(dentry->d_name.len, dentry->d_name.name, de)) {
                err = -EEXIST;
                luci_err("failed to add link, file exists %s",
		   dentry->d_name.name);
                goto outunlock;
            }

            // offset to next valid dentry from current de
            rec_len = luci_rec_len_from_disk(de->rec_len);
            luci_dbg("dname :%s inode :%u next_len :%u",
                      de->name,
                      de->inode,
                      rec_len);

	    // if new dentry record can be acommodated in this block
            if (!de->inode && rec_len >= new_dentry_len)
               goto gotit;

            if (rec_len >= (LUCI_DIR_REC_LEN(de->name_len) +
               LUCI_DIR_REC_LEN(dentry->d_name.len)))
               goto gotit;

            de = (struct luci_dir_entry_2*)((char*)de + rec_len);
        }

        unlock_page(page);
        luci_put_page(page);
        luci_dbg("dentry page %ld nr_pages :%ld ", n, npages);
    }

    // extend the directory to accomodate new dentry
    page = luci_get_page(dir, n);
    if (IS_ERR(page)) {
       err = -ENOSPC;
       luci_err_inode(inode, "failed to adding new link entry, no space");
       luci_err_inode(inode, "error dentry page %lu :%ld", n, PTR_ERR(page));
       return err;
    }

    lock_page(page);
    de = (struct luci_dir_entry_2*) page_address(page);
    de->inode = 0;
    de->rec_len = luci_rec_len_to_disk(chunk_size);
    luci_info_inode(dir, "allocated page %lu(%p) for new dentry", n, (char*)de);
    goto gotit;

outunlock:
    BUG_ON(page == NULL);
    unlock_page(page);
    luci_put_page(page);
    return err;

gotit:
    luci_dbg("luci: empty dentry found, adding new link entry");
    // Previous entry have to be modified
    if (de->inode) {
        struct luci_dir_entry_2 * de_new = (struct luci_dir_entry_2*)
	   ((char*) de + LUCI_DIR_REC_LEN(de->name_len));
	de_new->inode = inode->i_ino;
        de->rec_len = luci_rec_len_to_disk(LUCI_DIR_REC_LEN(de->name_len));
        de_new->rec_len = luci_rec_len_to_disk(rec_len - de->rec_len);
        de = de_new;
    }

    pos = page_offset(page) + (char*)de - (char*)page_address(page);
    err = luci_prepare_chunk(page, pos, new_dentry_len);
    if (err) {
        luci_err("error to prepare chunk during dentry insert");
        goto outunlock;
    }

    de->name_len = dentry->d_name.len;
    memcpy(de->name, dentry->d_name.name, de->name_len);
    de->inode = cpu_to_le32(inode->i_ino);
    luci_set_de_type(de, inode);
    err = luci_commit_chunk(page, pos, new_dentry_len);
    if (err) {
        luci_err("error to commit chunk during dentry insert");
        BUG();
    }

    dir->i_mtime = dir->i_ctime = LUCI_CURR_TIME;
    mark_inode_dirty(dir);

    luci_info_inode(dir, "sucessfully inserted dentry %s rec_len :%d "
       "next_rec :%d page :%lu pos :%llu size :%llu va :%p",
       dentry->d_name.name,
       LUCI_DIR_REC_LEN(de->name_len),
       de->rec_len,
       n,
       pos,
       dir->i_size,
       page_address(page));

    luci_put_page(page);
    return err;
}

static int
luci_create(struct inode *dir,
            struct dentry *dentry,
            umode_t mode,
            bool excl)
{
    int err;
    struct inode * inode;

    // create inode
    inode = luci_new_inode(dir, mode, &dentry->d_name);
    if (IS_ERR(inode)) {
        luci_err("failed to create new inode");
        return PTR_ERR(inode);
    }
    luci_dbg_inode(inode, "created new inode name :%s", dentry->d_name.name);

    inode->i_op = &luci_file_inode_operations;
    inode->i_fop = &luci_file_operations;
    inode->i_mapping ->a_ops = &luci_aops;
    mark_inode_dirty(inode);

    err = luci_add_link(dentry, inode);
    if (err) {
       inode_dec_link_count(inode);
       unlock_new_inode(inode);
       iput(inode);
       luci_err("inode add link failed, err :%d", err);
       return err;
    }
    luci_track_size(dir);
    unlock_new_inode(inode);
    d_instantiate(dentry, inode);
    return 0;
}

/*
 * core function to lookup dentries
 */
struct luci_dir_entry_2 *
luci_find_entry (struct inode * dir, const struct qstr * child,
                 struct page ** res) {
    struct page *page = NULL;
    struct luci_dir_entry_2 *de = NULL;
    unsigned long n, npages = dir_pages(dir);

    for (n = 0; n < npages; n++) {
        struct luci_dir_entry_2 *kaddr, *limit;
        //lookup dentry page
        page = luci_get_page(dir, n);
        if (IS_ERR(page)) {
            luci_err_inode(dir, "bad dentry page page :%ld err:%ld",
               n, PTR_ERR(page));
	    goto fail;
	}
        kaddr = (struct luci_dir_entry_2*) page_address(page);
	// limit takes care of page boundary issues
        limit = (struct luci_dir_entry_2*) ((char*) kaddr +
            luci_last_byte(dir, n) - LUCI_DIR_REC_LEN(child->len));
        // scan dentries
        for (de = kaddr; de <= limit; de = luci_next_entry(de)) {
            if (de->rec_len == 0) {
	        // check page boundary
                // Fixed an issue, where newly created dentry block was alloted
                // to an incorrect index due to a bug in alloc branch
                luci_err_inode(dir, "invalid zero record length found at page "
                    "%lu(%p-%p)", n, (char*)de, (char*)limit);
	        luci_put_page(page);
	        goto fail;
            }

	    if (luci_match(child->len, child->name, de)) {
                luci_dbg("dentry found %s", child->name);
                goto found;
            }

#           ifdef DEBUG_DENTRY
            luci_dbg("dentry name :%s, inode :%u, namelen :%u reclen :%u",
	        de->name, de->inode, de->name_len,
                luci_rec_len_from_disk(de->rec_len));
#           endif
        }
        luci_put_page(page);
    }
fail:
    return NULL;
found:
    *res = page;
    return de;
}

ino_t
luci_inode_by_name(struct inode *dir, const struct qstr *child)
{
    ino_t res = 0;
    struct luci_dir_entry_2 *de;
    struct page *page;
    de = luci_find_entry (dir, child, &page);
    if (de) {
        res = le32_to_cpu(de->inode);
        luci_put_page(page);
    }
    return res;
}

static int
luci_empty_dir(struct inode *dir)
{
    unsigned long n;
    unsigned long npages = dir_pages(dir);

    for (n = 0; n < npages; n++) {
        char *kaddr;
        struct page *page;
        struct luci_dir_entry_2 *de, *limit;

        page = luci_get_page(dir, n);
        if (IS_ERR(page)) {
            luci_err_inode(dir, "bad dentry page page :%ld err:%ld", n,
               PTR_ERR(page));
            return -PTR_ERR(page);
        }

        kaddr = page_address(page);
        de = (struct luci_dir_entry_2*)(kaddr);
        limit = (struct luci_dir_entry_2*)
	    ((char*)kaddr + luci_last_byte(dir, n) - LUCI_DIR_REC_LEN(1));

        for (; de <= limit; de = luci_next_entry(de)) {

            if (de->rec_len == 0) {
                luci_err("invalid dir rec length at %p", (char*)de);
	        luci_put_page(page);
                return -EIO;
            }

            if (de->inode) {
                if (de->name[0] != '.') {
		    goto not_empty;
	        }
	        if (de->name_len > 1) {
		    if (de->name[1] != '.') {
                        goto not_empty;
		    }
		    if (de->name_len >= 2) {
                        goto not_empty;
		    }
	        }
	    }
        }
        luci_put_page(page);
    }
    return 1;

not_empty:
    return 0;
}

static int
luci_delete_entry(struct luci_dir_entry_2* de, struct page *page)
{
    int err;
    loff_t pos;
    struct inode * inode = page->mapping->host;
    // Fix : an invalid ~, while computing the offset
    unsigned from = ((char*)de - (char*)page_address(page)) &
	    (luci_chunk_size(inode) - 1);
    // Fix : rec_len can be smaller than 'from', since it's an offset
    unsigned length = luci_rec_len_from_disk(de->rec_len);
    pos = page_offset(page) + from;
    luci_dbg("dentry %s pos %llu from %u len %u", de->name, pos, from, length);
    de->inode = 0;
    lock_page(page);
    err = luci_prepare_chunk(page, pos, length);
    BUG_ON(err);
    err = luci_commit_chunk(page, pos, length);
    if (err) {
        luci_err("error in commiting page chunk");
    }
    inode->i_ctime = inode->i_mtime = LUCI_CURR_TIME;
    mark_inode_dirty(inode);
    luci_put_page(page);
    return err;
}

static int
luci_unlink(struct inode * dir, struct dentry *dentry)
{
    struct inode * inode = DENTRY_INODE(dentry);
    struct luci_dir_entry_2 * de;
    struct page * page;
    int err;

    luci_dbg("name :%s", dentry->d_name.name);

    de = luci_find_entry(dir, &dentry->d_name, &page);
    if (!de) {
       err = -ENOENT;
       luci_err("name :%s not found", dentry->d_name.name);
       goto out;
    }

    err = luci_truncate(inode, 0);
    if (err) {
       err = -EIO;
       luci_err("name :%s failed to free blocks", dentry->d_name.name);
       goto out;
    }

    err = luci_delete_entry(de, page);
    if (err) {
       err = -EIO;
       luci_err("name :%s failed to delete", dentry->d_name.name);
       goto out;
    }

    inode->i_ctime = dir->i_ctime;
    inode_dec_link_count(inode);
out:
    return err;
}

static int
luci_mkdir(struct inode * dir, struct dentry *dentry, umode_t mode)
{
    int err = 0;
    struct inode *inode;

    luci_dbg_inode(dir,"mkdir");

    inode_inc_link_count(dir);
    inode = luci_new_inode(dir, S_IFDIR | mode, &dentry->d_name);
    if (IS_ERR(inode)) {
        luci_err("failed to create new inode");
        goto fail_dir;
    }
    inode->i_op = &luci_dir_inode_operations;
    inode->i_fop = &luci_dir_operations;
    inode->i_mapping->a_ops = &luci_aops;
    inode_inc_link_count(inode);
    err = luci_make_empty(inode, dir);
    if (err) {
        luci_err("failed to make empty directory");
        goto out_fail;
    }
    err = luci_add_link(dentry, inode);
    if (err) {
        luci_err("failed to add dentry in parent directory");
        goto out_fail;
    }

    unlock_new_inode(inode);
    d_instantiate(dentry, inode);
    return err;

out_fail:
    inode_dec_link_count(inode);
    inode_dec_link_count(inode);
    unlock_new_inode(inode);
    iput(inode);
    inode_dec_link_count(dir);
    return err;

fail_dir:
    inode_dec_link_count(dir);
    return PTR_ERR(inode);
}

static int
luci_rmdir(struct inode * dir, struct dentry *dentry)
{
    int err = -ENOTEMPTY;
    struct inode * inode = DENTRY_INODE(dentry);

    luci_dbg_inode(inode, "rmdir on inode");
    if (luci_empty_dir(inode) == 0) {
        err = luci_unlink(dir, dentry);
	if (err) {
            luci_err("rmdir failed for inode %lu", inode->i_ino);
	    return err;
	}
        inode_dec_link_count(inode);
        inode_dec_link_count(dir);
	return 0;
    }
    return err;
}

static struct dentry *
luci_lookup(struct inode *dir,
            struct dentry *dentry,
            unsigned int flags)
{
    ino_t ino;
    struct inode *inode;

    luci_dbg_inode(dir, "dir lookup");
    if (dentry->d_name.len > LUCI_NAME_LEN)
        return ERR_PTR(-ENAMETOOLONG);

    ino = luci_inode_by_name(dir, &dentry->d_name);
    if (!ino) {
        luci_err("inode lookup failed for %s", dentry->d_name.name);
        return NULL;
    }

    inode = luci_iget(dir->i_sb,  ino);
    if (inode == ERR_PTR(-ESTALE)) {
        luci_err("deleted inode referenced %lu", (unsigned long) ino);
        return ERR_PTR(-EIO);
    }
    //splice a disconnected dentry into the tree if one exists
    return d_splice_alias(inode, dentry);
}

static int
luci_link(struct dentry * old_dentry, struct inode * dir,
        struct dentry *dentry)
{
    luci_dbg("inode :%lu", dir->i_ino);
    return 0;
}

static int
luci_symlink(struct inode * dir, struct dentry *dentry,
        const char * symname)
{
    luci_dbg("inode :%lu", dir->i_ino);
    return 0;
}

static int
luci_mknod(struct inode * dir, struct dentry *dentry, umode_t mode, dev_t rdev)
{
    luci_dbg("inode :%lu", dir->i_ino);
    return 0;
}

static int
luci_tmpfile(struct inode *dir, struct dentry *dentry, umode_t mode)
{
    luci_dbg("inode :%lu", dir->i_ino);
    return 0;
}

#ifdef HAVE_NEW_RENAME
static int
luci_rename(struct inode * old_dir, struct dentry *old_dentry,
        struct inode * new_dir, struct dentry *new_dentry, unsigned int flags)
{
    luci_dbg_inode(old_dir, "renaming");
    return 0;
}
#else
static int
luci_rename(struct inode * old_dir, struct dentry *old_dentry,
    struct inode * new_dir, struct dentry *new_dentry)
{
    luci_dbg_inode(old_dir, "renaming");
    return 0;
}
#endif

const struct inode_operations luci_dir_inode_operations = {
    .create         = luci_create,
    .lookup         = luci_lookup,
    .link           = luci_link,
    .unlink         = luci_unlink,
    .symlink        = luci_symlink,
    .mkdir          = luci_mkdir,
    .rmdir          = luci_rmdir,
    .mknod          = luci_mknod,
    .rename         = luci_rename,
    .getattr        = luci_getattr,
    .tmpfile        = luci_tmpfile,
};
