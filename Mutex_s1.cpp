/* Centralized server maintains mutual exclusion for accessing file for updating counter! 
   The port number is passed as an argument */
#include <stdio.h>
#include <cstdlib>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <queue>
#include <list>

using namespace std;

#define REQUEST 1
#define RELEASE 2
#define FINISH 3
#define OK 4

void error(const char *msg) 					//function to print error messages
{
	perror(msg);
	exit(1);
}

queue<long> q;							// queue for maintaining the requests
list<long> ll;							// list for maintaining the active clients
int cs = 0;
pthread_mutex_t qlock, cslock;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

void *NewConnection(void *newsockfd) 				// thread function for each client request
{
	int req, n;
	long *ptr = (long *)newsockfd;
	long sock = *ptr;
	if(sock < 0) 
		error("ERROR on accept");

	cout << "Connected to Machine Number: " << sock << endl;
	
	while(1)
	{
		n = read(sock,&req,sizeof(int));		// Reading Client's Request
		if (n < 0) 
			error("ERROR reading from socket");
		
		if(req==REQUEST)				// When the client is requesting access to shared file
		{
			cout << "File Access Request by Machine <" << sock << ">" << endl;
			pthread_mutex_lock(&qlock);
			q.push(sock);				// Queue the client request
			pthread_mutex_unlock(&qlock);
		}
		else if(req==RELEASE)				// When the client releases the file access
		{
			pthread_mutex_lock(&cslock);
			if(cs == 1)				// if critical section is acquired 
			{
				cs = 0;				// Release the critical section
				cout << "File Released by Machine <" << sock << ">" << endl;
				pthread_cond_signal(&cv);	// Signal the conditional variable
			}
			pthread_mutex_unlock(&cslock);
		}
		else if(req==FINISH)				// When client sends a finish message, break from the loop
		{
			cout << "Work Completed. Exiting --> Machine <" << sock << ">" << endl;
			break;
		}
	}

	for(list<long> :: iterator s = ll.begin(); s != ll.end(); )	// Remove the particular client from active client's list
	{
		if(*s == sock)
			s = ll.erase(s);
		else s++;
	}
	
	close(sock);
	pthread_exit(NULL);
}

void *AccessGrant(void *arg) 							// thread function for granting access to Shared File to each client
{
	long req_grant;
	int req, n;
	cout << "Access Granter Started!" << endl;	
	while(1)
	{
		if(ll.empty())							// When there are no active clients, exit the thread
		{
			break;
		}
		pthread_mutex_lock(&cslock);
		if(cs == 1)							// When there is someone in the critical section
		{				
			pthread_cond_wait(&cv, &cslock);			// Wait on the conditional variable
		}
		pthread_mutex_lock(&qlock);
		if(!q.empty())							// When there are requests pending in the queue
		{
			cs = 1;							// Mark the critical section as taken 
			req_grant = q.front();				
			q.pop();						// Remove the request from queue
			req = OK;
			n = send(req_grant,&req,sizeof(int), 0);			// Sending Permission to Access
				if (n < 0) error("ERROR writing to socket");
			cout << "Permission Granted to Machine <" << req_grant << ">" << endl; 
		}
		pthread_mutex_unlock(&qlock);
		pthread_mutex_unlock(&cslock);
	}
	cout << "Access Granter Exiting!" << endl;			
}


int main(int argc, char *argv[])
{
	int t=0;
	long sockfd, newsockfd[10], portno;
	socklen_t clilen;
	
	struct sockaddr_in serv_addr, cli_addr;

	pthread_t threads[10];					// threads for handling client requests
	pthread_t access;

	if (argc < 2) {
		fprintf(stderr,"ERROR, no port provided\n");
		exit(1);
	}

	if(pthread_mutex_init(&qlock, NULL) != 0)		// Initializing mutex object
	{
		cout << "Queue mutex init has failed" << endl;
	}

	if(pthread_mutex_init(&cslock, NULL) != 0)		// Initializing mutex object
	{
		cout << "Conditional Variable mutex init has failed" << endl;
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
	if(sockfd < 0) 
		error("ERROR opening socket");

	bzero((char *) &serv_addr, sizeof(serv_addr));
	portno = atoi(argv[1]);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	
	int reuse = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)	//To reuse socket address in case of crashes and failures
		perror("setsockopt(SO_REUSEADDR) failed");

	#ifdef SO_REUSEPORT
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0) 
		perror("setsockopt(SO_REUSEPORT) failed");
	#endif

	if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR on binding");

	listen(sockfd,10);
	clilen = sizeof(cli_addr);

	cout << "Enter the total number of machines to connect: ";
	cin >> t;

	cout << "I am the Centralized Mutual Exclusion Co-ordinator! " << endl;

	int sock_index = 0;

	for(int i=0; i < t; i++)
	{
		newsockfd[sock_index] = accept(sockfd,(struct sockaddr *) &cli_addr,&clilen);

		ll.push_back(newsockfd[sock_index]);

		pthread_create(&threads[i], NULL, NewConnection, (void *)&newsockfd[sock_index]);
		
		sock_index= sock_index + 1;
	}
	
	pthread_create(&access, NULL, AccessGrant, NULL);

	for(int i=0; i<t; i++)
	{
		int rc = pthread_join(threads[i], NULL);
		if(rc)
		{
			printf("Error in joining thread :\n");
			cout << "Error: " << rc << endl;
			exit(-1);
		}
	
	}

	pthread_mutex_destroy(&qlock);
	pthread_mutex_destroy(&cslock);

	close(sockfd);
	return 0; 
	pthread_exit(NULL);
}

