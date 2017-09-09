#define DATASIZE 4096

typedef struct Cluster
{
	char name[256], type, data[DATASIZE];
	long Next, nextLevel, nextFileBlockNumber;
	mode_t mode;
}Cluster;
//d for directory, f for file, n-next File block(continued file block), c for clear,empty,NULL----> For mode
char Current_Working_Directory[1000];

Cluster* LocateCluster(long position);
int insertNode(char *path,mode_t mode,char ClusterType);
long getNextEmpty(void);
char *deletePath(char *path);
char *changePath(const char *);
int makeDirectory(char *path, mode_t mode);
int removeDirectory(char *path);
char *deleteName(char *path);
long getClusterNumberFromPath(char *og_path);
long forLargeFileBlock(long clusterNum,const char *buf, size_t size,off_t offset,long buf_pos_written);
Cluster* getSubDirtop(char *path);
int removeDirectoryErrors(char *path);
int for_rename(char *srcPath,char *destPath);
int mytruncate(char *path,int size);