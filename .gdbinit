set print static off
set print pretty on
set print array on
file build/sistema
source util/start.gdb
add-symbol-file ~/CE/lib/ce/boot.bin    0x100000
add-symbol-file build/io      $START_IO
add-symbol-file build/utente  $START_UTENTE
set arch i386:x86-64:intel
target remote localhost:1234
print wait_for_gdb=0
break sistema.cpp:gdb_breakpoint
continue
