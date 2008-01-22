# utente.s
#

# Tipi di interruzione per le chiamate di sistema e per le primitive di IO
#

.set tipo_a, 0x30
.set tipo_t, 0x31
.set tipo_g, 0x33
.set tipo_si, 0x34
.set tipo_w, 0x35
.set tipo_s, 0x36
.set tipo_ma, 0x37
.set tipo_mf, 0x38
.set tipo_d, 0x39
.set tipo_rl, 0x3a
.set tipo_r, 0x3b
.set tipo_l, 0x49
.set tipo_ep, 0x3c

.set io_tipo_rsen, 0x60
.set io_tipo_rseln, 0x61
.set io_tipo_wsen, 0x62
.set io_tipo_wse0, 0x63
.set io_tipo_tr, 0x64
.set io_tipo_trl, 0x65
.set io_tipo_tw, 0x66
.set io_tipo_rkbd, 0x64
.set io_tipo_ikbd, 0x65
.set io_tipo_wfikbd, 0x66
.set io_tipo_skbd, 0x67
.set io_tipo_smon, 0x68
.set io_tipo_wmon, 0x69
.set io_tipo_wvid, 0x69
.set io_tipo_cmon, 0x6a
.set io_tipo_gmon, 0x6b
.set io_tipo_lkbd, 0x6c
.set io_tipo_kmon, 0x6d
.set io_tipo_pkbd, 0x6e

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
