#include "icodb.h"
#include <stdio.h>

void menuRead();
void menuModify();
void menuPrint();


char whiteDummy = 0;

int main()
{
    char menu_choice;
    int err;
    while(1)
	{
	    printf("Select action:\n");
	    printf("   1) Read database\n");
	    printf("   2) Save database\n");
	    printf("   3) Modify database\n");
	    printf("   4) Print database\n");
	    printf("else) Quit\n");
	    scanf("%c", &menu_choice);
	    scanf("%c", &whiteDummy);
	    printf("\n");
	    
	    switch(menu_choice)
	    {
		case '1':
		    menuRead();
		    break;
		case '2':
		    if ((err = icodb_save()) != ERR_OK)
		    {
		    	printf("Save failed with return code: %d\n", err);
		    }
		    break;
		case '3':
		    menuModify();
		    break;
		case '4':
		    menuPrint();
		    break;
		default:
		    return 0;	    
	    }
	}
}

void menuRead()
{
    char menu_choice;
    int err;    
    printf("Select mode:\n");
    printf("1) READ_ALL\n");
    printf("2) READ_CHANGED\n");
    scanf("%c", &menu_choice);
    scanf("%c", &whiteDummy);
    printf("\n");
    switch(menu_choice)
    {
	case '1':
	    if ((err = icodb_read(READ_ALL)) != ERR_OK)
	    {
	    	printf("Read failed with return code: %d\n", err);
	    }
	    break;
	case '2':
	    if ((err = icodb_read(READ_CHANGED)) != ERR_OK)
	    {
	    	printf("Read failed with return code: %d\n", err);
	    }
	    break;
	default:
	    printf("Bad parameter!\n");
    }
}

void menuModify()
{
    int err;
    int recordNo;
    char data[30];
    printf("Enter record number: ");
    scanf("%d", &recordNo);
    scanf("%c", &whiteDummy);
    printf("\nEnter data: ");
    scanf("%s", &data[0]);
    scanf("%c", &whiteDummy);
    printf("\n");
    if((err = icodb_modify(recordNo, data)) != ERR_OK)
	printf("Modify failed with return code: %d\n", err);
}

void menuPrint()
{
	for (int i = 0; i < RECORDS_NO; i++)
	{
		printf("[%d] %s\n", i, local_db.records[i].data);
	}
}
