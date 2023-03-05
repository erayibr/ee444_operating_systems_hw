// create a disk file with "dd if=/dev/zero of=disk.image bs=2146304 count=1" before using the code
// tested on windows machine with WSL 1 (ubuntu 20.04)

// import libs
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

// declare some of the variables used inside the code
FILE *fp, *fileptr;
unsigned char FS [2146304];
int i, j, k, file_list_index, file_size_byte_index, first_block, empty_block_count, required_blocks, first_block_byte_index, data_block_address, file_name_byte;
int empty_blocks [4096];
char *buffer;
long filelen;

// format function, deletes all the information in the FS
void format(char * arg){
    
    // set first 4 byte block to 0xffffffff
    for(int i=0; i<4; i++){ 
        FS[i] = 0xff; 
        }
    
    // set all the remaining blocks to 0x0
    for(int i = 4; i < (4096*4 + 128*256) ; i++){ 
        FS[i] = 0x00;
        }

    // save the bytes into the disk
    fp = fopen(arg, "w");
    fwrite(&FS, 2146304, 1, fp);
    fclose(fp);
    printf("Formatted disk.\n");
}

// write function
void write(char * arg, char *srcPath, char *destFileName){
    
    // generate relative path from file name
    char *path = malloc (strlen (srcPath) + strlen("./"));
    path = strcat(strcat(path , "./"), srcPath);
    
    // Read the file as bytre array with the given referenced piece of code
    // Reference for reading file as byte array: https://stackoverflow.com/questions/22059189/read-a-file-as-byte-array
    fileptr = fopen(path, "rb");  // Open the file in binary mode
    fseek(fileptr, 0, SEEK_END);          // Jump to the end of the file
    filelen = ftell(fileptr);             // Get the current byte offset in the file
    rewind(fileptr);                      // Jump back to the beginning of the file

    buffer = (char *)malloc(filelen * sizeof(char)); // Enough memory for the file
    fread(buffer, filelen, 1, fileptr); // Read in the entire file
    fclose(fileptr); // Close the file
    // End of referenced code

    free(path); // free relative path variable

    // propmpt error if file size does not fit into FS
    if(filelen > 4096*512){
        printf("File too large for the filesystem, file could not be written. (File name: %s, File size: %d bytes) \n",  srcPath, (int)filelen);
        return;
    }
    
    // read the FS into FS array
    fp = fopen(arg, "rb");
    fread(&FS, 2146304, 1, fp);
    fclose(fp);
    
    // Trverse the empty blocks until the block requirement is met 
    // i.e. a 2048byte file require 4 blocks and the following for loop will only run 4 times
    // until it can obtain the block indexes.
    required_blocks = 1 + filelen/(512);
    empty_block_count = 0;
    for(int i=4; i<(4096*4 - 4); i = i + 4){
        if((FS[i] + FS[i+1] + FS[i+2] + FS[i+3] == 0) && (empty_block_count < required_blocks )){
            empty_blocks [empty_block_count] = i/4;
            empty_block_count ++;
        }
    }

    // if there are not enough blocks, prompt an error 
    if(empty_block_count < required_blocks){
        printf("Not enough space in the filesystem (%d bytes left, whereas file size is %d bytes), file could not be written. (File name: %s) \n", empty_block_count*512, required_blocks*512, srcPath);
        return;
    }
    else{ // if there are enough blocks start write operation
        
        // write the FAT table with the found blocks
        for(int i=1; i<required_blocks; i = i + 1){   
            FS [empty_blocks[i-1] * 4 + 1] = empty_blocks [i] / 255;
            FS [empty_blocks[i-1] * 4 ] = empty_blocks [i] % 255;
        }

        // write the final value in fat table as 0xff
        FS [empty_blocks[required_blocks - 1] * 4 ] = 0xff;
        FS [empty_blocks[required_blocks - 1] * 4 + 1] = 0xff;
        FS [empty_blocks[required_blocks - 1] * 4 + 2] = 0xff;
        FS [empty_blocks[required_blocks - 1] * 4 + 3] = 0xff;

        // write the actual data with the correct order based on FAT table content
        for(int i=0; i<required_blocks; i = i + 1){
            data_block_address = 4096*4 + 128*256 + empty_blocks[i]*512;
            for(int k=0; k<512; k = k + 1){
            FS[data_block_address + k] = buffer[i*512 + k];
        }
        }
    }

    // write file list
    for(int i=4096*4; i<(4096*4 + 128*256); i = i + 256){
        
        // get meaningful values from the FS indexes for simplicity
        file_list_index = (i-4096*4)/256;
        file_size_byte_index = i+253;
        first_block_byte_index = i+248;

        // Look for an empty spor in the list 
        if(FS[file_size_byte_index] == 0x00 && FS[file_size_byte_index + 1] == 0x00 && FS[file_size_byte_index + 2] == 0x00 && FS[file_size_byte_index + 3] == 0x00){
            printf("Succesful write at file list location %d.\n", file_list_index);
            
            // Write the first block in little-endian format
            FS[first_block_byte_index] = empty_blocks[0] % 255;
            FS[first_block_byte_index + 1] = empty_blocks[0] / 255;
            
            // Write the file name 
            if(sizeof(srcPath) < 248){
                for(int k=0; k<248; k = k + 1){
                    FS[i + k] = destFileName[k];
                }

                // Write the file size in little endian format
                for(int k=0; k<3; k = k + 1){
                    FS[file_size_byte_index + 2 - k] = filelen / (int)(pow(255, (2-k)));
                    filelen = filelen - FS[file_size_byte_index + 2 - k]*(int)(pow(255, (2-k)));
                }
            }
            else {
                // Prompt en error if file name is longer than 247 bytes.
                printf("File name is longer than 247 bytes, file could not be written.");
            }     
            break; 
        }
        else{
            // Prompt an error if the file list is traversed end no empty space was found.
            if(file_list_index == 127){
                printf("Disk Full, could not write.");
            }
            continue;
        }
    }

    // Write the resulting FS into the disk
    fp = fopen(arg, "w");
    fwrite(&FS, 2146304, 1, fp);
    fclose(fp);
}

// read function
void read(char * arg, char *srcFileName, char *destPath){
    
    // read the disk into FS array
    fp = fopen(arg, "rb");
    fread(&FS, 2146304, 1, fp);
    fclose(fp);

    int match_counter = 0;
    int start_block, current_block;
    int file_size = 0;

    // Traverse the file list and look for the provided file name
    for(i=0; i<128; i = i + 1){
        file_name_byte = 4096*4 + i*256;
        match_counter = 0;
        for(k=0; k<strlen(srcFileName); k++){
            if(FS[file_name_byte + k] == srcFileName [k]){
                match_counter++;
            }
        }

        // check if the file name match the provided file name
        if(match_counter == strlen(srcFileName)){
            
            // get the data like starting block and file size to start reading operation
            start_block = FS[file_name_byte + 248] + FS[file_name_byte + 249] *255;
            file_size = FS[file_name_byte + 253] + FS[file_name_byte + 254]*255 + FS[file_name_byte + 255]*255*255;
            
            // create a dynamic buffer array to read the file into
            buffer = (char *)malloc(file_size);
            current_block = start_block;
            
            // traverse the blocks that the file is using, load all data into buffer array
            for(k=0; k<(file_size/512 - 1); k++){
                for(j=0; j<512; j++){
                    buffer[k*512 + j] = FS[4096*4 + 128*256 + current_block*512 + j];
                }
                
                // get the next block 
                current_block = FS[current_block*4] + FS[current_block*4 + 1]*255;
            }

            // write the buffer content into a file using the provided desitanation file name.
            char *path = malloc (strlen (destPath) + strlen("./"));
            path = strcat(strcat(path , "./"), destPath);
            fp = fopen(path, "w");
            fwrite(&buffer, file_size, 1, fp);
            free(buffer);
            fclose(fp);
            printf("Write complete.\n");
            return;
        }
    }

    // if file not found, prompt an error
    printf("File not found.\n");
    return;
}

// delete function
void delete(char * arg, char *filename){

    // read the disk into FS array
    fp = fopen(arg, "rb");
    fread(&FS, 2146304, 1, fp);
    fclose(fp);

    int match_counter = 0;
    int start_block, current_block, next_block;
    int file_size = 0;
    
    // traverse the file list and look for a match with the provided file name
    for(i=0; i<128; i = i + 1){
        file_name_byte = 4096*4 + i*256;
        match_counter = 0;
        for(k=0; k<strlen(filename); k++){
            if(FS[file_name_byte + k] == filename [k]){
                match_counter++;
            }
        }

        // check if the file name matches the provided file name.
        if(match_counter == strlen(filename)){
            
            // start deleton by obtatining the file size and the starting block in the fat table
            start_block = FS[file_name_byte + 248] + FS[file_name_byte + 249] *255;
            file_size = FS[file_name_byte + 253] + FS[file_name_byte + 254]*255 + FS[file_name_byte + 255]*255*255;
            
            // replace the file list entry with zeros
            for(k=0; k<256; k++){
                FS[file_name_byte + k] = 0x00;
            }
            current_block = start_block;
            
            // Overwrite the related blocks of the FAT table with 0x00
            for(k=0; k<(file_size/512 - 1); k++){
                next_block = FS[current_block*4] + FS[current_block*4 + 1]*255;
                FS[current_block*4] = 0x00;
                FS[current_block*4 + 1] = 0x00;
                FS[current_block*4 + 2] = 0x00;
                FS[current_block*4 + 3] = 0x00;
                current_block = next_block;
            }
            // Prompt that file is deleted.
            printf("Deleted file with size %d bytes starting from block %d\n", file_size, start_block);
            break;
        }
    }

    // if the file was not found in the file list, propmpt an error.
    if(i == 128){
        printf("File \"%s\" is not found.\n", filename);
    }

    // write the resulting FS array into the disk
    fp = fopen(arg, "w");
    fwrite(&FS, 2146304, 1, fp);
    fclose(fp);

}


// listing function
void list(char * arg){

    // read the disk into the FS array
    fp = fopen(arg, "rb");
    fread(&FS, 2146304, 1, fp);
    fclose(fp);

    printf("file Name\tfile Size\n");

    char file_name[248];
    // printf("test\n");
    
    // start traversing the file list
    for(int i=4096*4; i<(4096*4 + 128*256); i = i + 256){
        file_list_index = (i-4096*4)/256;
        file_size_byte_index = i+253;
        // printf("%d\n", file_list_index);
        
        // check if the entry is not starting with a dot and the size is larger than zero
        if((FS[i] != 46) && ((FS[file_size_byte_index] + FS[file_size_byte_index+1] + FS[file_size_byte_index+2] + FS[file_size_byte_index+3]) > 0)){    
            // traverse the file name portion of the file list
            for(int k=0; k<247; k = k + 1){
                // print the file name
                if(FS[i+k] != 0X00){
                    printf("%c", FS[i+k]);
                }
                else{
                    // if the file name end (0x00) is reached, break
                    break;
                } 
            }
            // print the file size based on little endian format
            printf("\t%d\n", FS[file_size_byte_index] + FS[file_size_byte_index+1]*255 + FS[file_size_byte_index+2]*255*255);
        }
        
    }
}

// defrag function (incomplete)
void defrag(char * arg){

    // read the disk into FS array
    fp = fopen(arg, "rb");
    fread(&FS, 2146304, 1, fp);
    fclose(fp);

    int defragged_blocks = 0, num_of_blocks = 0, start_block, file_size;
    char buffer[512];
    char temp[4];

    for(int i=0; i<(128); i++){
        
    }
}

// main function
int main(int argc, char** argv){

    // convert the disk name into a relative path
    char *disk_name = malloc (strlen (argv[1]) + strlen("./"));
    disk_name = strcat(strcat(disk_name , "./"), argv[1]);
    
    // use the terminal inputs to call the correct function with the correct arguments
    if(strcmp(argv[2], "-format") == 0){
        format(disk_name);
        return 0;
    }
    else if(strcmp(argv[2], "-list") == 0){
        list(disk_name);
        return 0;
    }
    else if(strcmp(argv[2], "-write") == 0){
        write(disk_name, argv[3], argv[4]);
        return 0;
    }
    else if(strcmp(argv[2], "-read") == 0){
        read(disk_name, argv[3], argv[4]);
        return 0;
    }
    else if(strcmp(argv[2], "-delete") == 0){
        delete(disk_name, argv[3]);
        return 0;
    }
    else if(strcmp(argv[2], "-defrag") == 0){
        defrag(disk_name);
        return 0;
    }
    else {
        printf("Invalid argument\n");
        return 0;
    }
    
}
