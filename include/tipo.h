typedef void* addr; // indirizzo virtuale e indirizzo fisico nello spazio di memoria
typedef unsigned short ioaddr; // indirizzo (fisico) nello spazio di I/O
typedef unsigned char natb;
typedef unsigned short natw;
typedef unsigned long natl;
typedef void* str;
typedef const void* cstr;

// (* log di sistema
const natl LOG_MSG_SIZE = 80;
enum log_sev { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERR };

struct log_msg {
	log_sev sev;
	natw identifier;
	char msg[LOG_MSG_SIZE + 1];
};
// *)

#ifdef WIN
	typedef long unsigned int size_t;
#else
	typedef unsigned int size_t;
#endif
