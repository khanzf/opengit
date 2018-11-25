#ifndef __CLONE_H__
#define __CLONE_H__

#include "common.h"

#define CLONE_MULTI_ACK				BIT(0)
#define CLONE_MULTI_ACK_DETAILED		BIT(1)
#define CLONE_MULTI_NO_DONE			BIT(2)
#define CLONE_THIN_PACK				BIT(3)
#define CLONE_SIDE_BAND				BIT(4)
#define CLONE_SIDE_BAND_64K			BIT(5)
#define CLONE_OFS_DELTA				BIT(6)
#define CLONE_AGENT				BIT(7)
#define CLONE_SHALLOW				BIT(8)
#define CLONE_DEEPEN_SINCE			BIT(9)
#define CLONE_DEEPEN_NOT			BIT(10)
#define CLONE_DEEPEN_RELATIVE			BIT(11)
#define CLONE_NO_PROGRESS			BIT(12)
#define CLONE_INCLUDE_TAG			BIT(13)
#define CLONE_REPORT_STATUS			BIT(14)
#define CLONE_DELETE_REFS			BIT(15)
#define CLONE_QUIET				BIT(16)
#define CLONE_ATOMIC				BIT(17)
#define CLONE_PUSH_OPTIONS			BIT(18)
#define CLONE_ALLOW_TIP_SHA1_IN_WANT		BIT(19)
#define CLONE_ALLOW_REACHABLE_SHA1_IN_WANT	BIT(20)
#define CLONE_PUSH_CERT				BIT(21)
#define CLONE_FILTER				BIT(22)

struct smart_head {
        char sha[41];
        uint32_t cap;
};

struct branch {
        char sha[41];
        char *name;
};

int clone_main(int argc, char *argv[]);

#endif
