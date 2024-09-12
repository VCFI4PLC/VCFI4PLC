#ifndef _TEEPLC_H_
#define _TEEPLC_H_

#include <linux/ioctl.h>    

#define IOC_MAGIC  'm'

#define IOC_INIT    _IO(IOC_MAGIC, 0)
#define CMD(x) 		_IO(IOC_MAGIC, x)

#endif /* _TEEPLC_H_ */
