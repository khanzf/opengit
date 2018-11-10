#ifndef __CAT_FILE_H_
#define __CAT_FILE_H_

/* Commands */
// XXX Change to Enum
#define CAT_FILE_DEFAULT    0x00
#define CAT_FILE_TYPE       0x01
#define CAT_FILE_PRINT      0x02
#define CAT_FILE_SIZE	   0x04

int cat_file_main(int argc, char *argv[]);

#endif
