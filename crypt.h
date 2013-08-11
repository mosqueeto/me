#if CRYPT_S
#include <unistd.h>
#include <asm/types.h> /* gives __u32 as an unsigned 32bit integer */
#include <string.h>
//#include <asm/byteorder.h> /* generates warning */
//#include <endian.h> /* reputed substitute, but doesn't work*/
#include <linux/byteorder/little_endian.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef unsigned int __u32;

/* The data block processed by the encryption algorithm - 64 bits */
typedef __u32 Blowfish_Data[2];
/* The key as entered by the user - size may vary */
typedef char Blowfish_UserKey[16];
/* The expanded key for internal use - 18+4*256 words*/
typedef __u32 Blowfish_Key[1042];

/* Native byte order. For internal use ONLY. */
void _N_Blowfish_Encrypt(void *dataIn, 
					 	 void *dataOut,
					 	 const Blowfish_Key key);
void _N_Blowfish_Decrypt(void *dataIn, 
					     void *dataOut,
						 const Blowfish_Key key);

extern void Blowfish_Encrypt(void *dataIn, 
							 void *dataOut,
			       			 const Blowfish_Key key);
extern void Blowfish_Decrypt(void *dataIn, 
							 void *dataOut,
			       			 const Blowfish_Key key);

/* User key expansion. This is not byteorder dependent as all common
   implementations get it right (i.e. big-endian). */

extern void Blowfish_ExpandUserKey(const unsigned char *userKey, 
								   int userKeyLen,
				   				   Blowfish_Key key);

extern const Blowfish_Key Blowfish_Init_Key;
#endif

