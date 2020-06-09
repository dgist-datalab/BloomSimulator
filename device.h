#ifndef __DEVICE_H__
#define __DEVICE_H__

#define KiB 1024
#define MiB (1024L*KiB)
#define GiB (1024L*MiB)
#define TiB (1024L*GiB)
#define PiB (1024L*TiB)

#define DEFPAGESIZE (16*KiB)
#define LBASIZE (4*KiB)
#define DEFBLOCKPERPAGE (512)
#define DEFTOTALSIZE (64*GiB)
#define DEFSUPERBLOCKPERBLOCK (4)

#define OP 7

#endif
