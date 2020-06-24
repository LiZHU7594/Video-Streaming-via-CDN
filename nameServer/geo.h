#ifndef __GEO_H__
#define __GEO_H__
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <limits.h> 
using namespace std;


int minDistance(int dist[], bool spt_Set[], int nodes) 
{ 
    // Initialize min value 
    int min = INT_MAX, min_index; 
  
    for (int v = 0; v < nodes; v++) 
        if (spt_Set[v] == false && dist[v] <= min) 
            min = dist[v], min_index = v; 
  
    return min_index; 
} 


//return the closest video server to the client
string geo(char * filename, string client_ip){
	int curr_node, links, nodes;
	string ip,s;
	ifstream outFile;
	//unsigned -> unsigned int(0~65535)
	unsigned f,l,pos;
	map<string, int> client_map;//[ip:id]
	map<int, string> server_map;//[id:ip]
	string final_server_ip;

	char buffer[256];
	outFile.open(filename,std::ios::in);
	if(!outFile.is_open()){
		printf("open failed");
	}else{
		while(!outFile.eof()){
			outFile.getline(buffer,256,'\n');
			char * tmp;
			tmp = strtok(buffer,"NUM_NODES: ");
			nodes = atoi(tmp);

			//make hashmap of clients and servers and run for loop for nodes times
			for (int i = 0; i < nodes; ++i){
				memset(buffer,'\0',sizeof(buffer));
				outFile.getline(buffer,256,'\n');
				s = string(buffer);

				if(s.find("CLIENT")<1000){
					f = s.find(" CLIENT ");
					l = s.length();
					curr_node = atoi(s.substr(0,f).c_str());
					ip = s.substr(f+8,l);
					client_map[ip] = curr_node;
				}else if(s.find("SERVER")<1000){
					f = s.find(" SERVER ");
					l = s.length();
					curr_node = atoi(s.substr(0,f).c_str());
					ip = s.substr(f+8,l);
					server_map[curr_node] = ip;
				}else{
					continue;
				}
			}

			//read links
			outFile.getline(buffer,256,'\n');
			tmp = strtok(buffer,"NUM_LINKS ");
			links = atoi(tmp);

			//initialize an nodesXnodes array to store distances
			int data[nodes][nodes];
			memset(data,0,nodes*nodes*(sizeof(int)));

			string curr_line;
			int now_node = 0;

			while(std::getline(outFile, curr_line)){
				stringstream lineStream(curr_line);
				int val;
				int i=0;

				while(lineStream >> val){
					int col;
					int row;
					if(i%3==0){
						row = val;
					}
					if(i%3==1){
						col = val;
					}
					if(i%3==2){
						data[row][col] = val;
					}
					i++;
				}
			}

			int src = client_map[client_ip];

			//Dijkstra’s shortest path algorithm
			//find shortest paths from source to all vertices in the given graph.
			//Reference: www.geeksforgeeks.org
			int V = nodes;
			int dist[nodes]; 
		    bool spt_Set[nodes];  
		    for (int i = 0; i < nodes; i++) 
		        dist[i] = INT_MAX, spt_Set[i] = false; 
		  
		    dist[src] = 0; 
		  
		    for (int c = 0; c < nodes - 1; c++) {
		        int u = minDistance(dist, spt_Set, nodes); 
		        spt_Set[u] = true;  
		        for (int v = 0; v < nodes; v++) 
		            if (!spt_Set[v] && data[u][v] && dist[u] != INT_MAX 
		                && dist[u] + data[u][v] < dist[v]) 
		                dist[v] = dist[u] + data[u][v]; 
		    }  
		    //End of Reference

		    int min = INT_MAX;
		    int min_node;
		    for(int i=0;i<nodes;i++){
		    	if(server_map.find(i)!=server_map.end()){
		    		if(dist[i]<min){
		    			min = dist[i];
		    			min_node = i;
		    		}
		    	}
		    }
		    final_server_ip = server_map[min_node];
		    break;
		}
		outFile.close();
	}
	return final_server_ip;
}

#endif