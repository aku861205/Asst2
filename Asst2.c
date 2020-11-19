#include <stdio.h> 
#include <dirent.h> 
#include <pthread.h>



void Directory_Handling(const char *path) 
{ 
    struct dirent *de;  // Pointer for directory entry 
  
    // opendir() returns a pointer of DIR type.  
    DIR *dir = opendir(path); 
  
    if (dir == NULL)  // opendir returns NULL if couldn't open directory 
    { 
        printf("Could not open current directory" ); 
        return; 
    } 
  
    // for readdir() 
    while ((de = readdir(dir)) != NULL) 
            printf("%s\n", de->d_name); 
    //All files and subdirectories of current directory
    closedir(dir);     
}
//need to add pthread

int File_Handling()
{

}

int main()
{
    // Directory path to list files
    char path[100];

    // Input path from user
    printf("Enter path to list files: ");
    scanf("%s", path);

    Directory_Handling(path);
    File_Handling();
}
