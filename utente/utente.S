# utente.s
#
#define ASM 1
#include <costanti.h>

# Tipi di interruzione per le chiamate di sistema e per le primitive di IO
#

.set tipo_a, TIPO_A
.set tipo_t, TIPO_T
.set tipo_si, TIPO_SI
.set tipo_w, TIPO_W
.set tipo_s, TIPO_S
.set tipo_d, TIPO_D
.set tipo_rl, TIPO_RL
.set tipo_re, TIPO_RE
.set tipo_l, TIPO_L
.set tipo_ep, TIPO_EP

.set io_tipo_rsen, IO_TIPO_RSEN
.set io_tipo_rseln, IO_TIPO_RSELN
.set io_tipo_wsen, IO_TIPO_WSEN
.set io_tipo_wse0, IO_TIPO_WSE0
.set io_tipo_rcon, IO_TIPO_RCON
.set io_tipo_inic, IO_TIPO_INIC
.set io_tipo_wcon, IO_TIPO_WCON

	.text
	.global _activate_p
_activate_p:
	int $tipo_a
	ret

	.global _terminate_p
_terminate_p:
	int $tipo_t
	ret


	.global _end_program
_end_program:
	int $tipo_ep
	ret

	.global _sem_ini
_sem_ini:
	int $tipo_si
	ret

	.global _sem_wait
_sem_wait:
	int $tipo_w
	ret

	.global _sem_signal
_sem_signal:
	int $tipo_s
	ret

	.global _delay
_delay:
	int $tipo_d
	ret

	.global _readse_n
_readse_n:
	int $io_tipo_rsen
	ret

	.global _readse_ln
_readse_ln:
	int $io_tipo_rseln
	ret

	.global _writese_n
_writese_n:
	int $io_tipo_wsen
	ret

	.global _writese_0
_writese_0:
	int $io_tipo_wse0
	ret

	.global _readconsole
_readconsole:
	int $io_tipo_rcon
	ret

	.global _writeconsole
_writeconsole:
	int $io_tipo_wcon
	ret

	.global _iniconsole
_iniconsole:
	int $io_tipo_inic
	ret

	.global _readlog
_readlog:
	int $tipo_rl
	ret

	.global _resident
_resident:
	int $tipo_re
	ret

	.global _log
_log:
	int $tipo_l
	ret


	.global start, _start
start:
_start:
	call _lib_init
	jmp _main
