#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#define CAPACITY 100 //size of hashtable
//global vars
FILE *db;
int fifo_fd;
int cf_fd;
char client_fifo[20];
char *server_fifo;

unsigned long hash_function(char* str) {
	unsigned long hashval = 0;
	for (int j = 0; j < strlen(str)+ 1; j++) {
		hashval += str[j]; //basic approach using chars in str to compute int and taking the int%size
	}
	return hashval % CAPACITY;
}

typedef struct h_item {
	char key[33]; //char* key? 
	long int b_offset;
} h_item;


typedef struct HashTable {
	h_item** items; //array of pointers to 
	int size;
	int count;
} HashTable;
/*
void print_table(HashTable* table)
{
    printf("\nHash Table\n-------------------\n");

    for (int i = 0; i < table->size; i++)
    {
        if (table->items[i])
        {
            printf("Index:%d, Key:%s, Offset:%ld \n", i, table->items[i] -> key, table->items[i]->b_offset);
        }
    }

    printf("-------------------\n\n");
} debugging */

HashTable* create_table (int size) {
	HashTable* table = (HashTable*) malloc (sizeof(HashTable));
	table-> size = size;
	table-> count = 0;
	table-> items = (h_item**) calloc(table->size, sizeof(h_item*));
	for (int i = 0; i< table->size; i++) {
		table->items[i] = NULL;
	}
	return table;
}

void free_table(HashTable* table)
{
    // Frees the table.
    for (int i = 0; i < table->size; i++)
    {
        h_item* item = table->items[i];

        if (item != NULL) {
            free(table->items[i]);
		}
    }
    free(table->items);
    free(table);
}

struct HashTable* table;
//HashTable* table = create_table(CAPACITY);

void handle_sigquit(int sig)
{
    close(cf_fd);
	unlink(client_fifo);
    close(fifo_fd);
    unlink(server_fifo);
	free_table(table);
    exit(0);
}

int main(int argc, char* argv[])
{
	char msg[4];
//	char key[33];
	char value[1025];
	char buf[1061];
	int fifo_fd, n;
	int signo;
	struct stat st;
	memset(buf, 0, sizeof(buf));
	memset(msg, 0, sizeof(msg));
//	memset(key, 0, sizeof(key));
	memset(value, 0, sizeof(value));
 // 	table = create_table(100); //init size to 100
	table = create_table(100);

	if (argc < 3) {
        printf("usage: ./server <file name> <fifo name>");
        exit(-1);
    }
    
	//build table upon start
	db = fopen (argv[1], "r");
	if (db != NULL) {	
		while (fgets(buf,sizeof(buf), db)) {
			h_item* item = (h_item*) malloc(sizeof(h_item));
			item->b_offset = ftell(db) - (strlen(buf) + 1);
			if (item->b_offset < 0) {
				item->b_offset = 0;
			}
			memset(item->key, 0, 33);
			int ch = 0;
			while (buf[ch] != ',') {
				item->key[ch] = buf[ch];
				ch++;
			}
			int idx = hash_function(item->key);
			h_item* curr = table->items[idx];
			if (curr != NULL) {
				idx = (idx + 1) % table->size;
				curr = table->items[idx];
			}
			table->items[idx] = item;
  		  	table->count++;
		}
		memset(buf, 0, sizeof(buf));
		fclose(db);
	}

	server_fifo = argv[2];

	if (stat(server_fifo, &st) != 0) { //if fifo doesnt alr exist, make one
    	mkfifo(server_fifo, 0666);
		printf("creating fifo at: %s\n", server_fifo);
	}
    fifo_fd = open(server_fifo, O_RDWR); //read, write to avoid EOF read

    while(((n = read(fifo_fd, buf, 1061)) != EOF) && n > 0) {
		signal(SIGQUIT, handle_sigquit);
		if (table->size == table->count) {
			printf("Table Full. Exiting\n");
		}
		memcpy(msg, &buf[0], 3);
		if (strcmp(msg, "set") == 0) { //set functionality
			int i = 4;
			int j = 0;
		
			h_item* item = (h_item*) malloc(sizeof(h_item));
			memset(item->key, 0, sizeof(item->key));
			//create the item
			while (buf[i] != ' ') { //init key
				item->key[j] = buf[i];
				j++; i++;
			}
			
			i++; //skip over space
			j = 0;

			while (i < n) { // init value
				value[j] = buf[i];
				j++; i++;
			}

			int index = hash_function(item->key);
            h_item* curr_item = table->items[index];
			db = fopen (argv[1], "a");
            item->b_offset = ftell(db); //init offset
			
			if (curr_item == NULL) { //(1) key dne
				table->items[index] = item;
				table->count++;
				fprintf (db,"%s,%s\n",item->key,value);
            	fclose(db);
			} 
			else if (strcmp(curr_item->key, item->key) == 0) { //(2) update value				
				curr_item->b_offset = item->b_offset;
				fprintf (db,"%s,%s\n",item->key,value);
            	fclose(db);
				free(item);
			}
			else { //!null, not same key == collision
				while (curr_item != NULL) { //linear probing
					index = (index + 1) % table->size;
					curr_item = table->items[index];
				}
				table->items[index] = item;
				table->count++;
				fprintf (db,"%s,%s\n",item->key,value);
            	fclose(db);
			}
			//else { //(3) collision handling
		//	} */
		}

		if (strcmp(msg, "get") == 0) { //get functionality
			int i = 4;
			int j = 0;

			char pid[6];
			memset(pid, 0, sizeof(pid));
			while (buf[i] != ' ') { //init pid
                pid[j] = buf[i];
                j++; i++;
            }

			i++; //skip over space
			j = 0;
		
            char key[33];
			memset(key, 0, sizeof(key));
			while (i < n) { //init key 
				key[j] = buf[i];
				j++; i++;
			}
			memset(value, 0, sizeof(value));
			int index = hash_function(key);
			h_item* item = table->items[index];
			if (item == NULL) {
                sprintf(value, "Key %s does not exist.", key);
	           	free(item);
			}
			
			while (item != NULL && strcmp(item->key,key) != 0) {
				index = (index + 1) % table->size;
                item = table->items[index];
                if (item == NULL) {
                    sprintf(value, "Key %s does not exist.", key);
                    free(item);
					break;
				}
            }

			if (item != NULL && strcmp(item->key, key) == 0) {
                db = fopen (argv[1], "rb+");
                fseek(db,item->b_offset,SEEK_SET);
                char c; int count = 0;
                while (fread(&c,1,1,db) && c!= ',') {
                    count++;
                }
                count = 0;
                while (fread(&c,1,1,db) && c!= '\n') {
                    value[count] = c;
                    count++;
                }
                fclose(db);
            }	
			//client-specific fifo
			memset(client_fifo, 0, sizeof(client_fifo));
			sprintf(client_fifo,"client:%s",pid); //client:XXXXX where XXXXX is clients PID
			mkfifo(client_fifo ,0666);
			printf("creating fifo at: %s\n", client_fifo);
			cf_fd = open(client_fifo, O_WRONLY);
			write(cf_fd, value, strlen(value)); //temp, write VAL to fifo for client to read (replace w val)
			write(cf_fd, "\0",1);
//			close (cf_fd);
			memset(pid, 0, sizeof(pid));	
			memset(client_fifo, 0, sizeof(client_fifo));
 		}
		memset(msg, 0, sizeof(msg));
		memset(buf, 0, n);
		memset(value, 0, sizeof(value));
	//	free(table->items[index]);
    }

    close(fifo_fd); 
    unlink(server_fifo);
	return 0;
}
