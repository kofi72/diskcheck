# disk check
This tool reads and write blocks of data to check for disk errors, checking for inconsistencies.

## usage
The arguments:
```
disk-check -rw disk1 -ro disk2 --no-fail disk3 --fail not_a_disk
```
After a argument all following disks are applied to previous arguments.

The line above will result in:
- processing `disk1` in read-write mode
- processing `disk2` in read-only mode (the would-be default)
- processing `disk3` in a non-fail mode (the default), meaning if the disk does not exist, or access is denied, the program won't fail
- processing `not_a_disk` in a fail mode, and since it's not a disk, the program will exit with status -1, as it could not be stat()
