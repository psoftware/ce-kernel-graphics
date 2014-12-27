set print static off
set print pretty on
set print array on
set arch i386:x86-64
file build/sistema64
target remote localhost:1234
print gdb=1
#add-symbol-file build/io      0x40400000
#add-symbol-file build/utente  0x80000000
