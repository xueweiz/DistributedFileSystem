sudo mkdir /mnt/ramdisk
sudo chmod 777 -R /mnt/ramdisk/
sudo mount -t tmpfs -o size=512m tmpfs /mnt/ramdisk
sudo rm -r files
ln -s /mnt/ramdisk/ files
dd if=/dev/urandom of=file1 bs=10M count=2
dd if=/dev/urandom of=file2 bs=20M count=2
dd if=/dev/urandom of=file3 bs=30M count=2
echo "Done!"