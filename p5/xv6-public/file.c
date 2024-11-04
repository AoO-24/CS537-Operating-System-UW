//
// File descriptors
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

struct devsw devsw[NDEV];
struct
{
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}

// Allocate a file structure.
struct file *
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for (f = ftable.file; f < ftable.file + NFILE; f++)
  {
    if (f->ref == 0)
    {
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
struct file *
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if (f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if (f->ref < 1)
    panic("fileclose");
  if (--f->ref > 0)
  {
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if (ff.type == FD_PIPE)
    pipeclose(ff.pipe, ff.writable);
  else if (ff.type == FD_INODE)
  {
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// Get metadata about file f.
int filestat(struct file *f, struct stat *st)
{
  if (f->type == FD_INODE)
  {
    ilock(f->ip);
    stati(f->ip, st);
    iunlock(f->ip);
    return 0;
  }
  return -1;
}

// Read from file f.
int fileread(struct file *f, char *addr, int n)
{
  int r;

  if (f->readable == 0)
    return -1;
  if (f->type == FD_PIPE)
    return piperead(f->pipe, addr, n);
  if (f->type == FD_INODE)
  {
    ilock(f->ip);
    if ((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
  }
  panic("fileread");
}

// PAGEBREAK!
//  Write to file f.
int filewrite(struct file *f, char *addr, int n)
{
  int r;

  if (f->writable == 0)
    return -1;
  if (f->type == FD_PIPE)
    return pipewrite(f->pipe, addr, n);
  if (f->type == FD_INODE)
  {
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * 512;
    int i = 0;
    while (i < n)
    {
      int n1 = n - i;
      if (n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if (r < 0)
        break;
      if (r != n1)
        panic("short filewrite");
      i += r;
    }
    return i == n ? n : -1;
  }
  panic("filewrite");
}

int filewriteoff(struct file *f, char *data, int size, int offset)
{
  int bytes_written = 0, write_result = 0;

  // Check if file is not writable
  if (f->writable == 0)
  {
    return -1;
  }

  // Perform write operation if file type is FD_INODE
  if (f->type == FD_INODE)
  {
    int max_chunk_size = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * BSIZE;

    while (bytes_written < size)
    {
      int chunk_size = size - bytes_written;
      if (chunk_size > max_chunk_size)
      {
        chunk_size = max_chunk_size;
      }

      begin_op();
      ilock(f->ip);
      write_result = writei(f->ip, data + bytes_written, offset, chunk_size);
      iunlock(f->ip);
      end_op();

      if (write_result <= 0)
      {
        // If there is an error from writei, stop the write operation
        break;
      }

      // Increment offset and bytes_written by the amount written
      offset += write_result;
      bytes_written += write_result;
    }

    // Set return value based on whether all bytes were written
    write_result = (bytes_written == size) ? size : -1;
  }
  else
  {
    // If file type is not FD_INODE, output error message and panic
    panic("filewriteoff");
  }

  return write_result;
}
