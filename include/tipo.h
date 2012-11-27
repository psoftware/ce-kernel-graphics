typedef void* addr; // indirizzo virtuale e indirizzo fisico nello spazio di memoria
typedef unsigned short ioaddr; // indirizzo (fisico) nello spazio di I/O
typedef unsigned char natb;
typedef unsigned short natw;
typedef unsigned long natl;
typedef void* str;
typedef const void* cstr;

// (* log di sistema
const natl LOG_MSG_SIZE = 80;
#ifndef AUTOCORR
enum log_sev { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERR };
const log_sev MAX_LOG = LOG_ERR;
#else /* AUTOCORR */
enum log_sev { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERR, LOG_USR };
const log_sev MAX_LOG = LOG_USR;
#endif /* AUTOCORR */

struct log_msg {
	log_sev sev;
	natw identifier;
	char msg[LOG_MSG_SIZE + 1];
};
// *)

typedef unsigned int size_t;
