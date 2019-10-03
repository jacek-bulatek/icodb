#include "icodb.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>

#define PROJ_ID 255
#define SHARED_MEMORY_SIZE sizeof(struct sharedMemory)


localDB_t local_db = { 0 };

extern int errno;

typedef struct sharedMemory								// used for all data stored on HD
{
    pthread_mutex_t		dbMutex;						// shared mutex for database synchronization
    pthread_mutexattr_t dbAttr;							// its attributes
    pthread_mutex_t		shmMutex;						// shared mutex for shared memory synchronization
    pthread_mutexattr_t shmAttr;						// its attributes
    char				changedRecords[RECORDS_NO];		// array of changed records set up on save char type choosed since it meets current demand
    pid_t				pids[MAX_USERS];				// list of pid numbers of processes currently using DB
}sharedMemory;

struct shmid_ds* shmDsPtr = NULL;
struct sharedMemory* shmptr = NULL;
int shmId = 0;

int icodb_init() __attribute__((constructor));
void icodb_destroy();
void sigusrHandler(int sigNo);


int icodb_read(int mode)
{
    switch(mode)
    {
        case READ_ALL:
        {
            pthread_mutex_lock(&(shmptr->dbMutex));

            int fd;

            fd = open(DB_PATH, O_RDONLY);
            if (fd < 1)
			{
				return ERR_OPEN_DB;
            }

            int read_bytes = 0;

            do
            {
            	read_bytes += read(fd, ((void *) &local_db) + read_bytes, sizeof(localDB_t) - read_bytes);
            } while (sizeof(localDB_t) - read_bytes > 0);

            close(fd);
            pthread_mutex_unlock(&(shmptr->dbMutex));

            for (int i = 0; i < RECORDS_NO; i++)
            {
            	local_db.records[i].modified = 0;
            }
        }
        break;
        case READ_CHANGED:                                                  // READ_CHANGED is actually equal to SIGINT
        {
            int			fd;
            int			read_bytes = 0;
            record_t	record = { 0 };
            int			dummy;

        	pthread_mutex_lock(&(shmptr->dbMutex));

            fd = open(DB_PATH, O_RDONLY);
            if (fd < 1)
			{
				return ERR_OPEN_DB;
            }

            // read recordsNo
            read(fd, (void *) &dummy, sizeof(int));

            for (int i = 0; i < RECORDS_NO; i++)
            {
            	read_bytes = 0;

				do
				{
					read_bytes += read(fd, ((void *) &record) + read_bytes, sizeof(record_t) - read_bytes);
				} while (sizeof(record_t) - read_bytes > 0);

				if (record.modified)
				{
					strcpy(local_db.records[i].data, record.data);
					local_db.records[i].modified = 0;
				}
            }

            close(fd);
            pthread_mutex_unlock(&(shmptr->dbMutex));

            for (int i = 0; i < RECORDS_NO; i++)
            {
            	local_db.records[i].modified = 0;
            }
        }
        break;
    }

    return ERR_OK;
}

int icodb_save()
{
    char		temp_changed_records = 0;
    int			fd = 0;

    pthread_mutex_lock(&(shmptr->dbMutex));

    fd = open(DB_PATH, O_WRONLY);
    if (fd < 1)
	{
		return ERR_OPEN_DB;
    }

    write(fd, &local_db, sizeof(local_db));

    pthread_mutex_lock(&(shmptr->shmMutex));
    memset(shmptr->changedRecords, 0, RECORDS_NO);

    //for(int i = 0; i < RECORDS_NO; i++)
      //  temp_changed_records |= (local_db.records[i] == last_read_db.records[i] ? 0 : 1) << i;

    //shmptr->changedRecords = temp_changed_records;                          // Set flags for changed records
    for(int i = 0; i < MAX_USERS; i++)
    {
        if(shmptr->pids[i] != getpid() && shmptr->pids[i] != 0)
	{
            kill(shmptr->pids[i], SIGUSR1);                                  // Notify other users about recent change
	    printf("pid[%d]: %d\n", i, shmptr->pids[i]);
	}
    }
    pthread_mutex_unlock(&(shmptr->shmMutex));
    pthread_mutex_unlock(&(shmptr->dbMutex));

    return ERR_OK;
}

int icodb_modify(int recordNo, const char *data)
{
	if (recordNo >= RECORDS_NO)
	{
		return ERR_BAD_PARAM;
	}

	if (strlen(data) > RECORD_LEN)
	{
		return ERR_BAD_PARAM;
	}

	strcpy(local_db.records[recordNo].data, data);
	local_db.records[recordNo].modified = 1;

	return ERR_OK;
}

int icodb_init()
{
	int 	i = 0;
	int 	fd = 0;

	// init shared memory
	shmDsPtr = malloc(sizeof(struct shmid_ds));
	if(shmDsPtr == NULL)
	{
		return ERR_SHMEM_ALLOC;
	}
	key_t shmKey = ftok(DB_PATH, PROJ_ID);                                                  // Create common shared memory key
	shmId = shmget(shmKey, SHARED_MEMORY_SIZE, IPC_CREAT | IPC_EXCL | 0b111111111);         // Retrieve shared memory identifier
	if (shmId < 0)
	{
		if (errno == EEXIST)
		{
			shmId = shmget(shmKey, SHARED_MEMORY_SIZE, 0);
			shmptr = shmat(shmId, NULL, 0);                                                 // attach to shared memory
			goto shm_inited;                                                                // if shared memory segment already exists proceed
		}
		else
		{
			return ERR_SHMEM_ALLOC;
		}
	}
	shmptr = shmat(shmId, NULL, 0);
	pthread_mutexattr_init(&(shmptr->dbAttr));                                              // if shared memory segment does not exist
	pthread_mutexattr_setpshared(&(shmptr->dbAttr), PTHREAD_PROCESS_SHARED);                // initialize it
	pthread_mutexattr_settype(&(shmptr->dbAttr), PTHREAD_MUTEX_NORMAL);
	pthread_mutex_init(&(shmptr->dbMutex), &(shmptr->dbAttr));

	pthread_mutexattr_init(&(shmptr->shmAttr));
	pthread_mutexattr_setpshared(&(shmptr->shmAttr), PTHREAD_PROCESS_SHARED);
	pthread_mutexattr_settype(&(shmptr->shmAttr), PTHREAD_MUTEX_NORMAL);
	printf("pthread_init: %d\n", pthread_mutex_init(&(shmptr->shmMutex), &(shmptr->shmAttr)));

shm_inited:
    // allocate memory for local_db
    /*if(((local_db.records = malloc(sizeof(int)*RECORDS_NO)) == NULL) || ((last_read_db.records = malloc(sizeof(int)*RECORDS_NO)) == NULL))
    {
        destroyLocalDB();
        dbInitRC = LOCAL_DB_ALLOC_FAIL;
	return;
    }*/

    // check if user limit isn't reached
	pthread_mutex_lock(&(shmptr->shmMutex));
	shmctl(shmId, IPC_STAT, shmDsPtr);
	if(shmDsPtr->shm_nattch > MAX_USERS)
	{
		pthread_mutex_unlock(&(shmptr->shmMutex));
		icodb_destroy();
		return ERR_TOO_MANY_USERS;
	}
	else
	{
		shmptr->pids[shmDsPtr->shm_nattch - 1] = getpid();
	}
    pthread_mutex_unlock(&(shmptr->shmMutex));

    // load shared database
    pthread_mutex_lock(&(shmptr->dbMutex));

    fd = open(DB_PATH, O_RDONLY);
    if (fd == -1)
    {
    	fd = open(DB_PATH, O_WRONLY | O_CREAT);
    	if ( fd == -1 )
    	{
    		icodb_destroy();
	        return ERR_OPEN_DB;
    	}

    	write(fd, &local_db, sizeof(local_db));
    	close(fd);
    }

    fd = open(DB_PATH, O_RDONLY);
    if ( fd == -1 )
	{
		icodb_destroy();
		return ERR_OPEN_DB;
	}

    if ((i = read(fd, &local_db, sizeof(localDB_t)))== 0)
    {
    	icodb_destroy();
    	return ERR_EMPTY_DB;
    }
    else if (i < 0)
    {
    	icodb_destroy();
		return ERR_READ_DB;
    }
    else
    {
        for (int j = i; i != 0; i = read(fd, (char*)&(local_db)+j, sizeof(localDB_t)), j+=i);
    }
    close(fd);
    pthread_mutex_unlock(&(shmptr->dbMutex));
    signal(SIGUSR1, sigusrHandler);                                           // readSharedDB will handle the 'changed' read notification

    // register for release of resources at exit
    //atexit(icodb_destroy);

    return ERR_OK;
}

void icodb_destroy()
{
	pthread_mutex_lock(&(shmptr->shmMutex));
    for (int i = 0; i < MAX_USERS; i++)
    {
        if(shmptr->pids[i] == getpid())			// Clear pid entry
        {
        	shmptr->pids[i] = 0;
        }
    }

    shmctl(shmId, IPC_STAT, shmDsPtr);
    if (shmDsPtr->shm_nattch <= 1)
    {
		free(shmDsPtr);
		pthread_mutex_unlock(&(shmptr->shmMutex));
        shmdt(shmptr);                                                          // detach from shared memory
        shmptr=0;
		shmctl(shmId, IPC_RMID, NULL);                                          // Mark segment to be destroyed if there are no more users
    }
    else
    {
        pthread_mutex_unlock(&(shmptr->shmMutex));
        shmdt(shmptr);
        shmptr=0;
    }
    return;
}

void sigusrHandler(int sigNo)
{
	icodb_read(READ_CHANGED);
}
