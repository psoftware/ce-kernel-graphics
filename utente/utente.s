# utente.s
#

# Tipi di interruzione per le chiamate di sistema e per le primitive di IO
#

.set tipo_a, 0x30
.set tipo_t, 0x31
.set tipo_b, 0x32
.set tipo_g, 0x33
.set tipo_si, 0x34
.set tipo_w, 0x35
.set tipo_s, 0x36
.set tipo_ma, 0x37
.set tipo_mf, 0x38
.set tipo_d, 0x39
.set tipo_rl, 0x3a
.set tipo_r, 0x3b

.set io_tipo_rsen, 0x60
.set io_tipo_rseln, 0x61
.set io_tipo_wsen, 0x62
.set io_tipo_wse0, 0x63
.set io_tipo_tr, 0x64
.set io_tipo_trl, 0x65
.set io_tipo_tw, 0x66

	.text
	.global _activate_p
_activate_p:
	int $tipo_a
	ret

	.global _terminate_p
_terminate_p:
	int $tipo_t
	ret

	.global _begin_p
_begin_p:
	int $tipo_b
	ret

	.global _give_num
_give_num:
	int $tipo_g
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

	.global _readvterm_n
_readvterm_n:
	int $io_tipo_tr
	ret

	.global _readvterm_ln
_readvterm_ln:
	int $io_tipo_trl
	ret

	.global _writevterm_n
_writevterm_n:
	int $io_tipo_tw
	ret

	.global _readlog
_readlog:
	int $tipo_rl
	ret

	.global _resident
_resident:
	int $tipo_r
	ret

	.global start, _start
start:
_start:
	jmp _main
