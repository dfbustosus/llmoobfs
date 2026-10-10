# M1 read-only image format

Status: implemented, experimental. This is **not** the transactional format in
[`native-mvp.md`](native-mvp.md). Later formats may require rebuilding images.
All integers are unsigned little-endian, serialized explicitly. Block size: 4096
bytes. All reserved bytes and padding must be zero.

## Layout

`superblock → object table → file table → object payloads`

Each region begins on a block boundary. Image length must exactly match the
superblock. Tables occupy their record count rounded up to complete blocks.
Payloads are contiguous, each rounded up to a block; even an empty object reserves
one zero block. Extra trailing bytes are invalid.

## Superblock (4096 bytes)

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 8 | Magic: ASCII `MOOBFSR1` |
| 8 | 4 | Version: 1 |
| 12 | 4 | Block size: 4096 |
| 16 | 4 | File count: 1–128 |
| 20 | 4 | Object count: 1–file count |
| 24 | 8 | Object table offset: 4096 |
| 32 | 8 | File table offset |
| 40 | 8 | Payload region offset |
| 48 | 8 | Total image size |
| 56 | 4 | Superblock CRC32C |
| 60 | 4 | Object table CRC32C |
| 64 | 4 | File table CRC32C |
| 68 | 4028 | Reserved |

CRC32C uses reflected Castagnoli polynomial `0x82f63b78`, initial and final XOR
`0xffffffff`. The superblock checksum covers all 4096 bytes with its own field
zeroed. Table checksums include padding. `123456789` yields `e3069283`.

## Object record (64 bytes)

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 8 | Object ID, sequential from 1 |
| 8 | 8 | Physical payload offset from image start |
| 16 | 8 | Logical payload size, 0–65536 bytes |
| 24 | 32 | SHA-256 of logical payload bytes, excluding padding |
| 56 | 8 | Reserved |

The creator compares full bytes before sharing an object. Hash equality alone
cannot cause deduplication. Every object must have at least one file reference.

## File record (128 bytes)

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 8 | Inode number, sequential from 2 |
| 8 | 8 | Referenced object ID |
| 16 | 4 | Name length, 1–63 bytes |
| 20 | 4 | Reserved |
| 24 | 104 | Name bytes followed by zero padding |

Names are unique printable ASCII (32–126), excluding `/`, and excluding `.` and
`..` as complete names. Content may contain any bytes. Root inode: 1. Files have
independent inodes and link count 1 even when they reference the same object.

## Mount behavior and limits

The shared parser validates structure, bounds, checksums, all payload hashes and
padding before mounting. This requires a full image scan. The backing image must
remain unchanged while mounted; validation is not continuous protection against
an external writer. Metadata is checked for consistency, not authenticated.

Root: uid/gid 0, mode 0555. Files: uid/gid 0, mode 0444. Timestamps: zero.
Reads use iomap and the page cache; FIEMAP reports shared extents. Page-cache
contents can be separate per inode; disk sharing does not imply cache sharing.

No writes, subdirectories, symlinks, xattrs, mmap, direct I/O, snapshots,
transactions, encryption or semantic search are implemented.
