#ifndef __PSX_PAD
#define __PSX_PAD

#define PAD_SELECT      1
#define PAD_L3          2
#define PAD_R3          4
#define PAD_START       8
#define PAD_UP          16
#define PAD_RIGHT       32
#define PAD_DOWN        64
#define PAD_LEFT        128
#define PAD_L2          256
#define PAD_R2          512
#define PAD_L1          1024
#define PAD_R1          2048
#define PAD_TRIANGLE    4096
#define PAD_CIRCLE      8192
#define PAD_CROSS       16384
#define PAD_SQUARE      32768


#define PAD_TYPE_MOUSE 		0x01
#define PAD_TYPE_NEGCON 	0x02
#define PAD_TYPE_DIGITAL	0x04
#define PAD_TYPE_ANALOG		0x05
#define PAD_TYPE_DUAL 		0x07
#define PAD_TYPE_JOGCON		0x0E



typedef struct _PADTYPE
{
	unsigned char	stat;
	unsigned char	len:4;
	unsigned char	type:4;
	unsigned short	btn;
	unsigned char	rs_x, rs_y;
	unsigned char	ls_x, ls_y;
} PADTYPE;

#endif
