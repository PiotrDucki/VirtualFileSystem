#include <unistd.h>
//#include <malloc.h>
#include <stdlib.h>
#include <memory.h>
#include "FileSystem.h"
#include <list>
#include <string>


class VirtualFileSystem
{
  using list_iterator = std::list<Hole>::iterator;
private:
  SuperBlock superBlock;
  File fileTable[MAX_FILE_COUNT];
  std::list<Hole> freeHolesList;
  SIZE offset; //max size of VFS

public:
  // size - number of bytes tworzenie wirtualnego dysku
  VirtualFileSystem(SIZE size)
  {
    FILE* file_ptr;
    superBlock.blocks_count = getRequiredBlocksNumber(size);
    superBlock.the_biggest_hole = superBlock.blocks_count; //memory is empty
    superBlock.user_space = superBlock.blocks_count*(SIZE)sizeof(Block);
    offset = sizeof(superBlock) + sizeof(File)*MAX_FILE_COUNT + sizeof(Hole)*(MAX_FILE_COUNT+1);
    superBlock.system_size = superBlock.user_space + offset;
    superBlock.user_space_in_use = 0;
    superBlock.file_count = 0;
    superBlock.holes_count = 1;

    initFileTable();
    initFreeHoleList();

    file_ptr = fopen("filesystem", "w+b");
    if (!file_ptr)
      printf("Unable to create filesystem\n");
    truncate("filesystem", superBlock.system_size);

    if(writeMetadata(file_ptr)==1)
      printf("\n            Successfully created file system           \n");
    fclose(file_ptr);
  }

  //konstruktor używany przy owieraniu dysku
  VirtualFileSystem()
  {
    FILE* file_ptr;
    file_ptr = fopen("filesystem", "r+b");
    if (!file_ptr)
      printf("Unable to load filesystem\n");
    readMetadata(file_ptr);
    offset = sizeof(superBlock) + sizeof(File)*MAX_FILE_COUNT + sizeof(Hole)*(MAX_FILE_COUNT+1);
    fclose(file_ptr);
    printf("\n            Successfully loaded file system           \n");

  }

  //kopiowanie pliku z dysku systemu Minix na dysk wirtualny
  int copyFileFromPhysicalDisk(char *file_name)
  {
     FILE* file_ptr, *vfs_ptr;
     SIZE length, file_blocks_count;
     list_iterator hole_for_file;
     unsigned long file_size;

     length = strlen(file_name);

     if(length >= MAX_NAME_LENGTH)
     { printf("Error. File name is too long\n");   return 0;  }

     if(chechIfFileExistsOnVirtualDisk(file_name)==1)
     { printf("Error. File of that name is already in file system\n");  return 0; }

     if(superBlock.file_count >= MAX_FILE_COUNT)
     {   printf("Error. File Table is full\n");   return 0;   }

     file_ptr = fopen(file_name, "r");
     vfs_ptr = fopen("filesystem", "w+b");

     if (!file_ptr)
     {     printf("Error. Failed to open file %s\n", file_name); fclose(file_ptr);  fclose(vfs_ptr);  return 0; }

     if (!vfs_ptr)
     {   printf("Error. Failed to open file system \n"); fclose(file_ptr);  fclose(vfs_ptr);  return 0;   }

    file_size = getFileSize(file_ptr);
    file_blocks_count = getRequiredBlocksNumber(file_size);

    if(file_blocks_count>superBlock.the_biggest_hole)
    { printf("Error. Not enought free space on VFS file need %d blocks\n",file_blocks_count); fclose(file_ptr);  fclose(vfs_ptr);  return 0; }

    hole_for_file = bestFit(file_blocks_count);

    for(SIZE i=0; i<MAX_FILE_COUNT;i++)
      if(fileTable[i].is_in_use == false) //find first free spot for a file
      {
          fileTable[i].is_in_use = true;
          strcpy(fileTable[i].name, file_name);
          fileTable[i].size = file_size;
          fileTable[i].used_blocks = file_blocks_count;
          fileTable[i].first_block = hole_for_file->first_block;

          superBlock.file_count++;
          superBlock.user_space_in_use+= file_blocks_count*sizeof(Block);

          if(saveFileOnVFS(fileTable[i].first_block, fileTable[i].size, vfs_ptr, file_ptr, file_name)==1)
           printf("file %s saved VFS\n", file_name);
          break;
      }

    hole_for_file->first_block += file_blocks_count;
    hole_for_file->blocks_count -= file_blocks_count;

    if(hole_for_file->blocks_count == 0)
    {
        freeHolesList.erase(hole_for_file);
        superBlock.holes_count--;
    }

    updateTheBiggestHole();

    if(writeMetadata(vfs_ptr)==1)
      printf("metadata save \n" );
    fclose(file_ptr);
    fclose(vfs_ptr);
    printf("\n            Successfully copied %s on VFS           \n", file_name);

    return 1;
  }

  //kopiowanie pliku z dysku wirtualnego na dysk systemu Minix
  int copyFileFromVirtualDisk(char *file_name)
  {

    File file;
    FILE* vfs_ptr, *file_ptr;
    bool file_is_on_vfs = false;
    char* data;

    for(SIZE i=0;i<MAX_FILE_COUNT;i++)
    {
        if(fileTable[i].is_in_use == true)
          if(strcmp(fileTable[i].name, file_name)==0)
          {
              file = fileTable[i];
              file_is_on_vfs = true;
          }
    }
    if(file_is_on_vfs == false)
    { printf("Error. File of that name is no in file system\n");  return 0; }


    vfs_ptr = fopen("filesystem", "rb");
    if (!vfs_ptr)
    {  printf("Error. Could not open file system\n");  return 0;  }
    file_ptr = fopen(file_name, "w+b");
    if (!file_ptr)
    {  printf("Error. Could not create file\n");  return 0;  }

    data = (char*)calloc(1, file.size);

    fseek (vfs_ptr , offset+sizeof(Block)*file.first_block , SEEK_SET);

    if (fread(data, file.size, 1, vfs_ptr) != 1)
    {  printf("Error. Could not write %s to temp data \n", file.name);      return 0;  }

    if (fwrite(data, file.size, 1, file_ptr) != 1)
    {  printf("Error. Failed to write data to a file on HD\n");  return 0;  }

    free(data);
    fclose(file_ptr);
    fclose(vfs_ptr);
    printf("\n            Successfully copied %s on FS           \n", file.name);
    return 1;
  }

  //usuwanie pliku z wirtualnego dysku,
  int deleteFileFromVirtualDisk(char * file_name)
  {
    SIZE i;
    bool file_is_on_vfs = false;
    int hole_attached_to_other = 0;
    SIZE next_block, first_block;
    Hole newHole;
    list_iterator help;
    FILE* vfs_ptr;


    if(superBlock.file_count == 0)
    {  printf("Error. No files on VFS\n"); return 0; }

    for(i=0;i<MAX_FILE_COUNT;i++)
    {
        if(fileTable[i].is_in_use == true)
          if(strcmp(fileTable[i].name, file_name)==0)
          {
              file_is_on_vfs = true;
              break;
          }
    }
    if(file_is_on_vfs == false)
    { printf("Error. File of that name is no in file system\n");  return 0; }

    next_block = fileTable[i].first_block + fileTable[i].used_blocks;
    first_block = fileTable[i].first_block;
    fileTable[i].is_in_use = false; //delete data


    for(auto it = freeHolesList.begin(); it != freeHolesList.end(); it++)
    {
        if(it->first_block == next_block) //looking for hole after new one
        {
          help = it; //we might need to delete this hole latter
          it->first_block = fileTable[i].first_block;
          it->blocks_count += fileTable[i].used_blocks;

          fileTable[i].used_blocks = it->blocks_count;
          hole_attached_to_other++;
        }
        if(it->first_block + it->blocks_count == first_block) //looking for hole before new one
        {
          it->blocks_count += fileTable[i].used_blocks;

          fileTable[i].used_blocks = it->blocks_count;
          fileTable[i].first_block = it->first_block;
          hole_attached_to_other++;
        }
    }
    if(hole_attached_to_other == 0)
    {
        newHole.first_block = fileTable[i].first_block;
        newHole.blocks_count =  fileTable[i].used_blocks;
        freeHolesList.push_back(newHole);
    }

    superBlock.user_space_in_use -= fileTable[i].used_blocks*sizeof(Block);
    superBlock.file_count--;

    if(hole_attached_to_other == 0) //new hole
      superBlock.holes_count++;
    else if(hole_attached_to_other == 1) //2 hole combine into one
      ;
    else if(hole_attached_to_other == 2) //3 hole combine into one
    {
      superBlock.holes_count--;
      freeHolesList.erase(help);
    }

    updateTheBiggestHole();

    vfs_ptr = fopen("filesystem", "r+b");
    if (!vfs_ptr)
    {  printf("Error. Could not open file system\n");  return 0;  }
    if(writeMetadata(vfs_ptr)==1)
      printf("metadata save \n" );
    fclose(vfs_ptr);

    printf("\n           Successfully deleted %s from VFS           \n", fileTable[i].name);

    return 1;
  }

  //wyświetlanie katalogu dysku wirtualnego
  void displayCatalogue()
  {
    printf("\n**************** FILES ****************\n" );

    if(superBlock.file_count == 0)
      printf("\n               no files              \n");

    for(SIZE i=0; i < MAX_FILE_COUNT;i++)
    {
        if(fileTable[i].is_in_use == true)
        {
          printf("\n------------- file nr %d -------------\n", i+1);
          printf("name: %s \n", fileTable[i].name);
          printf("size: %d [bytes]\n", fileTable[i].size);
          printf("used space: %d [bytes] \n", (SIZE)(fileTable[i].used_blocks*sizeof(Block)));
          printf("used_blocks: %d \n", fileTable[i].used_blocks);
          printf("first_block: %d \n", fileTable[i].first_block);
        }
    }
  }

  //wyświetlenie zestawienia z aktualną mapą zajętości wirtualnego dysku
  void displayFileSystemInformation()
  {
      printf("\n**************** VFS Information **************** \n");
      printf("user_space %d [bytes] \n", superBlock.user_space);
      printf("user_space_in_use %d [bytes] \n", superBlock.user_space_in_use);
      printf("blocks_count %d [4096 bytes]\n", superBlock.blocks_count);
      printf("system_sizet %d [bytes]\n", superBlock.system_size);
      printf("file_count %d \n", superBlock.file_count);
      printf("the_biggest_hole %d [4096 bytes]\n", superBlock.the_biggest_hole);
      //printf("offset %d",offset);

      int i=1;
      printf("\n**************** FREE MEMORY HOLES **************** \n");
      if(freeHolesList.empty()==true)
        printf("\n               no free memory               \n");
      for(auto list_it = freeHolesList.begin(); list_it != freeHolesList.end() ;list_it++)
        printf("hole nr %d\nfirst block %d\nbloscks count %d\n", i++, list_it->first_block, list_it->blocks_count );

      printf("\n**************** FILES ****************\n" );

      if(superBlock.file_count == 0)
        printf("\n               no files              \n");
      for(SIZE i=0; i < MAX_FILE_COUNT;i++)
      {
          if(fileTable[i].is_in_use == true)
          {
            printf("\n------------- file nr %d -------------\n", i+1);
            printf("name: %s \n", fileTable[i].name);
            printf("size: %d [bytes]\n", fileTable[i].size);
            printf("used space: %d [bytes] \n", (SIZE)(fileTable[i].used_blocks*sizeof(Block)));
            printf("used_blocks: %d \n", fileTable[i].used_blocks);
            printf("first_block: %d \n", fileTable[i].first_block);
          }
      }

    printf( "\n**************** *** ****************\n\n\n" );

  }

  void dispalyFileSystemBlocks()
  {
    bool bitmap[superBlock.blocks_count];

    for(SIZE i=0;i<superBlock.blocks_count;i++)
      bitmap[i] = 1;
    for(auto it = freeHolesList.begin(); it != freeHolesList.end();it++)
    {
        for(SIZE i = it->first_block; i<it->first_block+it->blocks_count;i++)
          bitmap[i] = 0;
    }
    for(SIZE j=1;j<=superBlock.blocks_count;j++)
    {
      printf("%d", bitmap[j-1]);
      if(j%100 == 0)
        printf("\n");
    }
    printf("\n\n");
  }

  void deleteVFS()
  {
    if( remove( "filesystem" ) != 0 )
      perror( "Error deleting filesystem" );
    else
      puts( "filesystem successfully deleted" );
    superBlock.blocks_count = 0;
    superBlock.the_biggest_hole = 0; //memory is empty
    superBlock.user_space = 0;
    superBlock.system_size = 0;
    superBlock.user_space_in_use = 0;
    superBlock.file_count = 0;
    superBlock.holes_count = 0;

    for(SIZE i=0;i<MAX_FILE_COUNT;i++)
      fileTable[i].is_in_use = false;

    freeHolesList.clear();
  }
  //usuwanie wirtualnego dysku,

  ~VirtualFileSystem()
  {
    freeHolesList.clear();
  }

private:
  int writeMetadata(FILE* file_ptr)
  {
    fseek (file_ptr , 0 , SEEK_SET );
    if (fwrite(&superBlock, sizeof(struct SuperBlock), 1, file_ptr) != 1)
    {
        printf("Error. Could not write VFS data (superBlock)\n");
        fclose(file_ptr);
        return 0;
    }
    for(SIZE i=0; i<MAX_FILE_COUNT; i++)
    {
      if (fwrite(&fileTable[i], sizeof(struct File), 1, file_ptr) != 1)
      {
          printf("Error. Could not write VFS data (files)\n");
          fclose(file_ptr);
          return 0;
      }
    }
    for(auto it= freeHolesList.begin(); it != freeHolesList.end(); it++)
    {
      if (fwrite(&(*it), sizeof(struct Hole), 1, file_ptr) != 1)
      {
          printf("Error. Could not write VFS data (holes)\n");
          fclose(file_ptr);
          return 0;
      }
    }
    return 1;
  }
  int readMetadata(FILE* file_ptr)
  {
    Hole h;
    fseek (file_ptr , 0 , SEEK_SET );
    if (fread(&superBlock, sizeof(struct SuperBlock), 1, file_ptr) != 1)
    {
        printf("Error. Could not read VFS data (superBlock)\n");
        fclose(file_ptr);
        return 0;
    }

    fseek (file_ptr , sizeof(struct SuperBlock) , SEEK_SET );
    for(SIZE i=0; i<MAX_FILE_COUNT; i++)
    {
      if (fread(&fileTable[i], sizeof(struct File), 1, file_ptr) != 1)
      {
          printf("Error. Could not read VFS data (files)\n");
          fclose(file_ptr);
          return 0;
      }
    }
    for(SIZE i=0; i<superBlock.holes_count; i++)
    {
      if (fread(&h, sizeof(struct Hole), 1, file_ptr) != 1)
      {
          printf("Error. Could not write VFS data (holes)\n");
          fclose(file_ptr);
          return 0;
      }
      else
      {
        freeHolesList.push_back(h);
      }
    }
    return 1;

  }

  void initFreeHoleList()
  {
      Hole firstHole;
      firstHole.first_block = 0;
      firstHole.blocks_count = superBlock.blocks_count;
      freeHolesList.push_back(firstHole);
  }

  void initFileTable()
  {
    for(SIZE i=0; i < MAX_FILE_COUNT; i++)
      fileTable[i].is_in_use = false;
  }

  int chechIfFileExistsOnVirtualDisk(char *file_name)
  {
    for(SIZE i=0; i < MAX_FILE_COUNT; i++)
    {
        if(fileTable[i].is_in_use == true)
          if(strcmp(fileTable[i].name, file_name) == 0)
            return 1;
    }
    return 0;
  }

  unsigned long getFileSize(FILE* file)
  {
      unsigned long size;
      fseek(file, 0, SEEK_END);
      size = (unsigned long) ftell(file);
      rewind(file);
      return size;
  }

  list_iterator bestFit(SIZE blocks_needed_for_file)
  {
    list_iterator current = freeHolesList.begin();
    for(auto it = freeHolesList.begin(); it != freeHolesList.end(); it++)
    {
        if(it->blocks_count<current->blocks_count && it->blocks_count>=blocks_needed_for_file)
          current = it;
    }
    return current;
  }

  SIZE getRequiredBlocksNumber(unsigned long file_size)
  {
    if (file_size % BLOCK_SIZE == 0)
      return (SIZE) (file_size / BLOCK_SIZE);
    else
      return (SIZE) (file_size / (BLOCK_SIZE)) + 1;
  }

  void updateTheBiggestHole()
  {
    SIZE the_biggest_hole_blocks_count = 0;
      for(auto it = freeHolesList.begin(); it != freeHolesList.end(); it++)
        if(it->blocks_count > the_biggest_hole_blocks_count)
          the_biggest_hole_blocks_count = it->blocks_count;

    superBlock.the_biggest_hole = the_biggest_hole_blocks_count;
  }

  int saveFileOnVFS(SIZE block_nr, SIZE file_size, FILE* vfs_ptr, FILE* file_ptr, char* name)
  {
    char* data;

    data = (char*)calloc(1, file_size);
    if (fread(data, file_size, 1, file_ptr) != 1)
    {
        printf("Error. Could not write %s to temp data \n", name);
        return 0;
    }
    fseek (vfs_ptr , offset+block_nr*sizeof(Block) , SEEK_SET );
    if (fwrite(data, file_size, 1, vfs_ptr) != 1)
    {
        printf("Error. Failed to write block to file system\n");
        return 0;
    }
    rewind(vfs_ptr);
    free(data);
    return 1;

  }

};

void test()
{
  printf("\n\n__________________________________test start__________________________________\n\n");

    VirtualFileSystem vfs(5500000);
    vfs.displayFileSystemInformation();
    printf("\n__________________________________dodawnie test.txt__________________________________\n");
    vfs.copyFileFromPhysicalDisk((char*)"test.txt");
    vfs.displayFileSystemInformation();

    printf("\n__________________________________dodawnie test.jpg__________________________________\n");
      vfs.copyFileFromPhysicalDisk((char*)"test.jpg");
      vfs.displayFileSystemInformation();


  printf("\n__________________________________dodawnie test2.txt__________________________________\n");
    vfs.copyFileFromPhysicalDisk((char*)"test2.txt");
    vfs.displayFileSystemInformation();

    printf("\n__________________________________usuwanie test z VFS.jpg__________________________________\n");
    vfs.deleteFileFromVirtualDisk((char*)"test.jpg");
    vfs.displayFileSystemInformation();

    printf("\n__________________________________dodawnie test3.txt__________________________________\n");
      vfs.copyFileFromPhysicalDisk((char*)"test3.txt");
      vfs.displayFileSystemInformation();

      printf("\n__________________________________dodawnie test.jpg__________________________________\n");
        vfs.copyFileFromPhysicalDisk((char*)"test.jpg");
        vfs.displayFileSystemInformation();


    printf("\n__________________________________usuwanie test.jpg z dysku__________________________________\n");
    if( remove( "test.jpg" ) != 0 )
      perror( "Error deleting file" );
    else
      puts( "File successfully deleted" );

    sleep(3);
    printf("\n__________________________________kopiowanie test.jpg z VFS na dysk__________________________________\n");
    vfs.copyFileFromVirtualDisk((char*)"test.jpg");
    vfs.displayFileSystemInformation();

    printf("\n__________________________________bloki na dysku__________________________________\n");
    vfs.dispalyFileSystemBlocks();

   printf("\n__________________________________katalog plików__________________________________\n");
    vfs.displayCatalogue();

    printf("\n__________________________________ponowne dodanie test.jpg__________________________________\n");
      vfs.copyFileFromPhysicalDisk((char*)"test.jpg");
      vfs.displayFileSystemInformation();

    printf("\n__________________________________dodawnie test2.jpg__________________________________\n");
        vfs.copyFileFromPhysicalDisk((char*)"test2.jpg");
        vfs.displayFileSystemInformation();

        printf("\n__________________________________koniec__________________________________\n");
          //utwórz dysku lub wczytaj dysk | dane dysku takie jak superBlock fileTable i freeHolesList są zapisane w ramie
          //kopiowanie, usuwanie danych, wyświetlenie katalogu, wyświetelnie info, usuwanie dysku
          //zamknięcie programu - wyczszczenie danych z ramu

}

int main(int argc, char* argv[])
{


  test();
  
  return 0;
}
