set print static off
set print pretty on
set print array on
set arch i386:x86-64
file build/sistema64
source start.gdb
add-symbol-file build/io      $START_IO
add-symbol-file build/utente  $START_UTENTE
target remote localhost:1234
print gdb=1
