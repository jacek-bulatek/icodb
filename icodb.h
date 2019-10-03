#ifndef ICODB_H
#define ICODB_H


#include <sys/types.h>
#include <signal.h>


/**
 * DB configuration constants
 */
#define DB_PATH		"./database"
#define RECORDS_NO	8
#define MAX_USERS	8
#define RECORD_LEN	30


/**
 * Read modes
 */
#define READ_ALL        1
#define READ_CHANGED    2


/**
 * Error codes
 */
#define ERR_OK					0
#define ERR_BAD_PARAM			1
#define ERR_SHMEM_ALLOC			2
#define ERR_OPEN_DB				3
#define ERR_ALLOC				4
#define ERR_TOO_MANY_USERS		5
#define ERR_READ_DB				6


/**
 * Single record representation
 */
typedef struct record_t
{
	char		modified;
	char		data[RECORD_LEN];
} record_t;


/**
 * Database structure
 */
typedef struct localDB_t
{
    int			recordsNo;
    record_t	records[RECORDS_NO];
} localDB_t;


/**
 * Local copy of database
 */
extern localDB_t local_db;


/**
 * Reads data from file
 */
int icodb_read(int);

/**
 * Saves data to file
 */
int icodb_save();

/**
 * Modifies single record in local copy of database
 *
 * @param recordNo	Number od modifying record in database
 * @param data	 	New data to store in record
 */
int icodb_modify(int recordNo, const char *data);

/**
 * Remove PID entry,
 * Detach from shared memory
 * and mark it removeable
 * if you're last to use it
 */
void icodb_destroy();

#endif// ICODB_H
