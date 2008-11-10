# utente.s
#
#define ASM 1
#include <costanti.h>

# Tipi di interruzione per le chiamate di sistema e per le primitive di IO
#

.set tipo_a, TIPO_A
.set tipo_t, TIPO_T
.set tipo_g, TIPO_G
.set tipo_si, TIPO_SI
.set tipo_w, TIPO_W
.set tipo_s, TIPO_S
.set tipo_ma, TIPO_MA
.set tipo_mf, TIPO_MF
.set tipo_d, TIPO_D
.set tipo_rl, TIPO_RL
.set tipo_r, TIPO_R
.set tipo_l, TIPO_L
.set tipo_ep, TIPO_EP

.set io_tipo_rsen, IO_TIPO_RSEN
.set io_tipo_rseln, IO_TIPO_RSELN
.set io_tipo_wsen, IO_TIPO_WSEN
.set io_tipo_wse0, IO_TIPO_WSE0
.set io_tipo_tr, IO_TIPO_TR
.set io_tipo_trl, IO_TIPO_TRL
.set io_tipo_tw, IO_TIPO_TW
.set io_tipo_rkbd, IO_TIPO_RKBD
.set io_tipo_ikbd, IO_TIPO_IKBD
.set io_tipo_wfikbd, IO_TIPO_WFIKBD
.set io_tipo_skbd, IO_TIPO_SKBD
.set io_tipo_smon, IO_TIPO_SMON
.set io_tipo_wmon, IO_TIPO_WMON
.set io_tipo_wvid, IO_TIPO_WVID
.set io_tipo_cmon, IO_TIPO_CMON
.set io_tipo_gmon, IO_TIPO_GMON
.set io_tipo_lkbd, IO_TIPO_LKBD
.set io_tipo_kmon, IO_TIPO_KMON
.set io_tipo_pkbd, IO_TIPO_PKBD
.set io_tipo_icon, IO_TIPO_ICON

	.text
	.global _activate_p
_activate_p:
	int $tipo_a
	ret

	.global _terminate_p
_terminate_p:
	int $tipo_t
	ret

	.global _give_num
_give_num:
	int $tipo_g
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

	.global _mem_alloc
_mem_alloc:
	int $tipo_ma
	ret

	.global _mem_free
_mem_free:
	int $tipo_mf
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
	int $io_tipo_rkbd
	ret

	.global _writeconsole
_writeconsole:
	int $io_tipo_wvid
	ret

	.global _iniconsole
_iniconsole:
	int $io_tipo_icon
	ret

	.global _vkbd_read
_vkbd_read:
	int $io_tipo_rkbd
	ret

	.global _vkbd_intr_enable	
_vkbd_intr_enable:
	int $io_tipo_ikbd
	ret

	.global _vkbd_wfi
_vkbd_wfi:
	int $io_tipo_wfikbd
	ret

	.global _vkbd_send
_vkbd_send:
	int $io_tipo_pkbd
	ret

	.global _vkbd_switch
_vkbd_switch:
	int $io_tipo_skbd
	ret

	.global _vmon_switch
_vmon_switch:
	int $io_tipo_smon
	ret

	.global _vmon_write_n
_vmon_write_n:
	int $io_tipo_wmon
	ret

	.global _vmon_setcursor
_vmon_setcursor:
	int $io_tipo_cmon
	ret

	.global _vmon_getsize
_vmon_getsize:
	int $io_tipo_gmon
	ret

	.global _vmon_cursor_shape
_vmon_cursor_shape:
	int $io_tipo_kmon
	ret

	.global _vkbd_leds
_vkbd_leds:
	int $io_tipo_lkbd
	ret

	.global _readlog
_readlog:
	int $tipo_rl
	ret

	.global _resident
_resident:
	int $tipo_r
	ret

	.global _log
_log:
	int $tipo_l
	ret


	.global start, _start
start:
_start:
	jmp _main
