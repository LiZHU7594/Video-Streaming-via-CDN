#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include "DNSHeader.h"
#include "DNSQuestion.h"
#include "DNSRecord.h"
#include "geo.h"

#define HEADER_SIZE 12
#define QUESTION_SIZE 104
#define RECORD_SIZE 208
#define INET_ADDRSTRLEN 16 /* for IPv4 dotted-decimal */

// read ips from txt.file
void read_file(char * file_name, std::vector<std::string> * ip_addrs){
	char buffer[256];

	std::ifstream outFile;
	outFile.open(file_name,std::ios::in);
	if(!outFile.is_open()){
		printf("open failed\n");
	}else{
		while(!outFile.eof()){
			outFile.getline(buffer,256,'\n');//this method will end when meeting '\n' or the length is 256
			printf("%s\n",buffer);
			ip_addrs->push_back(buffer);
		}
		outFile.close();
	}
	ip_addrs->erase(ip_addrs->begin()+ip_addrs->size()-1);
}

int main(int argc, char * argv[]){
    //ip addr information
    std::vector<std::string> ip_addrs;
    std::ofstream myfile;
    
    map<string, string> geo_ip_map;
    int port;
    char * servers;
    char * log;
    char * opt = argv[1];
    if(argc == 5){
        if((!strcmp(opt, "--rr")) || (!strcmp(opt, "--geo"))){
            port = atoi(argv[2]);
            servers = argv[3];
            log = argv[4];
        }
        else{
            printf("argument error\n");
            return -1;
        }
    }
    else{
        return -1;
    }

    read_file(servers,&ip_addrs);

    myfile.open(log, std::ofstream::out | std::ofstream::trunc);
    myfile.close();

    int index = 0;
    int clientsd;

	//create socket
	int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockfd<0){
		perror("socket error");
		return -1;
	}

	//bind socket with IP and port, lick registering a phone number for the telephone
	struct sockaddr_in proxy_addr;
	proxy_addr.sin_family = AF_INET;
	proxy_addr.sin_addr.s_addr = INADDR_ANY;
	proxy_addr.sin_port = htons(port);
	int ret = bind(sockfd,(const struct sockaddr *)&proxy_addr, sizeof(proxy_addr));
	if(ret<0){
		perror("bind error");
		close(sockfd);
		return -1;
	}

	//Listening, waiting for client
	//listen() marks the socket referred to by sockfd as a passive socket
	//that is a socket that will be used to accept incoming connection requests by accept
	ret = listen(sockfd,10);
	if(ret<0){
		perror("listen error");
		close(sockfd);
		return -1;
	}

	while(true){
		printf("before accept...\n");

		//accepts a connection on a socket. Returns another socket representing client
		struct sockaddr_in connAddr;
		socklen_t connAddrLen = sizeof(connAddr);
		clientsd = accept(sockfd, (struct sockaddr *)&connAddr, &connAddrLen);
		if(clientsd==-1){
			printf("accept error...\n");
			exit(1);
		}
		printf("accept\n");

		//resolve client address
		char * client_ip = inet_ntoa(connAddr.sin_addr);

		//receive DNS header
		int headerlength;
		if(recv(clientsd, &headerlength, 4, MSG_WAITALL)<0){
			printf("recv header length error\n");
			return -1;
		}
		headerlength = ntohl(headerlength);
		printf("receive header length: %d\n", headerlength);

        char headerMSG[headerlength];
        memset(headerMSG,0,headerlength);
        if (recv(clientsd, headerMSG, headerlength, MSG_WAITALL) < 0){
            printf("recv header error");
            return -1;
        }
        printf("header:%s\n",headerMSG);
        
        std::string hmsgstr = std::string(headerMSG);
        std::cout << "Received header string:" << hmsgstr << "\n";
        DNSHeader header;
        header = header.decode(hmsgstr);
        if(header.QDCOUNT < 1){
            printf("****************ERROR: QDCOUNT < 1 ******************\n");
            close(clientsd);
            continue;
        }
        else if(header.QR == 1){
            printf("****************ERROR: QR = 1, it should be a query...************\n");
            close(clientsd);
            continue;
        }

		//receive DNS question
		int questionlen;
		if(recv(clientsd, &questionlen, sizeof(questionlen), MSG_WAITALL)<0){
			printf("recv question length error\n");
			return -1;
		}
		questionlen = ntohl(questionlen);
		printf("received question length: %d\n", questionlen);

		char questionMSG[questionlen];
		memset(questionMSG,0,questionlen);
		if(recv(clientsd,questionMSG,questionlen,MSG_WAITALL)<0){
			printf("recv question error");
			return -1;
		}
		std::string questionstr = std::string(questionMSG);
		DNSQuestion question;
		question = question.decode(questionMSG);
		printf("received DNS question for:%s\n",question.QNAME);

		//set unrelated field to 0
		header.TC=0;
		header.NSCOUNT=0;
		header.ARCOUNT=0;
		//QR=1 for a response header
		header.QR=1;
		//change response code according to QNAME
		if(strcmp(question.QNAME,"video.cse.umich.edu")!=0){
			printf("INCORRECT QUERY NAME ....CLOSING CONNECTION>>>\n");
			header.RCODE=3;
		}else{
			header.RCODE=0;
		}
		//set other field to default balue
		header.OPCODE=0;
		header.AA=1;
		header.RD=0;
		header.RA=0;
		header.Z=0;
		//QDCOUNT number of entries in question section
		header.QDCOUNT = 0;
		//ANCOUNT number of resource record
		header.ANCOUNT = 1;
		//now encode the DNS Response Header
		std::string headerStr = header.encode(header);
        headerlength = headerStr.length();
        printf("response headerlength=%d",headerlength);
        headerlength = htonl(headerlength);//htonl()--"Host to Network Long"

		//send response header
		//sends integer designating the size of DNS Response header
		if(send(clientsd,&headerlength,sizeof(headerlength),0)<0){
			printf("send header length error\n");
			return -1;
		}
		headerlength = ntohl(headerlength);
		printf("response header message: %s",headerStr.c_str());
		if(send(clientsd,headerStr.c_str(),headerlength,0)<0){
			printf("send heade string error\n");
			return -1;
		}
		std::cout<<"Response header sent..."<<std::endl;

		//get the server ip by round robin or geo
		std::string response_ip;
		if(!strcmp(opt,"--rr")){
			printf("before %d \n",index);
			response_ip = ip_addrs[index];
			index = (index+1)%ip_addrs.size();
			printf("after %d \n",index);
		}else if(!strcmp(opt,"--geo")){
			if(geo_ip_map.find(string(client_ip))==geo_ip_map.end()){
				response_ip = geo(servers,string(client_ip));
				geo_ip_map[string(client_ip)]=response_ip;
			}else{
				response_ip = geo_ip_map[string(client_ip)];
			}
		}

		if(response_ip.empty()){
			printf("response error");
			close(clientsd);
			return -1;
		}
		std::cout << "The response ip is:"<< response_ip << std::endl;

		//create instance of DNS record
		DNSRecord record;
		//NAME field is same as questions' QNAME
		strcpy(record.NAME,question.QNAME);
		//TYPE and CLASS = 1
		record.TYPE=1;
		record.CLASS=1;
		//No need to cache
		record.TTL=0;
		strcpy(record.RDATA,response_ip.c_str());
		record.RDLENGTH = response_ip.length();//ARPA Internet address

		//send DNS record
		std::string recordStr = record.encode(record);
		int recordlen = recordStr.length();
		printf("record length = %d\n",recordlen);
		recordlen = htonl(recordlen);
		printf("htonl length = %d\n",recordlen);
		if(send(clientsd,&recordlen,sizeof(recordlen),0)<0){
			perror("recordlen send error");
			return -1;
		}
		recordlen = ntohl(recordlen);
		printf("Sending record=%s\n",recordStr.c_str());
		if(send(clientsd,recordStr.c_str(),recordlen,0)<0){
			printf("send record string error\n");
			return -1;
		}
		close(clientsd);

		myfile.open(log, std::ios_base::app);
		if(myfile.is_open()){

			//<client-ip> <query-name> <response-ip>
			myfile << client_ip << " " << question.QNAME << " " << response_ip << std::endl;

			myfile.close();
		}else{
			std::cout << "Error while opening the file";
		}
	}

	printf("\n DNS close \n");
	close(sockfd);
}