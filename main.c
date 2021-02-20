#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>

#define VFS_NAME "vfs"
#define VFS_BLOCKSIZE 4096
#define VFS_INODES 80
#define VFS_DATABLOCKS 56
#define VFS_INODESBLOCKS 5
#define VFS_BLOCKS 64
#define VFS_INODESIZE 256
#define VFS_SIZE VFS_BLOCKS*VFS_BLOCKSIZE

typedef struct Superblock {
    int n_free_data_blocks;
    int n_free_inode_blocks;
    int inode_bitmap_offset;
    int data_bitmap_offset;
    int inode_array_offset;
    int data_offset;
} Superblock;

typedef struct inodeBitmap
{
    char occupied[VFS_INODES];
} inodeBitmap;

typedef struct dataBitmap
{
    char occupied[VFS_DATABLOCKS];
} dataBitmap;

typedef struct iNode
{
    int size;
    int first_block;
    char name[128];
} iNode;

int create_vfs()
{
    FILE* fp;
    char null_sign;
    Superblock superblock;
    inodeBitmap inodeBitmap;
    dataBitmap dataBitmap;
    int i;

    fp = fopen(VFS_NAME, "r");
    if (fp)
    {
        printf("Virtual file system already exists\n");
        fclose(fp);
        return -1;
    }
    printf("File system created\n");

    fp = fopen(VFS_NAME, "a");
    if (!fp)
    {
        printf("File system does not exist\n");
        return -1;
    }

    null_sign = '\0';
    for (i = 0; i < VFS_SIZE; i++)
    {
        fwrite(&null_sign, sizeof(char), 1, fp);
    }
    fclose(fp);

    superblock.n_free_inode_blocks = VFS_INODES;
    superblock.n_free_data_blocks = VFS_DATABLOCKS;
    superblock.inode_bitmap_offset = 1;
    superblock.data_bitmap_offset = 2;
    superblock.inode_array_offset = 3;
    superblock.data_offset = 8;

    fp = fopen(VFS_NAME, "r+");
    if (!fp)
    {
        printf("File system does not exist\n");
        return -1;
    }
    /* Store Superblock in vfs */
    fwrite(&superblock, sizeof(superblock), 1, fp);

    /* Store bitmaps in vfs */
    for (i = 0; i < VFS_INODES; i++)
    {
        inodeBitmap.occupied[i] = '\0';
    }
    for (i = 0; i < VFS_DATABLOCKS; i++)
    {
        dataBitmap.occupied[i] = '\0';
    }

    fseek(fp, VFS_BLOCKSIZE*1, SEEK_SET);
    fwrite(&inodeBitmap, sizeof(inodeBitmap), 1, fp);
    fseek(fp, VFS_BLOCKSIZE*2, SEEK_SET);
    fwrite(&dataBitmap, sizeof(dataBitmap), 1, fp);
    fclose(fp);

}

int remove_vfs()
{
    FILE* fp;

    fp = fopen(VFS_NAME, "r");
    if (fp)
    {
        fclose(fp);
        unlink(VFS_NAME);
        printf("File system removed\n");
        return 0;
    }
    printf("File system does not exist\n");
    return -1;
}

int get_needed_blocks(char* filename)
{
    struct stat st;
    int needed_data_blocks, size;

    stat(filename, &st);
    size = st.st_size;
    needed_data_blocks = size / VFS_BLOCKSIZE + 1;
    return needed_data_blocks;
    
}

int find_data_space(int needed_data_blocks, dataBitmap* dataBitmap)
{
    int blocks_to_find, first, current;
    /* Find enough free data blocks */
    blocks_to_find = needed_data_blocks;
    first = 0;
    current = 0;
    while (blocks_to_find > 0 && current < VFS_DATABLOCKS)
    {
        /* Free data block */
        if (dataBitmap->occupied[current] == 0)
        {
            blocks_to_find--;
            current++;
        }
        /* Occupied data block - enough data blocks were not found */
        else
        {   
            current++;
            first = current;
            blocks_to_find = needed_data_blocks;
        }
    }
    return first;
}

int find_inode_space(inodeBitmap *inodeBitmap)
{
    int i;

    for (i = 0; i < VFS_INODES; i++)
    {
        if (inodeBitmap->occupied[i] == 0)
        {
            return i;
        }
    }
    return -1;
}

void store_data_on_vfs(int first_block, char* filename, Superblock *superblock, int size)
{
    FILE *src, *vfs;
    struct stat st;
    int i, count;
    char buf[VFS_BLOCKSIZE];
    src = fopen(filename, "r");
    vfs = fopen(VFS_NAME, "r+");
    fseek(vfs, (superblock->data_offset+first_block)*VFS_BLOCKSIZE, SEEK_SET);
    
    count = VFS_BLOCKSIZE;
    i = size;
    while (i > 0)
    {
        if (i < VFS_BLOCKSIZE)
        {
            count = i;
        }
        fread(buf, 1, count, src);
        fwrite(buf, count, 1, vfs);
        i -= count;
    }
    
    fclose(src);
    fclose(vfs);
    printf("File %s stored in file system\n", filename);

}

int is_unique(inodeBitmap *inodeBitmap, Superblock *superblock, char* filename)
{
    FILE *vfs;
    iNode iNode;
    int index;

    vfs = fopen(VFS_NAME, "r");
    index = 0;
    while (index < VFS_INODES)
    {
        if (inodeBitmap->occupied[index] == 1)
        {
            fseek(vfs, superblock->inode_array_offset*VFS_BLOCKSIZE + index*VFS_INODESIZE, SEEK_SET);
            fread(&iNode, sizeof(iNode), 1, vfs);
            if (strcmp(iNode.name, filename) == 0)
            {
                return -1;
            }
        }
        index++;
    }
    return 0;
}

int copy_on_vfs(char* filename)
{
    FILE *src, *vfs;
    Superblock superblck;
    inodeBitmap inodeBitmap;
    dataBitmap dataBitmap;
    iNode iNode;
    char name[128];
    struct stat st;
    int needed_data_blocks, inode_space, data_space, index, size, i;

    /* Open src file */
    src = fopen(filename, "r");
    if (!src)
    {
        printf("File %s does not \n", filename);
        return -1;
    }

    /* Open vfs */
    vfs = fopen(VFS_NAME, "r+");
    if (!vfs)
    {
        printf("File system does not exist\n");
        return -1;
    }

    /* Read Superblock */
    fread(&superblck, sizeof(Superblock), 1, vfs);
    /* Check if vfs is not full */
    if (superblck.n_free_inode_blocks == 0)
    {
        printf("File system is full\n");
        return -1;
    }
    
    /* Read iNode bitmap */
    fseek(vfs, superblck.inode_bitmap_offset*VFS_BLOCKSIZE, SEEK_SET);
    fread(&inodeBitmap, sizeof(inodeBitmap), 1, vfs);

    /* Read data blocks bitmap */
    fseek(vfs, superblck.data_bitmap_offset*VFS_BLOCKSIZE, SEEK_SET);
    fread(&dataBitmap, sizeof(dataBitmap), 1, vfs);

    /* Check if file is unique */
    if (is_unique(&inodeBitmap, &superblck, filename) == -1)
    {
        printf("File %s is already in file system\n", filename);
        return -1;
    }

    stat(filename, &st);
    size = st.st_size;

    needed_data_blocks = get_needed_blocks(filename);
    if (superblck.n_free_data_blocks < needed_data_blocks)
    {
        printf("There is no enough size in file system\n");
        return -1;
    }

    
    /* Find space for data */
    data_space = find_data_space(needed_data_blocks, &dataBitmap);
    /* Find space for iNode */
    inode_space = find_inode_space(&inodeBitmap);

    /* Store data */
    store_data_on_vfs(data_space, filename, &superblck, size);


    /* Create and store iNode */
    stat(filename, &st);
    iNode.first_block = data_space;
    iNode.size = st.st_size;
    strcpy(iNode.name, filename);
    fseek(vfs, (superblck.inode_array_offset*VFS_BLOCKSIZE) + (inode_space*VFS_INODESIZE), SEEK_SET);
    fwrite(&iNode, sizeof(iNode), 1, vfs);

    /* Update info in bitmaps */
    for (i = data_space; i < data_space + needed_data_blocks; i++)
    {
        dataBitmap.occupied[i] = 1;
    }
    inodeBitmap.occupied[inode_space] = 1;
    /* Store bitmaps in vfs */
    fseek(vfs, VFS_BLOCKSIZE*superblck.inode_bitmap_offset, SEEK_SET);
    fwrite(&inodeBitmap, sizeof(inodeBitmap), 1, vfs);
    fseek(vfs, VFS_BLOCKSIZE*superblck.data_bitmap_offset, SEEK_SET);
    fwrite(&dataBitmap, sizeof(dataBitmap), 1, vfs);

    /* Update info in Superblock */
    superblck.n_free_data_blocks -= needed_data_blocks;
    superblck.n_free_inode_blocks -= 1;

    /* Store Superblock in vfs */
    fseek(vfs, 0, SEEK_SET);
    fwrite(&superblck, sizeof(superblck), 1, vfs);
    fclose(vfs);
    fclose(src);

}


void write_content_to_new_file(char* filename, iNode* iNodeptr, Superblock *superblock)
{
    FILE *dest, *vfs;
    int size, count;
    char buf[VFS_BLOCKSIZE];
    char* copyfilename;

    copyfilename = malloc(strlen(filename) + strlen("copy_") + 1);
    strcpy(copyfilename, "copy_");
    strcat(copyfilename, filename);
    dest = fopen(copyfilename, "w");
    size = iNodeptr->size;
    vfs = fopen(VFS_NAME, "r+");
    fseek(vfs, superblock->data_offset*VFS_BLOCKSIZE + iNodeptr->first_block*VFS_BLOCKSIZE, SEEK_SET);
    count = VFS_BLOCKSIZE;

    while (size > 0)
    {
        if (size < VFS_BLOCKSIZE)
        {
            count = size;
        }
        fread(buf, 1, count, vfs);
        fwrite(buf, count, 1, dest);
        size -= count;
    }
    printf("File %s copied to file %s\n", filename, copyfilename);
    fclose(dest);
    fclose(vfs);

}


int copy_from_vfs(char* filename)
{
    FILE *vfs;
    Superblock superblck;
    iNode iNode;
    int iNode_found, i;

    vfs = fopen(VFS_NAME, "r+");
    
    /* Read Superblock */
    fread(&superblck, sizeof(Superblock), 1, vfs);
    
    /* Get position of file in vfs */
    
    iNode_found = 0;
    for (i = 0; i < (VFS_INODES - superblck.n_free_inode_blocks)*VFS_INODESIZE; i+= VFS_INODESIZE)
    {
        fseek(vfs, superblck.inode_array_offset*VFS_BLOCKSIZE + i, SEEK_SET);
        fread(&iNode, sizeof(iNode), 1, vfs);
        if (strcmp(iNode.name, filename) == 0)
        {
            iNode_found = 1;
            break;
        }
    }

    if (iNode_found == 0)
    {
        printf("File %s does not exist in file system\n", filename);
        return -1;
    }

    write_content_to_new_file(filename, &iNode, &superblck);

    fclose(vfs);
}


int remove_from_vfs(char* filename)
{
    FILE *vfs;
    Superblock superblck;
    iNode iNode;
    inodeBitmap inodeBitmap;
    dataBitmap dataBitmap;
    int iNode_found, inode_position, position, n_occupied_blocks, i;
    char* null_sign;
    char null_buf[VFS_INODESIZE];

    vfs = fopen(VFS_NAME, "r+");
    
    /* Read Superblock */
    fread(&superblck, sizeof(Superblock), 1, vfs);

    /* Read iNode bitmap */
    fseek(vfs, superblck.inode_bitmap_offset*VFS_BLOCKSIZE, SEEK_SET);
    fread(&inodeBitmap, sizeof(inodeBitmap), 1, vfs);

    /* Read data blocks bitmap */
    fseek(vfs, superblck.data_bitmap_offset*VFS_BLOCKSIZE, SEEK_SET);
    fread(&dataBitmap, sizeof(dataBitmap), 1, vfs);

    /* Get position of file in vfs */
    
    iNode_found = 0;
    position = 0;

    while (position < (VFS_INODES - superblck.n_free_inode_blocks)*VFS_INODESIZE && iNode_found == 0)
    {
        fseek(vfs, superblck.inode_array_offset*VFS_BLOCKSIZE + position, SEEK_SET);
        fread(&iNode, sizeof(iNode), 1, vfs);
        if (strcmp(iNode.name, filename) == 0)
        {
            iNode_found = 1;
        }
        else
        {
            position += VFS_INODESIZE;
        }
    }

    if (iNode_found == 0)
    {
        printf("File %s does not exist in file system\n", filename);
        return -1;
    }

    /* Write nulls in place of iNode */
    fseek(vfs, superblck.inode_array_offset*VFS_BLOCKSIZE + position, SEEK_SET);
    for (i = 0; i < VFS_INODESIZE; i++)
    {
        null_buf[i] = '\0';
    }
    fwrite(null_buf, VFS_INODESIZE, 1, vfs);
    
    /* Update iNode bitmap */
    inodeBitmap.occupied[(int)position/VFS_INODESIZE] = 0;
    
    /* Update data bitmap */
    n_occupied_blocks = iNode.size / VFS_BLOCKSIZE + 1;
    for (i = iNode.first_block; i < n_occupied_blocks + iNode.first_block; i++)
    {
        dataBitmap.occupied[i] = 0;
    }

    /* Store bitmaps */
    fseek(vfs, VFS_BLOCKSIZE*superblck.inode_bitmap_offset, SEEK_SET);
    fwrite(&inodeBitmap, sizeof(inodeBitmap), 1, vfs);
    fseek(vfs, VFS_BLOCKSIZE*superblck.data_bitmap_offset, SEEK_SET);
    fwrite(&dataBitmap, sizeof(dataBitmap), 1, vfs);

    /* Update Superblock */
    superblck.n_free_data_blocks += n_occupied_blocks;
    superblck.n_free_inode_blocks += 1;
    fseek(vfs, 0, SEEK_SET);
    fwrite(&superblck, sizeof(superblck), 1, vfs);

    printf("File %s removed from file system\n", filename);
    fclose(vfs);
    return 0;

}

void list_vfs()
{
    FILE *vfs;
    Superblock superblock;
    inodeBitmap inodeBitmap;
    iNode iNode;
    int index;

    vfs = fopen(VFS_NAME, "r");
    if (!vfs)
    {
        printf("File system does not exist");
        return;
    }
    fread(&superblock, sizeof(superblock), 1, vfs);

    /* Read iNode bitmap */
    fseek(vfs, superblock.inode_bitmap_offset*VFS_BLOCKSIZE, SEEK_SET);
    fread(&inodeBitmap, sizeof(inodeBitmap), 1, vfs);

    index = 0;
    printf("*************************************\n");
    while (index < VFS_INODES)
    {
        if (inodeBitmap.occupied[index] == 1)
        {
            fseek(vfs, superblock.inode_array_offset*VFS_BLOCKSIZE + index*VFS_INODESIZE, SEEK_SET);
            fread(&iNode, sizeof(iNode), 1, vfs);
            printf("\t\\_\t %s\t %iB\n", iNode.name, iNode.size);
        }
        index++;
    }
    printf("*************************************\n");

    fclose(vfs);
}


void map_vfs()
{
    FILE * vfs;
    Superblock superblock;
    inodeBitmap inodeBitmap;
    dataBitmap dataBitmap;
    int addr_counter, current_state, n_blocks, i;

    vfs = fopen(VFS_NAME, "r");
    if (!vfs)
    {
        printf("File system does not exist\n");
        return;
    }

    fread(&superblock, sizeof(superblock), 1, vfs);
    fseek(vfs, superblock.inode_bitmap_offset*VFS_BLOCKSIZE, SEEK_SET);
    fread(&inodeBitmap, sizeof(inodeBitmap), 1, vfs);
    fseek(vfs, superblock.data_bitmap_offset*VFS_BLOCKSIZE, SEEK_SET);
    fread(&dataBitmap, sizeof(dataBitmap), 1, vfs);
    printf("|Address:%d, Type:%s, Size:%d(%d BLOCKS)|\n", 0, "SUPERBLOCK", VFS_BLOCKSIZE, 1);
    printf("|Address:%d, Type:%s, Size:%d(%d BLOCKS)|\n", superblock.inode_bitmap_offset*VFS_BLOCKSIZE, "INODE BLOCKS BITMAP", VFS_BLOCKSIZE, 1);
    printf("|Address:%d, Type:%s, Size:%d(%d BLOCKS)|\n", superblock.data_bitmap_offset*VFS_BLOCKSIZE, "DATA BLOCKS BITMAP", VFS_BLOCKSIZE, 1);
    printf("|Address:%d, Type:%s, Size:%d(%d BLOCKS)|\n", superblock.inode_array_offset*VFS_BLOCKSIZE, "INODE BLOCK", VFS_BLOCKSIZE*VFS_INODESBLOCKS, VFS_INODESBLOCKS);
    addr_counter = superblock.data_offset*VFS_BLOCKSIZE;
    n_blocks = 0;
    for (i = 0; i <= VFS_DATABLOCKS; i++)
    {
        
        if (i == 0)
        {
            current_state = dataBitmap.occupied[i];
        }

        if (dataBitmap.occupied[i] != current_state || i == VFS_DATABLOCKS)
        {
            if (current_state == 0)
            {
                printf("|Address:%d, Type:%s, Size:%d, State:%s(%d BLOCKS)|\n", addr_counter, "DATA BLOCK", VFS_BLOCKSIZE*i, "FREE", n_blocks);
            }
            else
            {
                printf("|Address:%d, Type:%s, Size:%d, State:%s(%d BLOCKS)|\n", addr_counter, "DATA BLOCK", VFS_BLOCKSIZE*i, "BUSY", n_blocks);
            }
            addr_counter = superblock.data_offset*VFS_BLOCKSIZE + i*VFS_BLOCKSIZE;
            current_state = dataBitmap.occupied[i];
            n_blocks = 0;
        }
        n_blocks++;
    }
}

int main()
{
    create_vfs();
    
    copy_on_vfs("pan-tadeusz.txt");
    list_vfs();
    map_vfs();
    copy_from_vfs("pan-tadeusz.txt");
    copy_on_vfs("pan-tadeusz3800B.txt");
    list_vfs();
    map_vfs();
    copy_on_vfs("pan-tadeusz3600B.txt");
    list_vfs();
    map_vfs();
    remove_from_vfs("pan-tadeusz.txt");
    list_vfs();
    map_vfs();
    copy_on_vfs("pan-tadeusz2300B.txt");
    list_vfs();
    map_vfs();
    copy_on_vfs("pan-tadeusz2300B.txt");
    list_vfs();
    map_vfs();
    remove_from_vfs("pan-tadeusz3600B.txt");
    list_vfs();
    map_vfs();
    copy_on_vfs("small21B.txt");
    list_vfs();
    map_vfs();
    copy_from_vfs("pan-tadeusz2300B.txt");
    copy_on_vfs("pan-tadeusz10600B.txt");
    list_vfs();
    map_vfs();
    copy_on_vfs("pan-tadeusz5400B.txt");
    list_vfs();
    map_vfs();
    copy_on_vfs("pan-tadeusz9400B.txt");
    list_vfs();
    map_vfs();
    remove_from_vfs("pan-tadeusz3800B.txt");
    list_vfs();
    map_vfs();
    remove_from_vfs("pan-tadeusz5400B.txt");
    list_vfs();
    map_vfs();
    copy_on_vfs("pan-tadeusz3800B.txt");
    list_vfs();
    map_vfs();
    copy_on_vfs("pan-tadeusz5400B.txt");
    list_vfs();
    map_vfs();
    copy_on_vfs("pan-tadeusz3600B.txt");
    list_vfs();
    map_vfs();

    remove_vfs();
    return 0;
}