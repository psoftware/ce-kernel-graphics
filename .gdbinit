set print static off
set print pretty on
set print array on
set arch i386:x86-64
file build/sistema64
add-symbol-file build/io      0x0000010000000100
add-symbol-file build/utente  0xffffff0000000100
target remote localhost:1234
print gdb=1
