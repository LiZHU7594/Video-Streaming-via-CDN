#include <stdio.h>
#include <string.h>   //strlen  
#include <string>
#include <stdlib.h>  
#include <errno.h>  
#include <unistd.h>   //close  
#include <arpa/inet.h>    //close  
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <iostream>
#include <vector>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros 
#include <chrono>
#include <iomanip>
#include <fstream>
#include <netdb.h>
#include <time.h>
#include <cstddef> 
#include "DNSclient.h"
using namespace std;

static const int MESSAGE_SIZE = 4097;
#define TRUE   1  
#define FALSE  0 
#define SER_PORT 80 

//sort function for bitrate vector
int vectorSort(vector<int> * v){
	int temp;
	for(int i=0;i<v->size();i++){
		for(int j=0;j<v->size()-1;j++){
			if((*v)[j]>(*v)[j+1]){
				temp = (*v)[j+1];
				(*v)[j+1] = (*v)[j];
				(*v)[j] = temp;
			}
		}
	}
	return 0;
}

int HttpProxy(int listen_port, char * server_ip, float alpha, char *log_path){
    int opt = TRUE;   
    std::ofstream myfile;
    int master_socket , addrlen , new_socket , client_socket[30] , server_socket[30] , 
          max_clients = 30 , activity, i , j , k,  valread , sd , serverfd , rate, ifTime = 0;   
    int max_sd , byteSent = 0, rateStart, rateEnd,chosenBitrate = 10;
    int noInput = 0, byteReceived = 0 , buffer_size = 0;  

    char msg , *ptr = NULL , *bufferPtr = NULL , headerBuffer[1000]  ,*ips[30],
            target[] = "/vod/big_buck_bunny.f4m", endOfHeader[] = "\r\n\r\n";
            
    struct sockaddr_in address;   
    double throughputs[30], throughput, currentThroughput = 0;
    
    string s,length , xml , bitrateString, chunkName;
    unsigned first, last, pos; 
    vector<int> bitrates;

    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point now ;
    std::chrono::duration<double> d;

	//set of socket descriptors
	// A socket is an abstraction of a communication endpoint. 
	//Just as they would use file descriptors to access a file, applications use socket descriptors to access sockets.
	fd_set readfds; 

	//initilaize all cilent_socket[] and server_socket to 0 so not checked
    for (i = 0; i < max_clients; i++){   
        client_socket[i] = 0;  
        server_socket[i] = 0;
        throughputs[i] = 0; 
        ips[i] = NULL;
    }   

	//create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0){   
        perror("socket failed");   
        exit(EXIT_FAILURE);   
    }   

	//set master socket to allow multiple connections
	//this is just a good habit, it will work without this
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ){   
        perror("setsockopt");   
        exit(EXIT_FAILURE);   
    }   

	//type of socket created
    address.sin_family = AF_INET;   
    address.sin_addr.s_addr = INADDR_ANY;   
    address.sin_port = htons( listen_port );   

    if (bind(master_socket, (sockaddr *)&address, sizeof(address))<0){   
        perror("bind failed");   
        exit(EXIT_FAILURE);   
    }   
    printf("Listener on port %d \n", listen_port);   

	//try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0){   
        perror("listen");   
        exit(EXIT_FAILURE);   
    }   

	//accept the incoming connection
    addrlen = sizeof(address);   
    puts("Waiting for connections ...");  

	while(TRUE){
		//clear the socket set
        FD_ZERO(&readfds);   

		//add master socket to set
		FD_SET(master_socket, &readfds);
		max_sd = master_socket;

		//add child sockets to set
		for(i=0;i<max_clients;i++){
			//socket descriptor
			sd = client_socket[i];

			//if valid socket descriptor then add it to read list
			if(sd>0)
				FD_SET(sd,&readfds);

			//highest file descriptor number, need it for the select function
			if(sd>max_sd)
				max_sd = sd;
		}

		//wait for an activity of one of the sockets, timeout is NULL
		//so wait indefinitely
        activity = select(FD_SETSIZE , &readfds , NULL , NULL , NULL); 

		if((activity < 0) && (errno!=EINTR)){
			printf("select error");
		}

		//if something happened on the master socket
		//then it is an incoming connection
		if (FD_ISSET(master_socket, &readfds)){
			//Accepts a connection on socket. 
			//Returns another socket representing client. Accepts
            if ((new_socket = accept(master_socket,(sockaddr *)&address, (socklen_t*)&addrlen))<0){   
                perror("accept");   
                exit(EXIT_FAILURE);   
            }   

			//inform user of socket number - used in send and receive commands
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs (address.sin_port));   

            struct sockaddr_in server;
            server.sin_family = AF_INET;
            server.sin_port = htons(SER_PORT);
            struct hostent* host = gethostbyname(server_ip);
            if (host == nullptr) {
                fprintf(stderr, "%s: unknown host\n", server_ip);
                return -1;
            }
            memcpy(&server.sin_addr, host->h_addr, host->h_length); 
			//create server socket to communicate with server
            serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

			//if there are errors, socket return -1
            if(serverfd < 0){
                perror("ERROR opening socket");        
                return -1;
            }

			//connect to web server
            if(connect(serverfd, (sockaddr*) &server, sizeof(server))<0){
                perror("ERROR connecting stream socket\n");
                return -1;
            }
            printf("connected to web server, forwarding message...\n");

			//add new sockets to array of sockets
			for(int i=0;i<max_clients;i++){
				if(client_socket[i]==0){
					client_socket[i] = new_socket;
					printf("Adding to list of sockets as %d\n",i);
					server_socket[i] = serverfd;
					ips[i] = inet_ntoa(address.sin_addr);
					break;
				}
			}
		}

		//else some IO operation on some other socket
		for(k=0;k<max_clients;k++){
			sd = client_socket[k];
			serverfd = server_socket[k];
			throughput = throughputs[k];

			if(FD_ISSET(sd,&readfds)&&sd>0){

				//receive message from client
				memset(headerBuffer,0,sizeof(headerBuffer));

				i = 0;
				noInput = 0;
				ptr = NULL;
				while(ptr==NULL){
					byteReceived = read(sd,&msg,1);
					if(byteReceived<0){
						perror("receive");
					}else if(byteReceived==0){
						noInput = 1;
						break;
					}
					headerBuffer[i]=msg;
					i++;
					ptr=strstr(headerBuffer,endOfHeader);
				}
				if(noInput==1){
					close(serverfd);
					close(sd);
					client_socket[k]=0;
					server_socket[k]=0;
					throughputs[k]=0;
				}

				//parse out the get content and see if it is big_buck_bunny.f4m
				string s = string(headerBuffer);
				pos = s.find("/vod/big_buck_bunny.f4m");

				//get xml. find bitrates amd modity initial header
				if(pos!=-1&&(bitrates.size()==0)){
					//modify the header to no_list
					s.replace(s.find(".f4m"),0,"_nolist");
					char tempHeader[s.size()+1];
					strcpy(tempHeader,s.c_str());

					//go get the available bitrates from server and ask for no-list
					byteSent = send(serverfd,headerBuffer,i,0);
					//read from web buffer
					memset(headerBuffer,0,1000);
					j=0;
					noInput = 0;
					ptr=NULL;
					while(ptr==NULL){
						byteReceived=read(serverfd,&msg,1);
						if(byteReceived<0){
							perror("receive");
						}else if(byteReceived==0){
							noInput=1;
							break;
						}
						headerBuffer[j]=msg;
						j++;
						ptr=strstr(headerBuffer,endOfHeader);
					}

					s=string(headerBuffer);
					first=s.find("Content-Length: ");
					last=s.find("Keep-Alive");
					length = s.substr(first+16,last-first);
					int contentLength = atoi(length.c_str());

					//dynamic buffer
					if(buffer_size<contentLength){
						char contentBuffer[contentLength*2];
						bufferPtr = contentBuffer;
						buffer_size = contentLength*2;
					}

					byteReceived = recv(serverfd,bufferPtr, contentLength, MSG_WAITALL);

					xml=string(bufferPtr);
					rateStart=-1;
					//gothrough all bitrates
					while((rateStart=xml.find("bitrate=\"",rateStart+1))!=-1){
						rateEnd = xml.find("\"",rateStart+9);
						bitrateString = xml.substr(rateStart+9,rateEnd-rateStart-9);
						rate=atoi(bitrateString.c_str());
						bitrates.push_back(rate);
					}
					vectorSort(&bitrates);
					memset(headerBuffer,0,sizeof(headerBuffer));
					strcpy(headerBuffer,tempHeader);
					//increase length of header due to no list
					i = i+7;
				}

				//do bitrate modification
				s=string(headerBuffer);
				pos = s.find("Seg");
				ifTime = 0;
				if(pos!=-1){
					ifTime=1;
					//if throughput is not calculated yet, use the minimum bitrate
					if(throughput==0){
						chosenBitrate = bitrates[0];
					}else{
						for(int r:bitrates){
							if(throughput>=(double)r*1.5){
								chosenBitrate = r;
							}else{
								break;
							}
						}
					}
					//modify the header;
					rateStart = s.find("vod/");
					s.replace(rateStart+4,pos-rateStart-4,to_string(chosenBitrate));
					i=s.length();
					memset(headerBuffer,0,sizeof(headerBuffer));
					strcpy(headerBuffer,s.c_str());
				}

				start = chrono::steady_clock::now();

				//finding chunkname
				s=string(headerBuffer);
                first = s.find("GET ");
                last = s.find(" HTTP/1");
                chunkName = s.substr(first+4,last-first-4);

                byteSent = send(serverfd,headerBuffer,i,0);

                //read from web server
                memset(headerBuffer,0,1000);
                i=0;
                noInput=0;
                ptr=NULL;
                while(ptr == NULL){
                    byteReceived = read(serverfd,&msg,1);
                    if(byteReceived < 0){
                        perror("receive");
                    }
                    else if(byteReceived == 0){
                        noInput = 1;
                        break;
                    }
                    headerBuffer[i] = msg;
                    i++;
                    ptr = strstr(headerBuffer,endOfHeader);   
                }

                s = string(headerBuffer);
                first = s.find("Content-Length: ");
                last = s.find("Keep-Alive");
                length = s.substr(first+16,last-first);
                int contentlength = atoi(length.c_str());
                printf("length = %d\n",contentlength);
                
                //dynamic buffer 
                if(buffer_size < contentlength){
                    char contentBuffer[contentlength*2];
                    bufferPtr = contentBuffer;
                    buffer_size = contentlength*2;
                }

                byteReceived = recv(serverfd,bufferPtr,contentlength,MSG_WAITALL);

                now = chrono::steady_clock::now();
                d = now-start;
                currentThroughput = ((double)contentlength*8.0/(1000.0*d.count()));

                if(throughput==0){
                	throughput = currentThroughput;
                }

                if(ifTime){
                    //time stamp here for throughput
                    //equation for throughput
                    throughput = alpha*currentThroughput + (1-alpha)*throughput;
                    throughputs[k] = throughput;
                }

                 
                //send header to browser
                byteSent = send(sd,headerBuffer,i,0);
                //send content to browser
                byteSent = send(sd,bufferPtr,contentlength,0);

                myfile.open(log_path, std::ofstream::out | std::ofstream::app);
                    if (myfile.is_open()){
                        //<browser-ip> <chunkname> <server-ip> <duration> <tput> <avg-tput> <bitrate>
                        //myfile << "Now we start writing";
                        //myfile << "1 This is ips[k]\n";
                        myfile << ips[k] ;
                        //myfile << "2 This is chunkName\n";
                        myfile << " "+ chunkName + " ";
                        //myfile << &server_ip << " ";
                        myfile << inet_ntoa(address.sin_addr);
                        //myfile << "4 This is d.count\n";
                        myfile << std::fixed << std::setprecision(6) << d.count() << " "; 
                        //myfile << "5 This is currentThroughput\n";
                        myfile << std::fixed << std::setprecision(2)<< currentThroughput << " ";
                        //myfile << "6 This is throughput\n";
                        myfile << std::fixed << std::setprecision(2) << throughput << " ";
                        //myfile << "7 This is chosenBitrate\n";
                        myfile << std::fixed << std::setprecision(2) << chosenBitrate << "\n" ;
                        myfile.close();
                     }
                    else 
                        cout << "Error while opening the file";
			}
		}
	}
	return 0;
}

void dnsProxy(int listen_port, char * dns_ip, int dns_port, float alpha, char * log_path){
	//call dns server and resove the IP address of the web server
	char * www_ip = const_cast<char *>(DNSquery(dns_ip,dns_port).c_str());
	HttpProxy(listen_port, www_ip, alpha,log_path);
}

int main(int argc, char **argv){
	std::ofstream myfile;

	// ./miProxy --modns <listen-port> <www-ip> <alpha> <log>
	if(argc == 6 && !(strcmp(argv[1], "--nodns"))){
		int listen_port = atoi(argv[2]);
		char *www_ip = argv[3];
		float alpha = atof(argv[4]);
		char *log_path = argv[5];
		myfile.open(log_path, std::ofstream::out | std::ofstream::trunc);
		myfile.close();
		if(alpha>1||alpha<0){
			std::cout << "Error: value of alpha should be in range of [0,1]";
			return -1;
		}
		HttpProxy(listen_port, www_ip, alpha, log_path);
	}
	// ./miProxy --dns <listen-port> < dns-ip> <dns-port> <alpha> <log>
	else if(argc==7&&!(strcmp(argv[1], "--dns"))){
		int listen_port = atoi(argv[2]);
		char *dns_ip = argv[3];
		int dns_port = atoi(argv[4]);
		float alpha = atof(argv[5]);
		char *log_path = argv[6];
		myfile.open(log_path, std::ofstream::out | std::ofstream::trunc);
		myfile.close();
		if(alpha>1||alpha<0){
			std::cout << "Error: value of alpha should be in range of [0,1]";
			return -1;
		}
		dnsProxy(listen_port, dns_ip, dns_port, alpha, log_path);
	}
	else{
		std::cout << "Error: Incorrect input format";
		return -1;
	}
	return 0;
}