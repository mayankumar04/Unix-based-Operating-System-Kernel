This is an attempt at creating a working prototype OS (based on Unix).

=============== EXECUTION INSTRUCTIONS ===============

Run the command "make clean; window_qemu read-input" and it should run the Text Editor.

=============== UPDATES ===============

12/21/2023: Added a rudimentary VIM Editor which uses the QEMU VGA support to produce a window during SSH (onto the Linux Machine).
  Need to edit the editor to include all the features and do some major bug fixing. Once that is done, then I need to tie it together
  with the file system to allow for writing. The file system also needs to be updated from Ext2 to something like FAT.
