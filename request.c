#include "io_helper.h"
#include "request.h"

#define MAXBUF (8192)


//
//	TODO: add code to create and manage the buffer
//

pthread_cond_t empty, fill;
pthread_mutex_t mutex;		//mutual exclusion lock
typedef struct buffer_node_tag
{
	int fd;	//file descreptor
	int fsize;
	char fname[100];
	struct buffer_node_tag *next ;
} buffer_node;

buffer_node *head =NULL, *tail=NULL;

int count=0;

void insert(int fs , char fn[],int fd){

	buffer_node* ptr = (buffer_node* )(malloc(sizeof(buffer_node)));
	if(count==0){
		//create head
		ptr->fsize=fs;
		ptr->fd=fd;
		strcpy(ptr->fname,fn);
		head=ptr;
		tail=ptr;
	}
	else{
		// create new node
		ptr->fsize=fs;
		strcpy(ptr->fname,fn);
		ptr->fd = fd;
		tail->next = ptr;
		tail=ptr;
	}
	count++;
}

buffer_node* delete_fifo(buffer_node* head){
	// deletion on the basis of fifo
	buffer_node* temp = head;
	head=head->next;
	count--;
	return temp;
}

// buffer_node* delete_ssf(buffer_node* ptr, buffer_node* prev){
// 	//delete on the basis of SSF 
// 	buffer_node* temp = ptr;
		
// 	//if ptr is in the middle;
// 	if(ptr->next!=NULL){
// 		prev->next= ptr->next;
// 	}
	
// 	if(ptr->next==NULL){
// 		prev->next=NULL;
// 	}
// 	return ptr;
// }

buffer_node* get_ssf(buffer_node* head){
	//function to get the ssf node from the linkedlist
	if(head==NULL){
		return NULL;
	}
	 int min = -1;
	 buffer_node* first_node=head;
	 buffer_node* ref = head;
	 buffer_node* tmp = head;
	 if(min> tmp->fsize){
	 	  min = tmp->fsize;
           first_node=tmp;
	 }
    while (tmp->next != NULL) {
        if (min > tmp->next->fsize){
            min = tmp->next->fsize;
            first_node=tmp->next;
            ref = tmp;
        }
  
        tmp = tmp->next;
    }
    ref->next = first_node->next;

    return first_node;

}

//
// Sends out HTTP response in case of errors
//
void request_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXBUF], body[MAXBUF];
    
    // Create the body of error message first (have to know its length for header)
    sprintf(body, ""
	    "<!doctype html>\r\n"
	    "<head>\r\n"
	    "  <title>OSTEP WebServer Error</title>\r\n"
	    "</head>\r\n"
	    "<body>\r\n"
	    "  <h2>%s: %s</h2>\r\n" 
	    "  <p>%s: %s</p>\r\n"
	    "</body>\r\n"
	    "</html>\r\n", errnum, shortmsg, longmsg, cause);
    
    // Write out the header information for this response
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    write_or_die(fd, buf, strlen(buf));
    
    sprintf(buf, "Content-Type: text/html\r\n");
    write_or_die(fd, buf, strlen(buf));
    
    sprintf(buf, "Content-Length: %lu\r\n\r\n", strlen(body));
    write_or_die(fd, buf, strlen(buf));
    
    // Write out the body last
    write_or_die(fd, body, strlen(body));
    
    // close the socket connection
    close_or_die(fd);
}

//
// Reads and discards everything up to an empty text line
//
void request_read_headers(int fd) {
    char buf[MAXBUF];
    
    readline_or_die(fd, buf, MAXBUF);
    while (strcmp(buf, "\r\n")) {
		readline_or_die(fd, buf, MAXBUF);
    }
    return;
}

//
// Return 1 if static, 0 if dynamic content (executable file)
// Calculates filename (and cgiargs, for dynamic) from uri
//
int request_parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;
    
    if (!strstr(uri, "cgi")) { 
	// static
	strcpy(cgiargs, "");
	sprintf(filename, ".%s", uri);
	if (uri[strlen(uri)-1] == '/') {
	    strcat(filename, "index.html");
	}
	return 1;
    } else { 
	// dynamic
	ptr = index(uri, '?');
	if (ptr) {
	    strcpy(cgiargs, ptr+1);
	    *ptr = '\0';
	} else {
	    strcpy(cgiargs, "");
	}
	sprintf(filename, ".%s", uri);
	return 0;
    }
}

//
// Fills in the filetype given the filename
//
void request_get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html")) 
		strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif")) 
		strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg")) 
		strcpy(filetype, "image/jpeg");
    else 
		strcpy(filetype, "text/plain");
}

//
// Handles requests for static content
//
void request_serve_static(int fd, char *filename, int filesize) {
    int srcfd;
    char *srcp, filetype[MAXBUF], buf[MAXBUF];
    
    request_get_filetype(filename, filetype);
    srcfd = open_or_die(filename, O_RDONLY, 0);
    
    // Rather than call read() to read the file into memory, 
    // which would require that we allocate a buffer, we memory-map the file
    srcp = mmap_or_die(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    close_or_die(srcfd);
    
    // put together response
    sprintf(buf, ""
	    "HTTP/1.0 200 OK\r\n"
	    "Server: OSTEP WebServer\r\n"
	    "Content-Length: %d\r\n"
	    "Content-Type: %s\r\n\r\n", 
	    filesize, filetype);
       
    write_or_die(fd, buf, strlen(buf));
    
    //  Writes out to the client socket the memory-mapped file 
    write_or_die(fd, srcp, filesize);
    munmap_or_die(srcp, filesize);
}

//
// Fetches the requests from the buffer and handles them (thread logic)
//
void* thread_request_serve_static(void* arg)
{
	// TODO: write code to actualy respond to HTTP requests
	buffer_node *tmp;
	buffer_node *t2=head;
	while(1)
	{
		pthread_mutex_lock(&mutex); 
		while (count == 0)
		pthread_cond_wait(&fill, &mutex); 
	// if scheduling_algo==0 we use fifo , otherwise we use ssf
		if(scheduling_algo==0){
			tmp = delete_fifo(head); 
		}
		if(scheduling_algo==1)
		{
			printf("239");
			 tmp= get_ssf(t2);

			// tmp = delete_ssf(ptr);
		}
		pthread_cond_signal(&empty); 
		pthread_mutex_unlock(&mutex); 
		request_serve_static(tmp->fd,tmp->fname,tmp->fsize);

		printf("Request for %s is removed from the buffer.\n",tmp->fname);
	}
}

//
// Initial handling of the request
//
void request_handle(int fd) {
    int is_static;
    struct stat sbuf;
    char buf[MAXBUF], method[MAXBUF], uri[MAXBUF], version[MAXBUF];
    char filename[MAXBUF], cgiargs[MAXBUF];
    
	// get the request type, file path and HTTP version
    readline_or_die(fd, buf, MAXBUF);
    sscanf(buf, "%s %s %s", method, uri, version);
    printf("method:%s uri:%s version:%s\n", method, uri, version);

	// verify if the request type is GET or not
    if (strcasecmp(method, "GET")) {
		request_error(fd, method, "501", "Not Implemented", "server does not implement this method");
		return;
    }
    request_read_headers(fd);
    
	// check requested content type (static/dynamic)
    is_static = request_parse_uri(uri, filename, cgiargs);
    if(strstr(filename,"..")!=NULL){
      request_error(fd, filename, "403", "Forbidden", "Traversing up in filesystem is not allowed");
      return;
    }
	// get some data regarding the requested file, also check if requested file is present on server
    if (stat(filename, &sbuf) < 0) {
		request_error(fd, filename, "404", "Not found", "server could not find this file");
		return;
    }
    
	// verify if requested content is static
    if (is_static) {
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
			request_error(fd, filename, "403", "Forbidden", "server could not read this file");
			return;
		}
		
		// TODO: write code to add HTTP requests in the buffer based on the scheduling policy

		pthread_mutex_lock(&mutex); 
		while (count ==buffer_max_size) 
			pthread_cond_wait(&empty, &mutex); 
		insert(sbuf.st_size,filename,fd);
		pthread_cond_signal(&fill); 
		pthread_mutex_unlock(&mutex); 
		printf("Request for %s is added to the buffer.\n", filename);


    } else {
		request_error(fd, filename, "501", "Not Implemented", "server does not serve dynamic content request");
    }
}
