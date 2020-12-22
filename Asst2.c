#include <stdio.h> 
#include <math.h>
#include <dirent.h> 
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <libgen.h>

#define RESET   "\033[0m"
#define RED     "\033[31m"      
#define GREEN   "\033[32m"  
#define YELLOW  "\033[33m"      
#define BLUE    "\033[34m"      
#define CYAN    "\033[36m"   
#define WHITE   "\033[37m"    
#define TRACK(s,x) printf("%s\tPath:%-30s\n",s,((ARGS*)x)->path);


//static pthread_t* threads_arr;
//int threads_count = 0;
//pthread_mutex_t lock;


typedef struct node
{
    double prob;
    int count;
    char* str;
    struct node* next;
}NODE;

typedef struct list
{
    int total;
    int tokens;
    char* path;
    NODE* first;
    struct list* next;
}LIST;

typedef struct datastructure
{
    LIST* first;
    LIST* last;
    int numberOfFiles;
    pthread_mutex_t m;
}DS;

typedef struct args
{
    char* path;
    DS* ds;
    //pthread_mutex_t m;
}ARGS;


void* fileHandler(void* );
int tokenizing(LIST* );
void computeProb(LIST*);
void insert(LIST* ,char* );
void report(NODE* );
char* parse_str(char* ,int* );
void sortingList(DS* );
void insertList(LIST** , LIST* );
NODE** getMeanList(LIST* ,LIST*);
double getKLD(NODE* ,NODE*);
NODE** getOutList(DS* );
void sortingNode(NODE** );
void insertNode(NODE** , NODE* );

/*
 * Function: newList
 * ----------------------------
 *  This function constructs a list and set the initial values
 *
 *   returns: LIST* pointer
 */
LIST* newList(char* path)
{
    LIST* new = (LIST*)malloc(sizeof(LIST));
    new->path = strdup(path);
    strcpy(new->path,path);
    new->next = NULL;
    new->tokens = 0;
    new->total = 0;
    new->first = NULL;
    return new;
}

/*
 * Function: newDS
 * ----------------------------
 *  This function constructs a datastructure and set the initial values
 *
 *   returns: DS* pointer
 */
DS* newDs()
{
    DS* new = (DS*)malloc(sizeof(DS));
    new->first = NULL;
    new->last = NULL;
    new->numberOfFiles = 0;
    pthread_mutex_init(&new->m, NULL);
    return new;
}

/*
 * Function: newNode
 * ----------------------------
 *  This function constructs a new node and set the initial values
 *
 *   returns: NODE* pointer
 */
NODE* newNode(char* str)
{
    NODE* new = (NODE*)malloc(sizeof(NODE));
    new->str = strdup(str);
    strcpy(new->str,str);
    new->prob = 0;
    new->count = 1;
    new->next = NULL;
    return new;
}

/*
 * Function: newArgs
 * ----------------------------
 *  This function constructs a new args and copy the ptr of DS
 *
 *   returns: ARGS* pointer
 */
ARGS* newArgs(char* path,ARGS* old)
{
    ARGS* new = (ARGS*)malloc(sizeof(ARGS));
    new->ds = old->ds;
    new->path = strdup(path);
    strcpy(new->path,path);
    return new;
}

/*
 * Function: destroyNodes
 * ----------------------------
 *  This function free all the nodes in the nodes list
 */
void destroyNodes(NODE* head)
{
    NODE* temp;
    while (head != NULL)
    {
       temp = head;
       head = head->next;
       free(temp->str);
       free(temp);
    }
}

/*
 * Function: destroyLists
 * ----------------------------
 *  This function free all the Lists struct
 */
void destroyLists(LIST* head)
{
    LIST* temp;
    while (head != NULL)
    {
       temp = head;
       head = head->next;
       destroyNodes(temp->first);
       free(temp->path);
       free(temp);
    }
}


/*
 * Function: directoryHandler
 * ----------------------------
 *  This function takes a path and the pointer from args
 *  open the directory and iterate through all files and subdirectories in that path
 *  it create a new pthread and pass directoryHandler function to it, if a directory is found
 *  it create a new pthread and pass fileHandler function to it, if a file is found
 * 
 *  *args: points to the address contains the path and datastructure
 *  
 *  return NULL ptr
 */
void* directoryHandler(void* args)
{

    struct dirent *de;  
    DIR *dir = opendir(((ARGS*)args)->path); 
    struct stat buf;

    //check if this dir is accessible
    if (dir == NULL)
    { 
        printf("Could not open current directory:%s\n",((ARGS*)args)->path); 
        return NULL; 
    } 

    pthread_t threads[2000]; 
    pthread_t threads2[2000];
    
    int count = 0;    
    int count2 = 0;
    

    
    //iterate through the whole directory
    while ((de = readdir(dir)) != NULL) 
    {

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s",((ARGS*)args)->path,de->d_name);
        stat(path,&buf);

        ARGS* a = newArgs(path,(ARGS*)args);

        //a directory is found
        if(S_ISDIR(buf.st_mode))
        {
            if(((de->d_name[0]) !='.' && (de->d_name[1]) != '.') || (de->d_name[0]) !='.')
            {
                //TRACK("Dir",a);
                pthread_create(&threads[count], NULL,directoryHandler,(void*)a);
                count++;
            }
        }
        //a regular file is found
        if(S_ISREG(buf.st_mode))
        {   
            pthread_create(&threads2[count2], NULL,fileHandler,(void*)a);
            count2++;
        }

    }

    //waiting for all directory threads
    for(int i = 0;i<count;i++)
    {            
        pthread_join(threads[i],NULL); 
    }

    //waiting for all file threads
    for(int i = 0;i<count2;i++)
    {            
        pthread_join(threads2[i],NULL); 
    }

    free(((ARGS*)args)->path);
    closedir(dir);   
    return NULL;
}

/*
 * Function: fileHandler
 * ----------------------------
 *  This function takes a path and the pointer from args
 *  it adds creates a new LIST and add it to the datastructure if the path is accessible
 *  and then, it calls the tokenizing function and computeProb function
 * 
 *  *args: points to the address contains the path and datastructure
 *  
 *  return NULL ptr
 */
void* fileHandler(void* args)
{
    //TRACK("File",args);
    if(access(((ARGS*)args)->path,R_OK) == -1)
    {
        printf("File doesn't exist");
        return NULL;
    }

    DS* temp = ((ARGS*)args)->ds;
    LIST* current =  newList(((ARGS*)args)->path);

    pthread_mutex_lock(&temp->m);

    if(temp->first == NULL)
    {
        temp->first = current;
        temp->last = current;
    }
    else
    {   
        temp->last->next = current;
        temp->last = current;
    }
    ++(temp->numberOfFiles);

    pthread_mutex_unlock(&temp->m);

    free(((ARGS*)args)->path);
    free(args);

    
    if(tokenizing(current))
        computeProb(current);

    pthread_exit(NULL);    
}



/*
 * Function: tokenizing
 * ----------------------------
 *  This function reads the argument file word by word
 *  and parse the string determine if the string counts as a token
 *  if the string is a valid token, then the string is inserted into the node list
 * 
 *  *l: points to file struct
 *  
 *  return 0 if there aren't any valid token else return 1
 */
int tokenizing(LIST* l)
{
    FILE *file = fopen(l->path, "r");


    //check accessible
    if(file == NULL)
    {      
        printf("Cannot access this file\n");
        return 0;
    }

    int node_count = 0;

    //read the file
    while(1)
    {

        char temp[10000]; 
        char temp2[10000];

        int ret = fscanf(file,"%s",temp);

        if(ret == EOF)
        {
            break;
        }
        
        int isToken = 0;

        int i = 0;
        int j = 0;
        int count = 0;
        //check if the string is a valid token and convert the string to lower 
        while(temp[i]!='\0')
        {

            if(isalpha(temp[i])||temp[i]=='-')
            {
                count = 1;
                temp2[j] = tolower(temp[i]);
                j++;
                i++;
            }
            else
            {   
                i++;
            }
        
        }
        temp2[j] = '\0';

        if(count)
        {
            insert(l,temp2);
            node_count++;
        }
    }

    fclose(file);

    if(node_count>0)
        return 1;
    else
        return 0;
    
}

/*
 * Function: computePro
 * ----------------------------
 *  This function computes the discrete probability of each token
 * 
 *  *l: points to file struct
 */
void computeProb(LIST* l)
{
    NODE* current = l->first;

    while(current != NULL)
    {
        current->prob = (double)current->count / l->total;
        current = current->next;
    }
}

/*
 * Function: insert
 * ----------------------------
 *  This function insert the valid token into the list in the alphabetical oreder
 * 
 *  *l: points to file struct
 *  *token: the token going to be inserted
 */
void insert(LIST* l,char* token)
{   
    ++(l->total);
    
    //empty list
    if(l->first == NULL)
    {
        l->first = newNode(token);
        ++(l->tokens);
        return;
    }

    //place new node in the beginning
    if(strcmp(l->first->str,token) > 0)
    {
    
        NODE* new = newNode(token);
        new->next = l->first;
        l->first = new;
        ++(l->tokens);
        return;
    }
    //place new node in the middle
    else
    {
        NODE* current = l->first;
        while (current->next != NULL && strcmp(current->next->str,token) <= 0)
        {
            //repeat token
            if(strcmp(current->next->str,token) == 0)
            {
                ++(current->next->count);
                return;
            }
            current = current->next;
        }

        if(strcmp(current->str,token) == 0)
        {
            ++(current->count);
            return;
        }
        
        NODE* new = newNode(token);
        new->next = current -> next;
        current -> next = new;
        ++(l->tokens);
    }
}

/*
 * Function: sortingList
 * ----------------------------
 *  This function sorts the files list based on the tokens they contain using insertion sort
 * 
 *  *d: points to data structure
 */
void sortingList(DS* d)
{
   LIST* sorted = NULL;
   LIST* current = d->first;

   while(current != NULL)
   {
        LIST* next = current->next;
        insertList(&sorted,current);
        current = next;
   }

   d->first = sorted;
}

/*
 * Function: insertList
 * ----------------------------
 *  This function is the helper function of insertion sort
 * 
 *  *d: points to list of LIST*
 *  *new : inserting file 
 */
void insertList(LIST** d, LIST* new)
{
    LIST* current;

    if(*d == NULL || (*d)->total >= new->total)
    {
        new->next = *d;
        *d= new;
    }
    else
    {
        current = *d;
        while(current->next != NULL && current->next->total < new->total)
        {
            current = current->next;
        }
        new->next = current->next;
        current->next = new;
    }
}

/*
 * Function: sortingNode
 * ----------------------------
 *  This function sorts the nodes list based on the count(the total tokens of two files)using insertion sort
 * 
 *  *l: points to the node list
 */
void sortingNode(NODE** l)
{
   NODE* sorted = NULL;
   NODE* current = *l;

   while(current != NULL)
   {
        NODE* next = current->next;
        insertNode(&sorted,current);
        current = next;
   }

   *l = sorted;
}

/*
 * Function: insertNode
 * ----------------------------
 *  This function is the helper function of insertion sort
 * 
 *  **d: points to list of NODE*
 *  *new : inserting node
 */
void insertNode(NODE** d, NODE* new)
{
    NODE* current;

    if(*d == NULL || (*d)->count >= new->count)
    {
        new->next = *d;
        *d= new;
    }
    else
    {
        current = *d;
        while(current->next != NULL && current->next->count < new->count)
        {
            current = current->next;
        }
        new->next = current->next;
        current->next = new;
    }
}

/*
 * Function: getMeanList
 * ----------------------------
 *  This function constructs a list of mean value between two files
 * 
 *  *file1: points to the first file being compared
 *  *file2: points to the second file being compared
 * 
 *  return the a list of NODE* containning the token and mean value
 */
NODE** getMeanList(LIST* file1,LIST* file2)
{
    int i = 0;
    int count = file1->total + file2->total;
    NODE** ret = (NODE**)malloc(count*sizeof(NODE*));
    NODE *temp1 = file1->first;
    NODE *temp2 = file2->first;


    //traverse through the file1 and file2
    while(temp1 != NULL && temp2 != NULL)
    {
        //1. both list have same token, both temp move to next
        if(strcmp(temp1->str,temp2->str) == 0)
        {
            ret[i] = newNode(temp1->str);
            ret[i]->prob = (temp1->prob + temp2->prob) / 2;
            temp1 = temp1->next;
            temp2 = temp2->next;
            

        }
        //2.compare two list,the smaller one add to list and temp move to next
        else if(strcmp(temp1->str,temp2->str) < 0)
        {
            ret[i] = newNode(temp1->str);
            ret[i]->prob = (temp1->prob) / 2;
            temp1 = temp1 -> next;
        }
        else
        {

            ret[i] = newNode(temp2->str);
            ret[i]->prob = (temp2->prob) / 2;
            temp2 = temp2->next;
        }

        i++; 
    }

    while(temp1 != NULL)
    {
        ret[i] = newNode(temp1->str);
        ret[i]->prob = (temp1->prob) / 2;
        temp1 = temp1 -> next;
        i++;
    }

    while(temp2 != NULL)
    {
        ret[i] = newNode(temp2->str);
        ret[i]->prob = (temp2->prob) / 2;
        temp2 = temp2 -> next;
        i++;
    }

    //connect all the nodes in the mean list
    for(int j = 0;j<i-1;j++)
    {
        ret[j]->next = ret[j+1];
    }

    return ret;
}

/*
 * Function: getKLD
 * ----------------------------
 *  This function calculates the KLD 
 * 
 *  *fileHead: the list of file
 *  *meanHead: the list of mean
 *  
 *  return the double value of KLD 
 */
double getKLD(NODE* fileHead ,NODE* meanHead)
{
    double sum = 0;
    int i = 0;

    NODE* temp1 = fileHead;
    NODE* temp2 = meanHead;

    while(temp1 != NULL)
    {
        //file list and mean list comtainning same token
        if(strcmp(temp1->str,temp2->str) == 0)
        {
            double num = temp1->prob / temp2->prob;
            sum += temp1->prob * log10(num);
            temp1 = temp1->next;
            temp2 = temp2->next;
        }
        //file list doesn't contain the token file list has
        else
        {
            temp2 = temp2->next;
        }
    }
    return sum;   
}

/*
void printNodes(NODE* head)
{
    NODE* temp = head;
    while(temp != NULL)
    {
        printf("Token: %-30sCount: %-3d Prob: %-3.3f\n",temp->str,temp->count,temp->prob);
        temp = temp->next;
    }
}
*/


/*
 * Function: getOutList
 * ----------------------------
 *  This function constructs the list for reporting
 * 
 * *ds:point to the data structure
 */
NODE** getOutList(DS* ds)
{
    int numberOfPairs = ds->numberOfFiles * (ds->numberOfFiles- 1 ) / 2;
    NODE** ret = (NODE**)malloc(sizeof(NODE*)*numberOfPairs);
    
    LIST* file1 = ds->first;
    LIST* file2;

    double kld,kld2;

    int i = 0;
    
    while(file1->next != NULL )
    {
        file2 = file1->next;

        while(file2 != NULL)
        {
            NODE** meanList = getMeanList(file1,file2);
            kld = getKLD(file1->first,*meanList);
            kld2 = getKLD(file2->first,*meanList);

            char str[1024];
            
            //formatting the output string
            snprintf(str, sizeof(str), "\"%s\" and \"%s\"",basename(file1->path),basename(file2->path));
            ret[i] = newNode(str);
            ret[i]->prob = ( kld + kld2 ) / 2;
            ret[i]->count = file1->total + file2->total;
            i++;
            file2 = file2->next;

            destroyNodes(*meanList);
            free(meanList);
        }
        file1 = file1->next;
    }


    for(int j = 0;j<i-1;j++)
    {
        ret[j]->next = ret[j+1];
    }

    destroyLists(ds->first);

    return ret;

}

/*
 * Function: report
 * ----------------------------
 *  This function report the results with colors
 * 
 *  *head: points to the first element in the list
 */
void report(NODE* head)
{
    NODE* temp = head;
    double d;
    while(temp!=NULL)
    {
        d = temp->prob;

        if(d >= 0 && d <= 0.1)
            printf(RED "%.3f  %s\n"RESET,d,temp->str);
        else if(d > 0.1 && d <= 0.15)
            printf(YELLOW "%.3f  %s\n"RESET,d,temp->str);
        else if(d > 0.15 && d <= 0.2)
            printf(GREEN "%.3f  %s\n"RESET,d,temp->str);
        else if(d > 0.2 && d <= 0.25)
            printf(CYAN "%.3f  %s\n"RESET,d,temp->str);
        else if(d > 0.25 && d <= 0.3)
            printf(BLUE "%.3f  %s\n"RESET,d,temp->str);
        else
            printf(WHITE "%.3f  %s\n"RESET,d,temp->str);
        
        temp = temp->next;
    }

}

int main(int argc, char *argv[])
{

    
    if(argc != 2)
    {
        printf("Invalid input\n");
        return EXIT_FAILURE; 
    }

    struct dirent *de;  
    DIR *dir = opendir(argv[1]); 
    struct stat buf;

    //check if this dir is accessible
    if (dir == NULL)
    { 
        printf("Error: could not open current directory: %s\n",argv[1]); 
        return EXIT_FAILURE; 
    } 

    
    /* Initializing */
    ARGS* a;
    a = (ARGS*)malloc(sizeof(ARGS));
    a->ds = newDs();
    a->path = strdup(argv[1]);
    strcpy(a->path,argv[1]);




    pthread_t thread;
    pthread_create(&thread,NULL,directoryHandler,(void*)a);
    pthread_join(thread,NULL);

    
    if(a->ds->first == NULL)
    {
        printf("Error:There is no file in the the target directory and its subdirectories!\n");
    }
    else if(a->ds->first->next == NULL)
    {
        printf("Error:There is only one files in the the target directory and its subdirectories!\n");

    }
    else
    {
        sortingList(a->ds);
        NODE** out = getOutList(a->ds);
        sortingNode(out);
        report(*out);
        destroyNodes(*out);
    }
 
    exit(0);
}



//     ./a.out "/Users/aku/Desktop/a"
