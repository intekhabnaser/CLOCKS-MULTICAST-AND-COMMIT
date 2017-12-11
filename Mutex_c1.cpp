/*
Client queries the centralized server for accessing the shared file
*/
#include <stdio.h>
#include <cstdlib>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>

#define REQUEST 1
#define RELEASE 2
#define FINISH 3

using namespace std;

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

int main(int argc, char *argv[])
{

	long sockfd, portno, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	int req;
	char buffer[256];
	
	if(argc < 2) {
		fprintf(stderr,"usage %s port\n", argv[0]);
		exit(0);
	}
	
	portno = atoi(argv[1]);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
	if(sockfd < 0) 
		error("ERROR opening socket");
	
	server = gethostbyname("localhost");
	
	if(server == NULL) {
		fprintf(stderr,"ERROR, no such host\n");
		exit(0);
	}
	
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(portno);
	
	if(connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
		error("ERROR connecting");
	
	int a = 0;

	while(a < 5)
	{
		req = REQUEST;
		n = write(sockfd,&req,sizeof(int));				// Sending Request to Access Shared File
			if (n < 0) error("ERROR writing to socket");
		cout << "Trying to Access Shared File!" << endl; 
		
		n = read(sockfd,&req,sizeof(int));				// Reading Centralized Server's Response
		if (n < 0) 
			error("ERROR reading from socket");

		cout << "Access Granted by for File!" << endl;			// Printing Server's Response
	
		ifstream shared_file("Counter.txt");
	
		string line;
		int cnt;

		if(shared_file.is_open())
		{
			getline(shared_file, line);				//Reading file by line and storing the values
			istringstream ss(line);
			ss >> cnt;
			shared_file.close();
		}
		else cout << "Unable to open file"; 
	
		sleep(1);
	
		cout << "Initial Counter Value: " << cnt << endl;
		cnt++;

		ofstream my_shared_file("Counter.txt");
		if(my_shared_file.is_open())
		{
			my_shared_file << cnt;				// Updating file with incremented counter value
			my_shared_file.close();
		}
		else cout << "Unable to open file";
	
		ifstream shared_file1("Counter.txt");
	
		if(shared_file1.is_open())
		{
			getline(shared_file1, line);			//Reading file by line and storing the values
			istringstream ss1(line);
			ss1 >> cnt;
			shared_file1.close();
		}
		else cout << "Unable to open file"; 
		cout << "Updated Counter Value: " << cnt << endl;
		req = RELEASE;
		n = write(sockfd,&req,sizeof(int));			// Sending Message to Release Shared File
		if (n < 0) error("ERROR writing to socket");

		cout << "Released File Access!\n" << endl;

		a++;
	}
	req = FINISH;
	n = write(sockfd,&req,sizeof(int));				// Sending Request to Access Shared File
	cout << "Work Finished. Closing Connection!" << endl;
	if (n < 0) error("ERROR writing to socket");


	close(sockfd);							//Close the client socket and terminate the connection
	return 0;
}

