# utente.s
#
#include <costanti.h>

# Tipi di interruzione per le chiamate di sistema e per le primitive di IO
#

	.text
	.global activate_p
activate_p:
	int $TIPO_A
	ret

	.global terminate_p
terminate_p:
	int $TIPO_T
	ret

	.global sem_ini
sem_ini:
	int $TIPO_SI
	ret

	.global sem_wait
sem_wait:
	int $TIPO_W
	ret

	.global sem_signal
sem_signal:
	int $TIPO_S
	ret

	.global delay
delay:
	int $TIPO_D
	ret

	.global readse_n
readse_n:
	int $IO_TIPO_RSEN
	ret

	.global readse_ln
readse_ln:
	int $IO_TIPO_RSELN
	ret

	.global writese_n
writese_n:
	int $IO_TIPO_WSEN
	ret

	.global writese_0
writese_0:
	int $IO_TIPO_WSE0
	ret

	.global readconsole
readconsole:
	int $IO_TIPO_RCON
	ret

	.global writeconsole
writeconsole:
	int $IO_TIPO_WCON
	ret

	.global iniconsole
iniconsole:
	int $IO_TIPO_INIC
	ret

	.global readhd_n
readhd_n:
	int $IO_TIPO_HDR
	ret

	.global writehd_n
writehd_n:
	int $IO_TIPO_HDW
	ret

	.global dmareadhd_n
dmareadhd_n:
	int $IO_TIPO_DMAHDR
	ret

	.global dmawritehd_n
dmawritehd_n:
	int $IO_TIPO_DMAHDW
	ret


	.global do_log
do_log:
	int $TIPO_L
	ret


	.global _start, start
_start:
start:
	call lib_init
	jmp main
