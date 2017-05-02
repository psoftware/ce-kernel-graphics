MOUNTDIR="/tmp/nucleofs";
DISKNAME="build/hd.img";

# elimino l'immagine vecchia
rm $DISKNAME

# creo l'immagine FAT16
dd if=/dev/zero of=$DISKNAME bs=512 count=30000
mkfs.vfat -F 16 $DISKNAME

# la monto
if [ -d "$MOUNTDIR" ]; then
	rm -r $MOUNTDIR;
fi
mkdir $MOUNTDIR
sudo mount -o loop $DISKNAME $MOUNTDIR

# ci copio la roba
sudo cp -r disk/* $MOUNTDIR

# la smonto
sudo umount $MOUNTDIR
