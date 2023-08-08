all: httpd kvstore

httpd: httpd.c
	gcc -o httpd httpd.c -g3

kvstore: kvstore.c
	gcc -o kvstore kvstore.c -g3
