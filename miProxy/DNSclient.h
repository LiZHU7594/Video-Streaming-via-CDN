#ifndef __DNSCLIENT_H__
#define __DNSCLIENT_H__

#include <stdio.h>
#include <iostream>
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
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include "DNSHeader.h"
#include "DNSQuestion.h"
#include "DNSRecord.h"

std::string DNSquery(char* hostname, int server_port){

	//create socket
	int sockfd;
	if((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))<0){
		printf("Failed to create socket\n");
		return "";
	}

	struct sockaddr_in serveraddr;
	struct hostent *host = gethostbyname(hostname);

	memset(&serveraddr,0,sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = *(unsigned long *)host->h_addr_list[0];
	serveraddr.sin_port = htons(server_port);

	//connect
	if(connect(sockfd,(struct sockaddr *)&serveraddr,sizeof(serveraddr))<0){
		printf("Cannot connect to server\n");
		return "";
	}

	//create a DNS header instance
	DNSHeader header;
	//set unrelated field to 0
	header.QR=0;
	header.TC=0;
	header.NSCOUNT=0;
	header.ARCOUNT=0;
	//response code, errors go here
	header.RCODE=0;
	//set other fileds to default
	header.OPCODE=0;
	header.AA=0;
	header.RD=0;
	header.RA=0;
	header.Z=0;
	//QDCOUNT number of entries in question section
	header.QDCOUNT = 1;
	//ANCOUNT number of resource record
	header.ANCOUNT = 0;

	//encode header
	std::string headerStr = header.encode(header);
	int headerlen = headerStr.length();
    headerlen = htonl(headerlen);

	//send the header length and header
	if(send(sockfd,&headerlen,sizeof(headerlen),0)==-1){
		printf("send header length error");
		return "";
	}
    headerlen = ntohl(headerlen);
	if(send(sockfd,headerStr.c_str(),headerlen,0)==-1){
		printf("send header string error");
		return "";
	}

	//now send DNS question
	//create DNS Question instance
	DNSQuestion question;
	question.QCLASS = 1;
	question.QTYPE = 1;
	strcpy(question.QNAME,"video.cse.umich.edu");

	//encode DNS question
	std::string questionStr = question.encode(question);
	int questionlen = questionStr.length();
	questionlen = htonl(questionlen);
	if(send(sockfd,&questionlen,sizeof(questionlen),0)==-1){
		printf("send question length error");
		return "";
	}
	questionlen = ntohl(questionlen);
	if(send(sockfd,questionStr.c_str(),questionlen,0)==-1){
		printf("send question string error");
		return "";
	}

	//now receive header
	headerlen = 0;
	if(recv(sockfd,&headerlen,sizeof(headerlen),MSG_WAITALL)<0){
		printf("recv header length error");
		return "";
	}
	headerlen = ntohl(headerlen);
	char headerMSG[headerlen];
	memset(headerMSG,0,headerlen);
	if(recv(sockfd,headerMSG,headerlen,MSG_WAITALL)<0){
		printf("recv header error");
		return "";
	}

	//resolve reply
	std::string hmsgstr = std::string(headerMSG);
	DNSHeader replyheader = replyheader.decode(hmsgstr);
	if(replyheader.RCODE!=0){
		printf("ERROR: RCODE=%d\n",replyheader.RCODE);
		printf("aborting...");
		close(sockfd);
		return "";
	}

	//now receive record
	int recordlen;
	if(recv(sockfd,&recordlen,sizeof(recordlen),MSG_WAITALL)<0){
		printf("recv record length error");
		return "";
	}
	recordlen = ntohl(recordlen);
	char recordMSG[recordlen];
	memset(recordMSG,'\0',recordlen);
	if(recv(sockfd,recordMSG,recordlen,MSG_WAITALL)<0){
		printf("receive record error");
		return "";
	}
	std::string rmsgstr = std::string(recordMSG,recordlen);
	DNSRecord record;
	record = record.decode(rmsgstr);
	std::string ipStr = std::string(record.RDATA,record.RDLENGTH);
	std::cout<<"server ip:"<<ipStr<<std::endl;

	close(sockfd);
	return ipStr;
}

#endif