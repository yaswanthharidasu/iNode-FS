# iNode - File System

- A filesystem (fs) is a combination of the <b>data structures</b> and the <b>methods</b> that an operating system uses to keep track of files on a disk or partition; i.e., the way the files are organized on the disk.

    1) <u><b>Data structures:</b></u> These are the <b>on-disk structures</b> are utilized by the file system to organize its data and metadata.
    2) <u><b>Methods:</b></u> These are the <b>access methods</b> which are used to work on the data structures, which helps in storing the meta-data and reading/writing the data.

- If you understand the data structures and access methods of a file system, you have developed a good mental model of how it truly works.

## Implementation Detatils:

### Disk Details:
- Size of Disk: 512MB
- Entire disk is divided into <b>blocks</b>, each of size 1024 bytes.

### Data Structures:
1) <b>super_block:</b> consists of the meta-data regarding the disk, such as:

    - No.of blocks taken by each data strcture.
    - Starting blocks of the data structures.

2) <b>inode:</b> each file has an equivalent unique inode number. No.of files stored on the disk is equal to no.of inodes. It consists of the meta-data regarding the file such as:
    - File size
    - Pointers to the data blocks, where the actual data is stored.

3) <b>file_inode_mapping:</b> stores the mapping between file names and inode numbers.

4) <b>inode_bitmap:</b> stores whether an inode number is free or taken.

5) <b>disk_bitmap:</b> stores whether a data block is free or taken.


### How data is organized on the disk?

<b>Disk structure:</b>

    ----------------------------------------------------------------------------------------------------------------------------------------------
    |super_block | -->  | file_inode_mapping | --> | inode_bitmap | --> | disk_bitmap | --> | inode_blocks (inode_table) | --> | data_blocks |
    ----------------------------------------------------------------------------------------------------------------------------------------------

    Storing the super_block, bitmaps, inode_table in data blocks only

### Methods:
- The following methods are the file operations which can be performed on the disk:
    1. <b>create file:</b> creates an empty text file.
    2. <b>open file:</b> opens a particular file in read/write/append mode as specified in input,
    multiple files can be opened simultaneously.
    3. <b>read file:</b> Displays the content of the file.
    4. <b>write file:</b> Writes the data to the file (overrides existing data in file).
    5. <b>append file:</b> Appends new data to the existing file data.
    6. <b>close file:</b> Closes the file.
    7. <b>delete file:</b> Deletes the file.
    8. <b>list of files:</b> Lists all files present in the current disk.
    9. <b>list of opened files:</b> Lists all opened files and specifies the mode they are opened in.
    10. <b>unmount:</b> Closes the currently mounted disk. 

## Application Flow:
- On starting the application, the following options will be available:
    1) <b>create disk:</b> Creates an empty disk of size 500MB.
    While creating the disk, a unique name will be given to it which will be used later to mount it.
    2) <b>mount disk:</b> Opens the specified disk for various file operations (which are mentioned above).
    3) <b>exit:</b> Close the application