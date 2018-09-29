#ifndef _REMOTE_H_
#define _REMOTE_H_

/* Usage Messages */
// XXX Change to Enum
#define REMOTE_USAGE_DEFAULT 0
#define REMOTE_USAGE_REMOVE 1

/* Command */
// XXX Change to Enum
#define CMD_DEFAULT		0x00
#define CMD_ADD			0x01
#define CMD_RENAME		0x02
#define CMD_REMOVE		0x04
#define CMD_SET_HEAD		0x08
#define CMD_SET_BRANCHES	0x10
#define CMD_GET_URL		0x20
#define CMD_SET_URL		0x40
#define CMD_PRUNE		0x80

/* Command-line option flags */
#define OPT_VERBOSE		0x01

#endif
