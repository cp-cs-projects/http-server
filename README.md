# Http Server
Implemented a simplified http server to interact with a client (telnet, curl, etc) and handle client requests  

run instructions: ./httpd [fifo name] [port number] , send client requests  
for the key-value store functions, also start ./kvstore [same fifo name]  

note: this project includes two folders to try this with. An empty folder “empty” and a test folder “fs”  

**ACTION:**  
Programmed the http requests GET, PUT, and HEAD | Implemented a GET and PUT to interact with a key-value hash table | Programmed HTTP error response codes: 400, 401, 403, 404, 500, 501. 

**TAKEAWAYS:**  
* implemented an O(1) data structure in C
* practiced client-server communication and using sockets for TCP
* gained knowledge on http requests and responses
