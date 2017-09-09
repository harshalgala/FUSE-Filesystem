# define FUSE_USE_VERSION 30

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "ramdisk.h"

Cluster *top;
long lastEmptySpace;
long number_of_Clusters;
char root[2];

char *changePath(const char *path)
{
//This initially clears the memory in the heap for memory allocation
	char *pathValue = NULL;
//Allocates size in form of char unit for(length of path +1)
	pathValue = (char*)malloc(sizeof(char)*(strlen(path)+1));
//Check if path is not NULL
	if(NULL != path)
	{
		strcpy(pathValue,path);
	}
//Check if pathValue is not NULL
	if(NULL != pathValue)
		return pathValue;

	return NULL;
}



Cluster* LocateCluster(long position)
{

	if(number_of_Clusters > position)
		return (Cluster*)(top + position);

	return NULL;
}

//Deletes the a path of the directory i.e shortens it from /home/user/Desktop to home/user/
char *deletePath(char *path)
{
	
	char *parentPath = NULL;

	char *file = NULL;

	int size = 0;
//Allocates size in char units for size 255
	file = (char*) malloc(sizeof(char)*255);

	//Searches last '/'
	file = strrchr(path,'/');
	
	//Check for invalid path
	if(file == NULL)
	{
		return NULL;
	}
	
	//Get the size of parentPath
	size = strlen(path)-strlen(file);
	
	//Shows that parent is the root
	if(size==0)
	{
		return root;
	}

	parentPath = (char*) malloc((size+1)*sizeof(char));

	strncpy(parentPath,path,size);
	
	parentPath[size]='\0';
	
	return parentPath;
}

Cluster* getSubDirtop(char *path)
{
	Cluster *tt;

	tt = LocateCluster(getClusterNumberFromPath(path));

	if(tt != NULL)
	{
		tt = LocateCluster(tt->nextLevel);
		
	}	

	return tt;
}


/***Remove Directory***/
static int ramdisk_rmdir(const char *path)
{
	char *passingPath = NULL;
	passingPath = changePath(path);
	int resultDirectory = -ENOENT;

	if((strlen(Current_Working_Directory)+strlen(passingPath)>250))
	{
		printf("Too Long Absolute Path\n");
		return resultDirectory;
	}

	if((resultDirectory = removeDirectoryErrors(passingPath))<0)
	{
		return resultDirectory;
	}

	return removeDirectory(passingPath);
}

int removeDirectory(char *path)
{
	long parentPosition = 0;
	Cluster *traverser = NULL;
	Cluster *next = NULL;
	Cluster *parent = NULL;
	long temporaryPosition = 0;
	
	parentPosition = getClusterNumberFromPath(deletePath(path));

	parent = LocateCluster(parentPosition);

	//printf("Parent:%s\n",parent->name);

	traverser = LocateCluster(parent->nextLevel);

	//for the cluster in parent's next level is the one we want to remove
	if(strcmp(traverser->name,deleteName(path)) == 0)
	{
		//if there no clusters in the next level, then it will be NULL
		temporaryPosition=traverser->Next;

		traverser->type = 'c';
		traverser->Next = number_of_Clusters + 1;
		traverser->nextLevel = number_of_Clusters + 1;
		strcpy(traverser -> name,"");
		strcpy(traverser -> data,"");

		// if there was a single child
		if(temporaryPosition>number_of_Clusters)
		{
			parent->nextLevel = number_of_Clusters+1;
		}
		else
		{
			parent->nextLevel = temporaryPosition;
		}

		return 0;
	}
	else
	{
		next = LocateCluster(traverser->Next);
		while(next != NULL)
		{
			if(strcmp(next->name,deleteName(path)) == 0)
			{
				traverser->Next = next->Next;

				next->type = 'c';
				next->Next = number_of_Clusters+1;
				next->nextLevel = number_of_Clusters+1;
				next->nextFileBlockNumber = number_of_Clusters+1;
				strcpy(next->name,"");
				strcpy(next->data,"");
				break;
			}

			traverser = next;
			next = LocateCluster(traverser->Next);
		}
		//Should not happen
		if(next==NULL)
		{
			return -ENOENT;
		}
		return 0;
	}
	
}

int removeDirectoryErrors(char *path)
{
	long position = number_of_Clusters + 1;
	Cluster *Node = NULL;

	position = getClusterNumberFromPath(path);

	Node = LocateCluster(position);

	if(NULL == Node)
	{
		return -ENOTDIR;
	}

	if(Node -> nextLevel < number_of_Clusters)
	{
		return -ENOTEMPTY;
	}

	return 0;

}

static int ramdisk_open(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

long getClusterNumberFromPath(char *original_path)
{
	Cluster *traverser;
	char *sub_directory_Name;
	long clusterNumber = 0;
	long temp_clusterNumber = 0;

	sub_directory_Name = (char*) malloc(sizeof(char)*256);
	
	if(sub_directory_Name == NULL)
	{
		return number_of_Clusters + 1;
	}
	
	char *path = (char*) malloc(sizeof(char)*strlen(original_path));
	
	if(path == NULL)
	{
		return number_of_Clusters + 1;
	}
	strcpy(path,original_path);
	
	if(strcmp(path,"/")==0)
		return 0;

	traverser = (Cluster*)LocateCluster(top->nextLevel);

	if(traverser == NULL)
	{
		return number_of_Clusters + 1;
	}

//breaks path in tokens with respect to '/' seperator i.e for the /home/user/abc will get home,user and abc as these tokens
	sub_directory_Name = strtok(path,"/");
	
	temp_clusterNumber = top->nextLevel;
	
	do
	{
		
		while(NULL!=traverser)
		{
			
			clusterNumber=temp_clusterNumber;
			
			if(strcmp(traverser->name,sub_directory_Name)==0)
			{
				temp_clusterNumber=traverser->nextLevel;
				
				traverser=LocateCluster(traverser->nextLevel);

				break;
			}
			else
			{
				
				temp_clusterNumber = traverser->Next;
				
				traverser = LocateCluster(traverser->Next);

				if(traverser == NULL)
				{
					return (number_of_Clusters + 1);
				}
				
			}
	
		}
		
	}while((NULL!=(sub_directory_Name=strtok(NULL,"/")))&&(NULL!=traverser));

	if(NULL != sub_directory_Name)
	{	
		return (number_of_Clusters + 1);
	}
	return clusterNumber;
}

/***Get Attribute***/
static int ramdisk_getattr(const char *path, struct stat *st)
{
	int result = -1;//For returning the value initialized to -1
	long clusterNumber = -1;
	char *passingPath = NULL;
	Cluster *Node;
	size_t size=0;
	Cluster *fileBlock = NULL;

	passingPath = changePath(path);

	if(strlen(Current_Working_Directory)+strlen(passingPath)>250)
	{
		printf("Too Long Absolute Path\n");
		return -ENOENT;
	}

	if(NULL != passingPath)
	{
//Copies 0 to st(Steing part) for (sizeof(struct stat) times)
		memset(st, 0, sizeof(struct stat));
	

//	printf("Inside ramdisk_getattr: path:%s\n",path);
//	st->st_uid = getuid();//Owner of the files and directories. Here it is the same user who mounted the filesystem
//	st->st_gid = getgid();//Owner group of users of files and directories


//getuid---for getting user ID and getgid---for getting group ID
	
//	st->st_atime = time(NULL);//Last access time for the file
//	st->st_mtime = time(NULL);//Last modification time of the file

//Filled both of them with the current time i.e Unix Time

	if(strcmp(path, "/") == 0)
	{
		st->st_mode = S_IFDIR | 0755;//Specifies if the file is a regular file,directory or other
		st->st_nlink = 2;
		result = 0;
//st_mode---specifies the permission bits of that file and st_nlink---specifies the number of hardlinks
	}
	else if((clusterNumber = getClusterNumberFromPath(passingPath))<number_of_Clusters)
	{
		Node = LocateCluster(clusterNumber);

		if(Node->type == 'd')
		{
			st -> st_mode = S_IFDIR | 0755;
		}
		else if(Node->type == 'f')
		{
			st -> st_mode = S_IFREG | 0755;
		}

		st->st_nlink = 1;

	fileBlock = LocateCluster(clusterNumber);

	size += strlen(fileBlock->data);

	while(fileBlock->nextFileBlockNumber < number_of_Clusters)
	{
		fileBlock = LocateCluster(fileBlock->nextFileBlockNumber);

		size+=strlen(fileBlock->data);
	}
	st->st_size = size;
		result = 0;
	}
	else
	{
		result = -ENOENT;
	}
//st_size---specifies size of files in bytes
	}
	return result;
}
/* A hard link allows a user to create two exact files without having to duplicate the data on 
disk. However unlike creating a copy, if you modify the hard link you are in turn modifying 
the original file as well as they both reference the same inode */

long getNextEmpty(void)
{
	long nextEmptySpace;
	Cluster *traverser = NULL;

	nextEmptySpace = lastEmptySpace;

	traverser = LocateCluster(nextEmptySpace);

	while(traverser != NULL)
	{
		if(traverser->type == 'c')
			break;

		nextEmptySpace++;
		nextEmptySpace = nextEmptySpace%(number_of_Clusters-1);

		if(nextEmptySpace == lastEmptySpace)
			return number_of_Clusters+1;

		traverser = LocateCluster(nextEmptySpace);

	}
	lastEmptySpace = nextEmptySpace;


	
	return nextEmptySpace;
}

static int ramdisk_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct  fuse_file_info *fi )
{
	/* typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
				const struct stat *stbuf, off_t off); */
	//*buf---pointer to the buffer which we want to write the entry
	//name---name of the current entry

	(void) offset;
	(void) fi;
	Cluster *temp;
	char *p;

	filler( buffer, ".", NULL, 0 ); // Current Directory
	filler( buffer, "..", NULL, 0 ); // Parent Directory

	p = (char*)path;
	char *passingPath = NULL;
	passingPath = changePath(path);

	if(strlen(Current_Working_Directory)+strlen(passingPath)>250)
	{
		printf("Too Long Absolute Path");
		return -ENOENT;
	}

	temp = LocateCluster(getClusterNumberFromPath(p));

	if(temp != NULL)
	{
		temp = LocateCluster(temp -> nextLevel);
	}

	while(temp != NULL)
	{
		
		filler(buffer,temp->name,NULL,0);

		temp = LocateCluster(temp -> Next);
	}

	return 0;
}

//last parameter is the count of chars from buf already written to my'file'
long forLargeFileBlock(long clusterNumber,const char *buffer, size_t size,off_t offset,long buf_pos_written)
{
	Cluster *fileBlock = NULL;
	Cluster *nextFileBlock = NULL;
	size_t i;
	long nextClusterNum;

	if(buf_pos_written >= size)
	{
		fileBlock -> nextFileBlockNumber = number_of_Clusters + 1;

		return buf_pos_written;
	}
	
	fileBlock=LocateCluster(clusterNumber);

	for(i=offset;i<DATASIZE-1;i++)
	{
		fileBlock->data[i] = buffer[buf_pos_written];
		buf_pos_written++;
		if(buf_pos_written >= size)
			break;
	}
	fileBlock->data[i]='\0';
	
	if(size > buf_pos_written)
	{
		nextClusterNum = getNextEmpty();

		if(nextClusterNum > number_of_Clusters)
		{
			return -ENOMEM;
		}

		fileBlock->nextFileBlockNumber = nextClusterNum;
		nextFileBlock = LocateCluster(nextClusterNum);
		nextFileBlock -> type = 'n';
		nextFileBlock -> nextLevel = number_of_Clusters + 1;
		nextFileBlock -> Next = number_of_Clusters + 1;
		nextFileBlock -> nextFileBlockNumber = number_of_Clusters + 1;

		buf_pos_written = forLargeFileBlock(nextClusterNum,buffer,size,0,buf_pos_written);
	}

	return buf_pos_written;

}

/***Read***/
static int ramdisk_read(const char *path, char *buffer, size_t size,off_t offset, struct fuse_file_info *fi)
{
	//path---path of the file we want to read
	//buffer---we are going to store the chunk which interests the system
	//size---represent the size of the chunk
	//offset---place from where we are going to start reading	

	(void) fi;

	long clusterNumber = number_of_Clusters;
	Cluster *file;
	char *result;
	char *data;
	size_t haveRead = 0;
	int i = 0;
	int empty_counter = 0;

	char *passingPath = NULL;
	passingPath = changePath(path);

	if((strlen(Current_Working_Directory)+strlen(passingPath)>250))
	{
		printf("Too Long Absolute Path\n");
		return -ENOENT;
	}

	memset(buffer,0,size);

	clusterNumber = getClusterNumberFromPath(passingPath);

	if(clusterNumber > number_of_Clusters)
	{
		return -ENOENT;
	}

	//File exists or it seems so
	file = LocateCluster(clusterNumber);

	if(file == NULL)
	{
		return -ENOENT;
	}
	//Do file exsits at Clusters also?

	while(offset > DATASIZE)
	{
		if(file->nextFileBlockNumber < number_of_Clusters)
		{
			file = LocateCluster(file -> nextFileBlockNumber);
			offset -= DATASIZE;
		}
		else
		{
			//No more data in this file
			return 0;
		}
	}

	//Reached the offset i.e from where to start reading
	//for copying till DATASIZE

	if((offset+size) < DATASIZE)
	{
		//offset taken into consideration
		data = (char*)(file -> data + offset);
		result = strncpy(buffer,data,size);
		haveRead = size;
	}
	else
	{
		//copy data from fileBlock
		data = (char*)(file->data +offset);

		result = strncpy(buffer,data,DATASIZE-offset);
		haveRead = DATASIZE-offset;
		//travelling now

		while(haveRead < size)
		{
			file = LocateCluster(file->nextFileBlockNumber);

			if(file == NULL)
			{
				return haveRead;
			}
			//For reading only one new block
			if(size-haveRead < DATASIZE)
			{
				result = strncat(buffer,file->data,size-haveRead);
				haveRead+=(size-haveRead);
				break;
			}
			else
			{
				result = strncat(buffer,file->data,DATASIZE);
				haveRead += DATASIZE;
			}
		}
	}

	if(result == NULL)
	{
		return -ENOENT;
	}
	for(i=0;i<strlen(buffer);i++)
	{
		if(buffer[i] == '\0')
		{
			empty_counter++;
		}
	}

	return haveRead;
}
/***Write***/
static int ramdisk_write(const char *path, const char *buffer, size_t size,off_t offset, struct fuse_file_info *fi)
{
	long clusterNumber = number_of_Clusters;
	Cluster *file;
	int i = offset;
	long count = 0;
	int block_counter = 0;

	char *passingPath = NULL;
	passingPath = changePath(path);

	if((strlen(Current_Working_Directory) + strlen(passingPath)>250))
	{
		printf("Too Long Absolute Path");
		return -ENOENT;
	}

	clusterNumber = getClusterNumberFromPath(passingPath);

	if(clusterNumber > number_of_Clusters)
	{
		return -ENOENT;
	}

	file = LocateCluster(clusterNumber);
	
	if(file == NULL)
	{
		printf("Cluster is Null.\n");
		return -ENOENT;
	}

	if((size+offset) > DATASIZE-1)
	{
		while(offset > DATASIZE-1)
		{
			if(file->nextFileBlockNumber < number_of_Clusters)
			{
				clusterNumber = file->nextFileBlockNumber;
				file = LocateCluster(clusterNumber);
				offset -= DATASIZE;
				block_counter++;
			}
			else
			{
				//printf("\n\nOffset greater than current file size? Offset:%u Total Blocks traversed:%d\n\n",offset,temp_block_counter);
				//THIS FILE DOESN'T CONTAIN THIS MUCH DATA. Is this allowed? Test edge case.
				return 0;
			}
		}

		return forLargeFileBlock(clusterNumber,buffer,size,offset,count);
	}
	else
	{
		
		for(i=offset;i<(offset+size);i++)
		{

			file->data[i] = buffer[count];
			
			count++;
		}
		file->data[i]='\0';	
	}

	return count;
}

/***Unlink***/
static int ramdisk_unlink(const char *path)
{
	long clusterNumber = 0;
	Cluster *file = NULL;
	Cluster *parentCluster = NULL;
	Cluster *traveller = NULL;
	char *passingPath = NULL;
	passingPath = changePath(path);

	if((strlen(Current_Working_Directory) + strlen(passingPath)>250))
	{
		printf("Too Long Absolute Path");
		return -ENOENT;
	}
	//Remove it from parent is the first step
	parentCluster = LocateCluster(getClusterNumberFromPath(deletePath(passingPath)));
	//printf("Parent Pos:%u from path |%s|\n\n\n",getClusterNumberFromPath(deletePath(passingPath)),deletePath(passingPath));

	clusterNumber = getClusterNumberFromPath(passingPath);
	file = LocateCluster(clusterNumber);

	if(parentCluster -> nextLevel == clusterNumber)
	{
		parentCluster -> nextLevel = file -> Next;
	}
	else
	{
		traveller = LocateCluster(parentCluster -> nextLevel);

		while(traveller -> Next < number_of_Clusters)
		{
			if(traveller -> Next == clusterNumber)
			{
				traveller -> Next = file -> Next;
				break;
			}
			traveller = LocateCluster(traveller -> Next);
			if(traveller == NULL)
			{
				printf("Caution:Should never happen\n");
			}
		}
	}

	while(file -> nextFileBlockNumber<number_of_Clusters)
	{
		file->type = 'c';
		clusterNumber = file->nextFileBlockNumber;
		file->nextFileBlockNumber = clusterNumber + 1;
		file = LocateCluster(clusterNumber);
	}

	file->type = 'c';

	return 0;
}

int insertNode(char *path,mode_t mode,char ClusterType)
{
	Cluster *dirToInsertIn;
	Cluster *traverser;
	Cluster *newNode;
	long positionToInsertIn = 0;
	long parentPosition = 0;
	
	parentPosition = getClusterNumberFromPath(deletePath(path));


	if(parentPosition>number_of_Clusters)
	{
		return -ENOTDIR;
	}

	
	positionToInsertIn = getNextEmpty();

	if(positionToInsertIn > number_of_Clusters)
	{
			return -ENOMEM;
	}

	newNode = LocateCluster(positionToInsertIn);

	if(path != NULL)//returns 1 if allowed
	{
		strcpy(newNode->name,deleteName(path));
	}
	else
	{
		return -ENOENT;
	}

	newNode->mode=mode;

	newNode->type=ClusterType;

	newNode->nextLevel = number_of_Clusters + 1;
	newNode->Next = number_of_Clusters + 1;
	newNode->nextFileBlockNumber = number_of_Clusters + 1;

	dirToInsertIn=LocateCluster(parentPosition);

	if(dirToInsertIn==NULL)
	{
		return -ENOTDIR;
	}	

	if(dirToInsertIn->nextLevel==(number_of_Clusters + 1))
	{
		dirToInsertIn->nextLevel=positionToInsertIn;
		
		return 0;
	}

	//need to traverse the children of the parent
	traverser=LocateCluster(dirToInsertIn->nextLevel);

	while(traverser->Next<number_of_Clusters)
	{
		traverser=LocateCluster(traverser->Next);
	}

	traverser->Next = positionToInsertIn;
	
	return 0;
}

int mytruncate(char *path,int size)
{
	Cluster *fileBlock=NULL;
	Cluster *finalFileBlock=NULL;
	long blockIndex = number_of_Clusters + 1;
	int numBlocks=0;
	int currentBlock=0;

	blockIndex = getClusterNumberFromPath(path);
	fileBlock = LocateCluster(blockIndex);

	numBlocks = size/(DATASIZE-1);
	
	while(currentBlock<numBlocks)
	{
		fileBlock=LocateCluster(fileBlock->nextFileBlockNumber);
		if(fileBlock==NULL)
			return 0;

		currentBlock++;
	}

	fileBlock -> data[size%(DATASIZE-1)]='\0';
	finalFileBlock = fileBlock;
	fileBlock = LocateCluster(fileBlock->nextFileBlockNumber);
	finalFileBlock -> nextFileBlockNumber = number_of_Clusters + 1;
	while(NULL!=fileBlock)
	{
		fileBlock->type='c';
		fileBlock=LocateCluster(fileBlock->nextFileBlockNumber);
	}

	return 0;

}

int makeDirectory(char *path, mode_t mode)
{

	return insertNode(path,mode,'d');
}

/***Make Directory***/
static int ramdisk_mkdir(const char *path, mode_t mode)
{
	int result = -1;
	char *passingPath = NULL;

	passingPath = changePath(path);

	if((strlen(Current_Working_Directory)+strlen(passingPath)>250))
	{
		printf("Too Long Absolute Path\n");
		return -ENOENT;
	}

	result = makeDirectory(passingPath, mode);
	
	return result;
}

static int ramdisk_rename(const char *source, const char *destination)
{
	char *source_Pass = NULL;
	char *destination_Pass = NULL;

	source_Pass = changePath(source);
	destination_Pass = changePath(destination);

	if(NULL == source_Pass || NULL == destination_Pass)
	{
		return -ENOMEM;
	}
	return for_rename(source_Pass,destination_Pass);
}

static int ramdisk_truncate(const char *path, off_t size)
{
	char *passingPath = NULL;
	passingPath = changePath(path);

	return mytruncate(passingPath,size);
	
}

static int ramdisk_mknod(const char *path, mode_t mode, dev_t rdev)
{
	char *passingPath = NULL;
	passingPath = changePath(path);

	if((strlen(Current_Working_Directory)+strlen(passingPath)>250))
	{
		printf("Too Long Absolute Path\n");
		return -ENOENT;
	}

	//printf("Before insertNode\n\n");

	return insertNode(passingPath,mode,'f');

}

int for_rename(char *srcPath,char *destPath)
{
	long sourceParentPosition = number_of_Clusters + 1;
	long srcPos = number_of_Clusters + 1;
	long destinationParentPosition = number_of_Clusters + 1;
	Cluster *sourceParent ,*sourceCluster, *traverser,*previous,*destinationParent= NULL;

	//find parent of srcPath
	sourceParentPosition = getClusterNumberFromPath(deletePath(srcPath));
	sourceParent = LocateCluster(sourceParentPosition);

	//traverse through parent till you get ref to srcpath
	//change ref to srcPath->Next
	traverser = LocateCluster(sourceParent->nextLevel);
	srcPos=sourceParent->nextLevel;
	if(strcmp(traverser->name,deleteName(srcPath))==0)
	{
		sourceCluster
=traverser;
		
		sourceParent->nextLevel = sourceCluster
->Next;
	}
	else
	{
		previous = traverser;
		traverser = LocateCluster(previous->Next);
		while(traverser != NULL)
		{
			if(strcmp(traverser->name,deleteName(srcPath))==0)
			{
				sourceCluster
		 = traverser;
				srcPos = previous->Next;
				previous->Next = traverser->Next;
				break;
			}

			previous = traverser;
			traverser = LocateCluster(previous->Next);
		}

		if(srcPos==sourceParent->nextLevel||srcPos>number_of_Clusters)
		{
			//THIS SHOULD NEVER HAPPEN
			return -ENOENT;
		}
	}
	
	//find destPath parent
	destinationParentPosition = getClusterNumberFromPath(deletePath(destPath));
	destinationParent = LocateCluster(destinationParentPosition);
	
	sourceCluster->Next = destinationParent->nextLevel;
	
	destinationParent->nextLevel = srcPos;
	strcpy(sourceCluster->name, deleteName(destPath));

	return 0;
}

char *deleteName(char *path)
{
	char *fileName = strrchr(path, '/');

	if(NULL != fileName)
	{
		if(fileName[0]!='\0')
		{
			return (char*)(fileName+1);
		}
	}
	
	return fileName;
}

static struct fuse_operations ramdisk_operations = {
    .getattr	= ramdisk_getattr,
    .readdir	= ramdisk_readdir,
    .mkdir		= ramdisk_mkdir,
    .read		= ramdisk_read,
    .open 		= ramdisk_open,
    .rmdir 		= ramdisk_rmdir,
    .mknod      = ramdisk_mknod,
    .write 		= ramdisk_write,
    .truncate   = ramdisk_truncate,
    .unlink 	= ramdisk_unlink,
    .rename 	= ramdisk_rename,
};

int main(int argc, char *argv[])
{
	Cluster* t;
	long i;
	long int mem_size = 0;
	char *fileName = NULL;
	char *conversionResult = NULL;
	int initial_argument_count = argc;
	FILE *file;
	top=NULL;
	lastEmptySpace=0;
	number_of_Clusters=0;
	strcpy(root,"/");

	if(argc<3 || argc>4)
	{
		printf("Invalid input. Please enter input in either of the following formats:\n");
		printf("ramdisk <mount_point> <size>\n");
		printf("ramdisk <mount_point> <size> [<filename>]\n");
		return 0;
	}

	if (getcwd(Current_Working_Directory, sizeof(Current_Working_Directory)) == NULL)
   	{
   		printf("Failed to get Current Working Directory\n");
   		return 0;
   	}

   	conversionResult = (char*) (sizeof(char)* strlen(argv[2]));

	mem_size = strtol(argv[2], &conversionResult, 10);   	

	if(conversionResult==argv[2])
	{
		printf("Please enter a valid size\n");
		return 0;
	}

	mem_size = mem_size *(1024*1024);

	number_of_Clusters = (mem_size)/sizeof(Cluster);

	top = (Cluster*) malloc(number_of_Clusters *sizeof(Cluster));

	if(NULL == top)
	{
		printf("Either memory is full or the size given was 0. Please restart the program with another size\n");
	}

	strcpy(top->name,"/");
	top->type = 'd';
	top->Next = number_of_Clusters+1;
	top->nextLevel = number_of_Clusters+1;
	top->nextFileBlockNumber = number_of_Clusters+1;
	strcpy(top->data,"");

	for(i=1;i<number_of_Clusters;i++)
	{
		t = LocateCluster(i);
		t->type = 'c';
		t->Next = number_of_Clusters+1;
		t->nextLevel = number_of_Clusters+1;
		t->nextFileBlockNumber = number_of_Clusters+1;
		strcpy(t->name,"");
		strcpy(t->data,"");
	}

	if(argc==4)
	{
		fileName = (char*) malloc (sizeof(char)*strlen(argv[3]));
		strcpy(fileName,argv[3]);
		argc--;

		file = fopen(fileName, "rb");

		if (file != NULL) 
		{
			fread(top, sizeof(Cluster), number_of_Clusters, file);
    		fclose(file);
		}
}
	argc--;

	fuse_main(argc,argv,&ramdisk_operations,NULL);
	if(initial_argument_count == 4)
	{
		file = fopen(fileName, "wb");

		if (file != NULL) 
		{
			fwrite(top, sizeof(Cluster), number_of_Clusters, file);
    		fclose(file);
		}
	return 0;

}}