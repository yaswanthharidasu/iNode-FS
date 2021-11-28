#include<set>
#include<cmath>
#include<deque>
#include<vector>
#include<cstring>
#include<iostream>
#include<unistd.h>
#include<unordered_map>

using namespace std;

//////////////////////////////////////////// COLORS ///////////////////////////////////////////////

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"
#define RESET "\033[0m"

/////////////////////////////////////////// DATA: Begin ///////////////////////////////////////////
/*
    File System contains:
        Super Block || file_inode_mapping || inode bitmap || data bitmap || inode blocks (inode table) || data blocks

    Storing the Super block, bitmaps, inode table in data blocks only
*/

#define DISK_SIZE 536870912             // 512MB
#define BLOCK_SIZE 1024                 // 1KB
#define NO_OF_DISK_BLOCKS 524288        // DISK_SIZE/BLOCK_SIZE
#define NO_OF_INODES 32768              // 1:16KB Ratio 
#define NO_OF_BLOCK_POINTERS 20
#define NO_OF_DESCRIPTORS 10

// For 16KB (16384 bytes)       --> one inode
// For 512MB (536870912 bytes)  --> 536870912/16384 = 32768 inodes

struct file_inode_mapping {
    char file_name[20];
    int inode_num;
};

struct inode {
    int file_size;
    // 20 direct pointers to the data blocks
    int ptr[NO_OF_BLOCK_POINTERS];
};

// False means block/inode is free
// True means not free
bool inode_bitmap[NO_OF_INODES] = { false };
int no_of_inode_bitmap_blocks = (int)(ceil((float)sizeof(inode_bitmap) / BLOCK_SIZE));

bool disk_bitmap[NO_OF_DISK_BLOCKS] = { false };
int no_of_disk_bitmap_blocks = (int)(ceil((float)sizeof(disk_bitmap) / BLOCK_SIZE));


struct super_block {
    int no_of_super_block_blocks = (int)(ceil((float)sizeof(struct super_block) / BLOCK_SIZE));
    int no_of_file_inode_mapping_blocks = (int)(ceil(((float)(sizeof(struct file_inode_mapping) * NO_OF_INODES) / BLOCK_SIZE)));
    int inode_bitmap_start = no_of_super_block_blocks + no_of_file_inode_mapping_blocks;
    int disk_bitmap_start = inode_bitmap_start + no_of_inode_bitmap_blocks;

    int inode_blocks_start = disk_bitmap_start + no_of_disk_bitmap_blocks;
    int no_of_inode_blocks = (int)(ceil(((float)(sizeof(struct inode) * NO_OF_INODES) / BLOCK_SIZE)));

    int data_blocks_start = inode_blocks_start + no_of_inode_blocks;
    int no_of_available_blocks = NO_OF_DISK_BLOCKS - data_blocks_start;
};


// ======================================= DATA: End ==============================================

string mounted_disk_name;

struct super_block sb;
struct file_inode_mapping files[NO_OF_INODES];
struct inode inode_arr[NO_OF_INODES];
bool tmp_inode_bitmap[NO_OF_INODES];
bool tmp_disk_bitmap[NO_OF_DISK_BLOCKS];

deque<int> free_disk_blocks;
deque<int> free_inodes;
unordered_map<string, int> file_to_inodes;

unordered_map<string, int> read_files;
unordered_map<string, int> write_files;
unordered_map<string, int> append_files;
unordered_map<int, string> open_files;

set<int> file_descriptors;

// int fd_count = 0;

// ===================================== Utilities ================================================

void printResult(string result) {
    cout << endl;
    cout << KCYN "=======================================================" RESET << endl;
    cout << "RESULT: " << result << endl;
    cout << KCYN "=======================================================" RESET << endl << endl;
}

bool write_block(string file_name, string block, int block_num, int block_size) {
    // cout << "Writing " << block_size << " no.of bytes" << endl << endl;
    // cout << "Writing block: " << endl << block << endl;
    // Find the inode number of the file
    int inode_num = file_to_inodes[file_name];
    // Get the data block pointer in the inode block 
    int disk_block_num = inode_arr[inode_num].ptr[block_num];
    // No block allocated to that pointer
    if (disk_block_num == -1) {
        // Get the free data block
        if (free_disk_blocks.size() == 0) {
            return false;
        }
        int free_disk_block = free_disk_blocks.front();
        free_disk_blocks.pop_front();
        // Storing that block number in the inode_arr pointers
        disk_block_num = free_disk_block;
        inode_arr[inode_num].ptr[block_num] = free_disk_block;
    }
    // cout << "Writing to block num: " << disk_block_num << endl;
    // cout << "Writing to the block: " << disk_block_num << endl;
    int size = BLOCK_SIZE;
    FILE* disk_ptr;

    char buff[size];
    memset(buff, 0, size);

    // Erasing the data present in that block 
    disk_ptr = fopen(&mounted_disk_name[0], "r+b");
    fseek(disk_ptr, (disk_block_num * BLOCK_SIZE), SEEK_SET);
    fwrite(buff, sizeof(char), block_size, disk_ptr);
    fclose(disk_ptr);

    char buff1[block_size];
    // Now write the data into the block
    strcpy(buff1, &block[0]);
    disk_ptr = fopen(&mounted_disk_name[0], "r+b");
    fseek(disk_ptr, (disk_block_num * BLOCK_SIZE), SEEK_SET);
    fwrite(buff1, sizeof(char), block_size, disk_ptr);
    fclose(disk_ptr);
    return true;
}

void read_block(int inode_num, int block_num, int block_size) {
    // cout << "Reading " << block_size << " no.of bytes" << endl << endl;
    // Get the data block pointer in the inode block 
    int disk_block_num = inode_arr[inode_num].ptr[block_num];
    // cout << "Read: " << disk_block_num << endl;
    FILE* disk_ptr;
    char buff[block_size];
    memset(buff, 0, block_size);

    // Read the data present in that block 
    disk_ptr = fopen(&mounted_disk_name[0], "r+b");
    fseek(disk_ptr, (disk_block_num * BLOCK_SIZE), SEEK_SET);
    int bytes = fread(buff, sizeof(char), block_size, disk_ptr);
    fclose(disk_ptr);

    // Print data
    for (int i = 0; i < block_size; i++)
        cout << buff[i];
}

void clear_block(int disk_block_num) {
    int size = BLOCK_SIZE;
    FILE* disk_ptr;

    char buff[size];
    memset(buff, 0, size);

    // Erasing the data present in that block 
    disk_ptr = fopen(&mounted_disk_name[0], "r+b");
    fseek(disk_ptr, (disk_block_num * BLOCK_SIZE), SEEK_SET);
    fwrite(buff, 1, BLOCK_SIZE, disk_ptr);
    fclose(disk_ptr);
}

// ===================================== File Operations ==========================================

void create_file() {
    string file_name;
    cout << "Enter filename: ";
    cin >> file_name;
    // Existing file name 
    if (file_to_inodes.find(file_name) != file_to_inodes.end()) {
        printResult(KRED "File name already exists" RESET);
        return;
    }
    // No free inodes
    if (free_inodes.size() == 0) {
        printResult(KRED "No free inodes" RESET);
        return;
    }
    // No free disk blocks
    if (free_disk_blocks.size() == 0) {
        printResult(KRED "No free disk blocks" RESET);
        return;
    }
    // Select one inode and disk_block
    int free_inode = free_inodes.front();
    free_inodes.pop_front();
    int free_disk_block = free_disk_blocks.front();
    free_disk_blocks.pop_front();

    // Store the file_name and inode number in file_inode_mapping
    strcpy(files[free_inode].file_name, &file_name[0]);
    files[free_inode].inode_num = free_inode;

    file_to_inodes[file_name] = free_inode;

    // Now store the file size and data 
    inode_arr[free_inode].file_size = 0;
    inode_arr[free_inode].ptr[0] = free_disk_block;

    printResult(KGRN "File '" + file_name + "' created successfully" RESET);
}

void open_file() {
    string file_name;
    cout << "Enter filename: ";
    cin >> file_name;

    if (file_to_inodes.find(file_name) == file_to_inodes.end()) {
        printResult(KRED "File '" + file_name + "'not present" RESET);
        return;
    }
    // Check if the file descriptors present or not
    if (file_descriptors.size() == 0) {
        printResult(KRED "No available file descriptors" RESET);
        return;
    }

    int fm;
    while (true) {
        cout << "Enter file mode (Read = 0, Write = 1, Append = 2): ";
        cin >> fm;
        if (fm == 0 || fm == 1 || fm == 2) {
            break;
        }
        cout << endl << KYEL "Enter the valid option" RESET << endl;
    }
    int fd;
    // Check if the file descriptor already exists for that file in the given mode
    if (fm == 0) {
        if (read_files.find(file_name) != read_files.end()) {
            printResult(KYEL "File is already opened in read mode with fd: " + to_string(read_files[file_name]) + RESET);
            return;
        }
        else {
            // Assigning file_descriptor to the file
            // fd_count += 1;
            fd = *file_descriptors.begin();
            read_files[file_name] = fd;
            open_files[fd] = file_name;
            file_descriptors.erase(fd);
            printResult(KGRN "File '" + file_name + "' opened in read mode with fd: " + to_string(fd) + RESET);
        }
    }
    else if (fm == 1) {
        if (write_files.find(file_name) != write_files.end()) {
            printResult(KYEL "File is already opened in write mode with fd: " + to_string(write_files[file_name]) + RESET);
            return;
        }
        else {
            // Assigning file_descriptor to the file
            // fd_count += 1;
            fd = *file_descriptors.begin();
            write_files[file_name] = fd;
            open_files[fd] = file_name;
            file_descriptors.erase(fd);
            printResult(KGRN "File '" + file_name + "' opened in write mode with fd: " + to_string(fd) + RESET);
        }
    }
    else if (fm == 2) {
        if (append_files.find(file_name) != append_files.end()) {
            printResult(KYEL "File is already opened in append mode with fd: " + to_string(append_files[file_name]) + RESET);
            return;
        }
        else {
            // Assigning file_descriptor to the file
            // fd_count += 1;
            fd = *file_descriptors.begin();
            append_files[file_name] = fd;
            open_files[fd] = file_name;
            file_descriptors.erase(fd);
            printResult(KGRN "File '" + file_name + "' opened in read mode with fd: " + to_string(fd) + RESET);
        }
    }
}

void read_file() {
    int fd;
    cout << "Enter file descriptor: ";
    cin >> fd;
    // Check if the file is opened or not
    if (open_files.find(fd) == open_files.end()) {
        printResult(KYEL "File with given descriptor: " + to_string(fd) + " doesnot exist" RESET);
        return;
    }
    string file_name = open_files[fd];
    // Check if the file is opened in write mode
    if (read_files.find(file_name) == read_files.end() || read_files[file_name] != fd) {
        printResult(KYEL "File with given descriptor: " + to_string(fd) + " is not opened in read mode" RESET);
        return;
    }

    // Find out the file inode number
    int inode_num = file_to_inodes[file_name];

    // Find the file size
    int data_size = inode_arr[inode_num].file_size;
    int no_of_blocks = data_size / BLOCK_SIZE;
    int last_block_size = BLOCK_SIZE;
    if (data_size % BLOCK_SIZE != 0) {
        no_of_blocks++;
        last_block_size = data_size % BLOCK_SIZE;
    }
    // cout << "Reading " << data_size << " no.of bytes" << endl;

    cout << endl;
    cout << KCYN "===================================" RESET << endl;
    cout << "DATA:" << endl;
    cout << KCYN "###################################" RESET << endl;

    for (int i = 0; i < no_of_blocks; i++) {
        if (i == NO_OF_BLOCK_POINTERS) {
            printResult(KYEL "Read all data blocks given to the file" RESET);
            break;
        }
        if (i == no_of_blocks - 1) {
            read_block(inode_num, i, last_block_size);
        }
        else {
            read_block(inode_num, i, BLOCK_SIZE);
        }
    }

    // // Get the first disk block number
    // int disk_block_num = inode_arr[inode_num].ptr[0];
    // cout << "Reading from block: " << disk_block_num << endl;

    // char buff[BLOCK_SIZE];
    // FILE* disk_ptr = fopen(&mounted_disk_name[0], "r+b");
    // // Move to the correct location
    // fseek(disk_ptr, (disk_block_num * BLOCK_SIZE), SEEK_SET);
    // fread(buff, 1, BLOCK_SIZE, disk_ptr);
    // fclose(disk_ptr);

    // Display data
    // string data = string(buff);
    // cout << data << endl;
    cout << endl;
    cout << KCYN "===================================" RESET << endl << endl;
}

void write_file() {
    int fd;
    cout << "Enter file descriptor: ";
    cin >> fd;
    // Check if the file is opened or not
    if (open_files.find(fd) == open_files.end()) {
        printResult(KYEL "File with given descriptor: " + to_string(fd) + " doesnot exist" RESET);
        return;
    }
    string file_name = open_files[fd];
    // Check if the file is opened in write mode
    if (write_files.find(file_name) == write_files.end() || write_files[file_name] != fd) {
        printResult(KYEL "File with given descriptor: " + to_string(fd) + " is not opened in write mode" RESET);
        return;
    }
    cout << endl;
    cout << KCYN "====================" RESET << endl;
    cout << "Enter Data:" << endl;
    cout << KCYN "====================" RESET << endl;

    cout.flush();
    // Read the data from the user
    bool flag = true;
    string data, line;
    cin.ignore();
    while (getline(cin, line)) {
        if (line == "$")
            break;
        if (flag) {
            data += line;
            flag = false;
        }
        else {
            data += "\n" + line;
        }
        line.clear();
    }

    // Find the inode number
    int inode_num = file_to_inodes[file_name];


    // Delete the existing data and release the data blocks
    for (int i = 0; i < NO_OF_BLOCK_POINTERS; i++) {
        int disk_block_num = inode_arr[inode_num].ptr[i];
        if (disk_block_num != -1) {
            // cout << disk_block_num << endl;
            clear_block(disk_block_num);
            free_disk_blocks.push_back(disk_block_num);
            inode_arr[inode_num].ptr[i] = -1;
        }
    }


    int data_size = data.length();
    int no_of_blocks = data_size / BLOCK_SIZE;
    int last_block_size = BLOCK_SIZE;
    if (data_size % BLOCK_SIZE != 0) {
        last_block_size = data_size % BLOCK_SIZE;
        no_of_blocks++;
    }
    // cout << "Writing " << data_size << " no.of bytes" << endl;
    inode_arr[inode_num].file_size = data_size;

    // Data can be stored in one block
    // if (data_size < BLOCK_SIZE) {
    //     // Find out the file inode number
    //     int inode_num = file_to_inodes[file_name];
    //     // Get the first disk block number
    //     int disk_block_num = inode_arr[inode_num].ptr[0];
    //     // Write the data
    //     char buff[data_size];
    //     strcpy(buff, &data[0]);
    //     // cout << "Buffer: " << buff << endl;
    //     FILE* disk_ptr = fopen(&mounted_disk_name[0], "r+b");
    //     // Move to the correct location
    //     fseek(disk_ptr, (disk_block_num * BLOCK_SIZE), SEEK_SET);
    //     fwrite(buff, 1, data_size, disk_ptr);
    //     fclose(disk_ptr);
    // }
    // else {
    for (int i = 0; i < no_of_blocks; i++) {
        if (i == NO_OF_BLOCK_POINTERS) {
            int i_num = file_to_inodes[file_name];
            inode_arr[i_num].file_size = 20 * BLOCK_SIZE;
            printResult(KYEL "No free blocks to write the remaining file" RESET);
            break;
        }
        string block;
        if (i == no_of_blocks - 1) {
            block = data.substr((i * BLOCK_SIZE), (last_block_size));
            if (!write_block(file_name, block, i, last_block_size)) {
                printResult(KYEL "No free blocks to write the file" RESET);
            }
        }
        else {
            block = data.substr((i * BLOCK_SIZE), (BLOCK_SIZE));
            if (!write_block(file_name, block, i, BLOCK_SIZE)) {
                printResult(KYEL "No free blocks to write the remaining file" RESET);
            }
        }
    }
    // }
    // Write at the begining of the file
}

void append_file() {
    int fd;
    cout << "Enter file descriptor: ";
    cin >> fd;
    // Check if the file is opened or not
    if (open_files.find(fd) == open_files.end()) {
        printResult(KYEL "File with given descriptor: " + to_string(fd) + " doesnot exist" RESET);
        return;
    }
    string file_name = open_files[fd];
    // Check if the file is opened in write mode
    if (append_files.find(file_name) == append_files.end() || append_files[file_name] != fd) {
        printResult(KYEL "File with given descriptor: " + to_string(fd) + " is not opened in append mode" RESET);
        return;
    }
    cout << endl;
    cout << KCYN "====================" RESET << endl;
    cout << "Enter Data:" << endl;
    cout << KCYN "====================" RESET << endl;

    cout.flush();
    // Read the data from the user
    bool flag = true;
    string data, line;
    cin.ignore();
    while (getline(cin, line)) {
        if (line == "$")
            break;
        if (flag) {
            data += line;
            flag = false;
        }
        else {
            data += "\n" + line;
        }
        line.clear();
    }

    // Find the inode of the file
    int inode_num = file_to_inodes[file_name];

    // Find the last data block of the file
    int i;
    for (i = 0; i < NO_OF_BLOCK_POINTERS; i++) {
        if (inode_arr[inode_num].ptr[i] == -1) {
            break;
        }
    }
    int last_block_index = i - 1;

    // Find the disk block number 
    int disk_block_num = inode_arr[inode_num].ptr[last_block_index];

    FILE* disk_ptr;
    char buff[BLOCK_SIZE];
    memset(buff, 0, BLOCK_SIZE);

    // Read the data present in that block 
    disk_ptr = fopen(&mounted_disk_name[0], "r+b");
    fseek(disk_ptr, (disk_block_num * BLOCK_SIZE), SEEK_SET);
    int bytes = fread(buff, sizeof(char), BLOCK_SIZE, disk_ptr);
    fclose(disk_ptr);

    // cout << "Reading from: " << disk_block_num << endl;
    string last_block_data;
    for (int i = 0; i < BLOCK_SIZE; i++) {
        if (buff[i] != '\0') {
            // cout << buff[i] << endl;
            last_block_data += buff[i];
        }
    }

    // Find the file size
    int file_size = inode_arr[inode_num].file_size;

    // Remove the last block size
    file_size -= last_block_data.length();

    // Append the newly appended data to the last block data
    last_block_data += "\n" + data;
    int data_size = last_block_data.length();

    // Now, add the last block size along with the new data
    file_size += data_size;
    inode_arr[inode_num].file_size = file_size;

    // Find the details required to add the last_block_data + new_data
    int no_of_blocks = data_size / BLOCK_SIZE;
    int last_block_size = BLOCK_SIZE;
    if (data_size % BLOCK_SIZE != 0) {
        last_block_size = data_size % BLOCK_SIZE;
        no_of_blocks++;
    }

    // cout << last_block_data << endl;
    // Now write the data from the last_block_index
    for (int i = 0; i < no_of_blocks; i++) {
        string block;
        if (i == no_of_blocks - 1) {
            block = last_block_data.substr((i * BLOCK_SIZE), (last_block_size));
            write_block(file_name, block, last_block_index, last_block_size);
        }
        else {
            block = last_block_data.substr((i * BLOCK_SIZE), (BLOCK_SIZE));
            write_block(file_name, block, last_block_index, BLOCK_SIZE);
        }
        last_block_index++;
    }
}

void close_file() {
    int fd;
    cout << "Enter file descriptor: ";
    cin >> fd;
    if (open_files.find(fd) == open_files.end()) {
        printResult(KRED "File with given desciptor: " + to_string(fd) + " is not opened" RESET);
        return;
    }
    // Find file_name
    string file_name = open_files[fd];
    if (read_files.find(file_name) != read_files.end() && read_files[file_name] == fd) {
        // Remove file
        open_files.erase(fd);
        read_files.erase(file_name);
        // Insert file descriptor into the set
        file_descriptors.insert(fd);
    }
    else if (write_files.find(file_name) != write_files.end() && write_files[file_name] == fd) {
        // Remove file
        open_files.erase(fd);
        write_files.erase(file_name);
        // Insert file descriptor into the set
        file_descriptors.insert(fd);
    }
    else if (append_files.find(file_name) != append_files.end() && append_files[file_name] == fd) {
        // Remove file
        open_files.erase(fd);
        append_files.erase(file_name);
        // Insert file descriptor into the set
        file_descriptors.insert(fd);
    }
    printResult(KGRN "File '" + file_name + "' with descriptor: '" + to_string(fd) + "' closed successfully" RESET);
}

void delete_file() {
    string file_name;
    cout << "Enter file name: ";
    cin >> file_name;
    // Check if the file exists or not
    if (file_to_inodes.find(file_name) == file_to_inodes.end()) {
        printResult(KRED "File '" + file_name + "' doesnot exists" RESET);
        return;
    }

    // Check if the file is opened or not.
    if (read_files.find(file_name) != read_files.end() || write_files.find(file_name) != write_files.end() || append_files.find(file_name) != append_files.end()) {
        printResult(KYEL "File is currently opened. Close to delete the file" RESET);
        return;
    }

    int inode_num = file_to_inodes[file_name];
    // Deleting the data in the file
    for (int i = 0; i < NO_OF_BLOCK_POINTERS; i++) {
        int disk_block_num = inode_arr[inode_num].ptr[i];
        if (disk_block_num == -1) {
            break;
        }
        // Clear the block
        clear_block(disk_block_num);
        // Remove the data block pointer in the inode
        inode_arr[inode_num].ptr[i] = -1;
        // Add that disk_block to free_disk_blocks vector
        free_disk_blocks.push_back(disk_block_num);
    }

    // Removing from file_to_inode_mapping
    strcpy(files[inode_num].file_name, "");
    files[inode_num].inode_num = -1;

    // Add the inode to the free_inodes vector
    free_inodes.push_back(inode_num);

    // After all blocks are cleared, then remove the file name
    file_to_inodes.erase(file_name);

    printResult(KGRN "File '" + file_name + "' deleted successfully" RESET);
}

void list_files() {
    if (file_to_inodes.size() > 0) {
        cout << endl;
        cout << KCYN " =============================" RESET << endl;
        cout << KCYN "|" RESET " List of files in the disk: " KCYN "|" RESET << endl;
        cout << KCYN " =============================" RESET << endl;
        for (auto it : file_to_inodes) {
            cout << KCYN "| " RESET + it.first << endl;
        }
        cout << KCYN " =============================" RESET << endl << endl;
    }
    else {
        printResult(KYEL "No files in the disk" RESET);
    }
}

void list_opened_files() {
    if (open_files.size() > 0) {
        cout << endl;
        cout << KCYN " =============================" RESET << endl;
        cout << KCYN "|" RESET " List of opened files :     " KCYN "|" RESET << endl;
        cout << KCYN " =============================" RESET << endl;
        for (auto it : open_files) {
            string mode;
            if (read_files.find(it.second) != read_files.end() && read_files[it.second] == it.first) {
                mode = "READ";
            }
            else if (write_files.find(it.second) != write_files.end() && write_files[it.second] == it.first) {
                mode = "WRITE";
            }
            else if (append_files.find(it.second) != append_files.end() && append_files[it.second] == it.first) {
                mode = "APPEND";
            }
            cout << KCYN "| " RESET + it.second << "\t\t( " << it.first << " - " << mode << " )" << endl;
        }
        cout << KCYN " =============================" RESET << endl << endl;
    }
    else {
        printResult(KYEL "No opened files" RESET);
    }
}

bool unmount_disk() {

    FILE* disk_ptr;
    int size;

    // Store back the file_inode_mapping
    size = sizeof(files);
    char fim_buff[size];
    memset(fim_buff, 0, size);
    memcpy(fim_buff, files, size);
    disk_ptr = fopen(&mounted_disk_name[0], "r+b");
    fseek(disk_ptr, (sb.no_of_super_block_blocks * BLOCK_SIZE), SEEK_SET);
    fwrite(fim_buff, 1, size, disk_ptr);
    fclose(disk_ptr);

    // Clear the file_inode_mapping
    memset(fim_buff, 0, size);
    memcpy(files, fim_buff, size);

    // cout << "After fim" << endl;

    // Make all the inodes reserved
    for (int i = 0; i < NO_OF_INODES; i++) {
        tmp_inode_bitmap[i] = true;
    }
    // Now assign the free blocks based on the free_inodes vector
    for (int i = 0; i < free_inodes.size(); i++) {
        tmp_inode_bitmap[free_inodes[i]] = false;
    }

    // Make all the data blocks reserved
    for (int i = sb.data_blocks_start; i < NO_OF_DISK_BLOCKS; i++) {
        tmp_disk_bitmap[i] = true;
    }
    // Now assign the free blocks based on the free_disk_blocks vector
    for (int i = 0; i < free_disk_blocks.size(); i++) {
        tmp_disk_bitmap[free_disk_blocks[i]] = false;
    }

    // Store back the inode bitmap
    size = sizeof(tmp_inode_bitmap);
    char tib_buff[size];
    memcpy(tib_buff, tmp_inode_bitmap, size);
    disk_ptr = fopen(&mounted_disk_name[0], "r+b");
    fseek(disk_ptr, (sb.inode_bitmap_start * BLOCK_SIZE), SEEK_SET);
    fwrite(tib_buff, 1, size, disk_ptr);
    fclose(disk_ptr);

    // Store back the disk bitmap
    size = sizeof(tmp_disk_bitmap);
    char tdb_buff[size];
    memcpy(tdb_buff, tmp_disk_bitmap, size);
    disk_ptr = fopen(&mounted_disk_name[0], "r+b");
    fseek(disk_ptr, (sb.disk_bitmap_start * BLOCK_SIZE), SEEK_SET);
    fwrite(tdb_buff, 1, size, disk_ptr);
    fclose(disk_ptr);

    // Clear the bitmaps
    memset(tmp_inode_bitmap, 0, sizeof(tmp_inode_bitmap));
    memset(tmp_disk_bitmap, 0, sizeof(tmp_disk_bitmap));

    // cout << "AFter bitmaps" << endl;

    // Store back the inode_arr
    size = sizeof(inode_arr);
    char iarr_buff[size];
    memcpy(iarr_buff, inode_arr, size);
    disk_ptr = fopen(&mounted_disk_name[0], "r+b");
    fseek(disk_ptr, sb.inode_blocks_start * BLOCK_SIZE, SEEK_SET);
    fwrite(iarr_buff, 1, size, disk_ptr);
    fclose(disk_ptr);

    // Clear the inode_arr
    memset(iarr_buff, 0, size);
    memcpy(inode_arr, iarr_buff, size);

    // cout << "After inode_arr" << endl;
    free_disk_blocks.clear();
    free_inodes.clear();
    file_to_inodes.clear();
    read_files.clear();
    write_files.clear();
    append_files.clear();
    open_files.clear();
    file_descriptors.clear();

    return true;
}

void fileOps() {
    int option;
    while (true) {
        cout << KBLU "###########################" RESET << endl;
        cout << KBLU "#" RESET " OPTIONS:                " KBLU "#" RESET << endl;
        cout << KBLU "###########################" RESET << endl;
        cout << KBLU "#" RESET " 1. Create file          " KBLU "#" RESET << endl;
        cout << KBLU "#" RESET " 2. Open file            " KBLU "#" RESET << endl;
        cout << KBLU "#" RESET " 3. Read file            " KBLU "#" RESET << endl;
        cout << KBLU "#" RESET " 4. Write file           " KBLU "#" RESET << endl;
        cout << KBLU "#" RESET " 5. Append file          " KBLU "#" RESET << endl;
        cout << KBLU "#" RESET " 6. Close file           " KBLU "#" RESET << endl;
        cout << KBLU "#" RESET " 7. Delete file          " KBLU "#" RESET << endl;
        cout << KBLU "#" RESET " 8. List of files        " KBLU "#" RESET << endl;
        cout << KBLU "#" RESET " 9. List of opened files " KBLU "#" RESET << endl;
        cout << KBLU "#" RESET " 10. Unmount             " KBLU "#" RESET << endl;
        cout << KBLU "###########################" RESET << endl << endl;
        cout << "Enter option: ";
        cin >> option;
        if (option == 1) {
            create_file();
        }
        else if (option == 2) {
            open_file();
        }
        else if (option == 3) {
            read_file();
        }
        else if (option == 4) {
            write_file();
        }
        else if (option == 5) {
            append_file();
        }
        else if (option == 6) {
            close_file();
        }
        else if (option == 7) {
            delete_file();
        }
        else if (option == 8) {
            list_files();
        }
        else if (option == 9) {
            list_opened_files();
        }
        else if (option == 10) {
            if (unmount_disk()) {
                printResult(KGRN "Disk '" + mounted_disk_name + "' unmounted successfully" RESET);
                mounted_disk_name = "";
                break;
            }
        }
        else {
            cout << endl << KYEL "Enter valid option" RESET << endl;
        }
    }
}

// ===================================== Disk Operations ==========================================

void create_disk(string& disk_name) {
    if (access(&disk_name[0], F_OK) != -1) {
        printResult(KRED "Disk '" + disk_name + "' already exists" RESET);
        return;
    }

    char buff[BLOCK_SIZE];
    memset(buff, 0, sizeof(buff));

    // Initializing the entire disk to NULL
    FILE* disk_ptr = fopen(&disk_name[0], "w");
    for (int i = 0; i < NO_OF_DISK_BLOCKS; i++) {
        fwrite(buff, 1, sizeof(buff), disk_ptr);
    }
    fclose(disk_ptr);

    // Store the super block 
    int size;
    size = sizeof(struct super_block);
    char sb_buff[size];
    memset(sb_buff, 0, size);
    memcpy(sb_buff, &sb, size);
    // cout << "super_block" << sb_buff << endl;
    disk_ptr = fopen(&disk_name[0], "r+b");
    fseek(disk_ptr, 0, SEEK_SET);
    fwrite(sb_buff, 1, size, disk_ptr);
    fclose(disk_ptr);

    // Store the file_inode_mapping
    size = sizeof(files);
    char fim_buff[size];
    memset(fim_buff, 0, size);
    memcpy(fim_buff, files, size);
    // cout << "file_inode_mapping " << fim_buff << endl;
    disk_ptr = fopen(&disk_name[0], "r+b");
    fseek(disk_ptr, sb.no_of_super_block_blocks * BLOCK_SIZE, SEEK_SET);
    fwrite(fim_buff, 1, size, disk_ptr);
    fclose(disk_ptr);

    // // ==========================================
    // disk_ptr = fopen(&disk_name[0], "r+b");
    // memset(fim_buff, 1, size);
    // fseek(disk_ptr, sb.no_of_super_block_blocks * BLOCK_SIZE, SEEK_SET);
    // fread(fim_buff, 1, size, disk_ptr);
    // fclose(disk_ptr);
    // memcpy(files, fim_buff, size);
    // for (int i = 0; i < NO_OF_INODES; i++)
    //     cout << files[i].file_name << endl;

    // // ==========================================


        // Store the inode_bitmap
    size = sizeof(inode_bitmap);
    char tib_buff[size];
    memset(tib_buff, 0, size);
    memcpy(tib_buff, inode_bitmap, size);
    disk_ptr = fopen(&disk_name[0], "r+b");
    fseek(disk_ptr, sb.inode_bitmap_start * BLOCK_SIZE, SEEK_SET);
    fwrite(tib_buff, 1, size, disk_ptr);
    fclose(disk_ptr);

    // Store the disk_bitmap
    bool tmp_disk_bitmap[NO_OF_DISK_BLOCKS];
    // Before the datablocks, all the meta data is stored.
    for (int i = 0; i < sb.data_blocks_start; i++)
        tmp_disk_bitmap[i] = true;
    // Remaining disk blocks are free
    for (int i = sb.data_blocks_start; i < NO_OF_DISK_BLOCKS; i++) {
        tmp_disk_bitmap[i] = false;
    }
    size = sizeof(tmp_disk_bitmap);
    char tdb_buff[size];
    memset(tdb_buff, 0, size);
    memcpy(tdb_buff, tmp_disk_bitmap, size);
    disk_ptr = fopen(&disk_name[0], "r+b");
    fseek(disk_ptr, sb.disk_bitmap_start * BLOCK_SIZE, SEEK_SET);
    fwrite(tdb_buff, 1, size, disk_ptr);
    fclose(disk_ptr);

    // Store the inodes
    for (int i = 0; i < NO_OF_INODES; i++) {
        // Initializing all the data block pointers to -1
        for (int j = 0; j < NO_OF_BLOCK_POINTERS; j++) {
            inode_arr[i].ptr[j] = -1;
        }
        // memset(inode_arr[i].ptr, -1, sizeof(inode_arr[i].ptr));
    }
    size = sizeof(inode_arr);
    char iarr_buff[size];
    memset(iarr_buff, 0, size);
    memcpy(iarr_buff, inode_arr, size);
    disk_ptr = fopen(&disk_name[0], "r+b");
    fseek(disk_ptr, sb.inode_blocks_start * BLOCK_SIZE, SEEK_SET);
    fwrite(iarr_buff, 1, size, disk_ptr);
    fclose(disk_ptr);

    printResult(KGRN "Disk '" + disk_name + "' created successfully" RESET);
}

void mount_disk(string& disk_name) {
    if (access(&disk_name[0], F_OK) == -1) {
        printResult(KRED "Disk '" + disk_name + "' not exists" RESET);
        return;
    }

    mounted_disk_name = disk_name;

    FILE* disk_ptr;
    int size;

    // Retreiving super_block
    size = sizeof(struct super_block);
    char sb_buff[size];
    memset(sb_buff, 0, size);
    disk_ptr = fopen(&disk_name[0], "r+b");
    fseek(disk_ptr, 0, SEEK_SET);
    fread(sb_buff, 1, size, disk_ptr);
    memcpy(&sb, sb_buff, size);
    fclose(disk_ptr);

    // Retrieve the file_inode_mapping
    size = sizeof(files);
    char fim_buff[size];
    memset(fim_buff, 0, size);
    disk_ptr = fopen(&disk_name[0], "r+b");
    fseek(disk_ptr, sb.no_of_super_block_blocks * BLOCK_SIZE, SEEK_SET);
    fread(fim_buff, 1, size, disk_ptr);
    // cout << "file_inode_mapping_buff: " << fim_buff << endl;
    memcpy(files, fim_buff, size);
    // for (int i = 0; i < NO_OF_INODES; i++) {
    //     cout << files[i].file_name << endl;
    // }
    // cout << "========" << endl;
    fclose(disk_ptr);

    // Retrieve the inode_bitmap
    size = sizeof(tmp_inode_bitmap);
    char tib_buff[size];
    memset(tib_buff, 0, size);
    disk_ptr = fopen(&disk_name[0], "r+b");
    fseek(disk_ptr, sb.inode_bitmap_start * BLOCK_SIZE, SEEK_SET);
    fread(tib_buff, 1, size, disk_ptr);
    memcpy(tmp_inode_bitmap, tib_buff, size);
    fclose(disk_ptr);

    // Retrieve the disk_bitmap
    size = sizeof(tmp_disk_bitmap);
    char tdb_buff[size];
    memset(tdb_buff, 0, size);
    disk_ptr = fopen(&disk_name[0], "r+b");
    fseek(disk_ptr, sb.disk_bitmap_start * BLOCK_SIZE, SEEK_SET);
    fread(tdb_buff, 1, size, disk_ptr);
    memcpy(tmp_disk_bitmap, tdb_buff, size);
    fclose(disk_ptr);

    // Retrieve inode_arr
    size = sizeof(inode_arr);
    char iarr_buff[size];
    memset(iarr_buff, 0, size);
    disk_ptr = fopen(&disk_name[0], "r+b");
    fseek(disk_ptr, sb.inode_blocks_start * BLOCK_SIZE, SEEK_SET);
    fread(iarr_buff, 1, size, disk_ptr);
    memcpy(inode_arr, iarr_buff, size);
    fclose(disk_ptr);

    // Store the free disk blocks
    for (int i = sb.data_blocks_start; i < NO_OF_DISK_BLOCKS; i++) {
        if (tmp_disk_bitmap[i] == false) {
            free_disk_blocks.push_back(i);
        }
    }

    // Store the free inodes
    for (int i = 0; i < NO_OF_INODES; i++) {
        if (tmp_inode_bitmap[i] == false) {
            free_inodes.push_back(i);
        }
    }

    // Store the file names
    for (int i = 0; i < NO_OF_INODES; i++) {
        // cout << files[i].file_name << " " << files[i].inode_num << endl;
        if (string(files[i].file_name).length() > 0)
            file_to_inodes[files[i].file_name] = files[i].inode_num;
    }

    // Storing the file descriptors
    for (int i = 1; i <= NO_OF_DESCRIPTORS; i++) {
        file_descriptors.insert(i);
    }

    printResult(KGRN "Disk '" + disk_name + "' mounted successfully" RESET);
    fileOps();
}

// =========================================== main() =============================================

int main() {
    while (true) {
        cout << KBLU "#########################" RESET << endl;
        cout << KBLU "#" RESET " OPTIONS:              " KBLU "#" RESET << endl;
        cout << KBLU "#########################" RESET << endl;
        cout << KBLU "#" RESET " 1. Create Disk        " KBLU "#" RESET << endl;
        cout << KBLU "#" RESET " 2. Mount Disk         " KBLU "#" RESET << endl;
        cout << KBLU "#" RESET " 3. Exit               " KBLU "#" RESET << endl;
        cout << KBLU "#########################" RESET << endl << endl;
        int option;
        cout << "Enter an option: ";
        cin >> option;
        if (option == 1) {
            string disk_name;
            cout << "Enter disk name: ";
            cin >> disk_name;
            create_disk(disk_name);
        }
        else if (option == 2) {
            string disk_name;
            cout << "Enter disk name: ";
            cin >> disk_name;
            mount_disk(disk_name);
        }
        else if (option == 3) {
            return 0;
        }
        else {
            cout << endl << KYEL "Enter valid option" RESET << endl;
        }
    }

    return 0;
}
