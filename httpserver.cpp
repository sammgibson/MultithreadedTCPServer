#include <iostream>
#include <stdio.h>
#include <string>
#include <iomanip>
#include <sstream>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <algorithm>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <regex>
#include <errno.h>
#include <err.h>
#include <fcntl.h>


using namespace std;
#define BUFFER_SIZE 32768
#define THREAD_BUFFER_SIZE 16380
//int num = 0;
vector<int> threadAvail;
off_t logOffsetNum = 0;


pthread_mutex_t mylock;
pthread_mutex_t sockLock;
pthread_mutex_t coutLock;
pthread_mutex_t logLock;


struct socketInfo{
    int socket;
    string buff;
    int index;
    int logDescriptor;
    //bool log;
        //char *buff
};
int logWritePUT(char *buffer, off_t *startHere, int logSocketDesc, int zeroOffset, ssize_t readSize){// int
    // CONVERT TO HEX
   // cout << readSize << endl;
  //  cout << *startHere << endl;
    char *items= (char *)malloc(69*sizeof(char));
    int i=0;
    int starter = 0;
    while((*buffer) && i < readSize){
        if(i%20 == 0){
            if(i != 0){
                starter = i;
                //printf("%s", items);
                pwrite(logSocketDesc, items, 69, (*startHere));
                *startHere=(*startHere)+69;
                memset(items, '\0', 69);
            }
            sprintf(items, "\n%08d", zeroOffset*20);
            zeroOffset++;
        }else if(i == readSize){
            //printf("%s", items);
        }
        sprintf(&items[9 + (i%20) * 3], " %02x", (unsigned int)(unsigned char)*buffer++);
        i++;
    }
    int rest = string(items).size();
    if(rest != 0){
        //cout << "REST: " << rest << endl;
        pwrite(logSocketDesc, items, rest, (*startHere));
        *startHere = (*startHere)+rest;
        memset(items, '\0', 69);
        sprintf(items, "%s", "\n========\n");
        pwrite(logSocketDesc, items, 10, (*startHere));
    }
    return zeroOffset;
}

string getFileName(string buff){
  ssize_t space = buff.find(" ");
  string header = buff.substr(space+1);
  ssize_t sndSpace = header.find(" ");
  header = header.substr(0,sndSpace);
  return header;
}

bool validFileName(int socket, string fileName){

  if(fileName.size() != 27){
    string eResponse = "HTTP/1.1 400 Bad Request\r\n";
    eResponse += "Content-Length: 0\r\n\r\n";
    write(socket, eResponse.c_str(), eResponse.size());
      //cout << "FILENAME TOO SHORT" << fileName.length()<< endl;
    return false;
  }

  regex e ("[a-z0-9-_]*", regex_constants::icase);
  if(!regex_match(fileName,e)){
    string eResponse = "HTTP/1.1 400 Bad Request\r\n";
    eResponse += "Content-Length: 0\r\n\r\n";
    write(socket, eResponse.c_str(), eResponse.size());
      //cout << "FILENAME NOT VALID" << endl;
    return false;
  }
  return true;
}



void *threadPass(void *structure){
    
    socketInfo *sockInfo = (socketInfo *)structure;
    int socket = sockInfo->socket;
    string buff = sockInfo->buff;
    int index = sockInfo->index;
    int log = sockInfo->logDescriptor;
    //cout << "LOGNAME IN FUNCITON: " << log << endl;
    pthread_mutex_lock(&coutLock);
    //cout << "Do we get to threadpass, threadID: " << index << endl;
    //cout << "HERES THE BUFFER IN THE FUNCTION: \n" << buff <<"\n\n" << endl;
    pthread_mutex_unlock(&coutLock);
    string firstThree = buff.substr(0,3);
    // search for either PUT or GET
    bool PUT = false;
    if(buff.substr(0,3) == "PUT"){
        //cout << "IS THERE A PUT?" << endl;
        PUT = true;
    }else if(buff.substr(0,3) == "GET"){
        //cout << "DO WE GET HERE?" << endl;
        ///PUT = false;
    }else{// Request does not contain PUT OR GET exit with error
        
        if(log > 0){
            string logHead = "FAIL: " + buff.substr(0,buff.find(" ")) + "-- response 500\n========\n";
                        
            pthread_mutex_lock(&logLock);
            off_t startHere = logOffsetNum;
            logOffsetNum += logHead.length();
            pthread_mutex_unlock(&logLock);
                   //pwrite(logSocketDesc, items, 10, (*startHere));
            pwrite(log, logHead.c_str(), logHead.length(), startHere);
        }
        string response = "HTTP/1.1 500 INTERNAL SERVER ERROR\r\nContent-Length: 0\r\n\r\n";
        pthread_mutex_lock(&mylock);
        write(socket, response.c_str(), response.size());
        pthread_mutex_unlock(&mylock);
        close(socket);
        //pthread_mutex_lock(&sockLock);
        threadAvail.push_back(index);
        //pthread_mutex_unlock(&sockLock);
        pthread_exit(NULL);
    }
    pthread_mutex_lock(&mylock);
    string fileName = getFileName(buff);
    bool valid = validFileName(socket, fileName);
    if(!valid){
        if(log > 0){
                 string logResponse = "FAIL: " + buff.substr(0,3) + " " + fileName + " HTTP/1.1 -- response 400\n========\n";
                 
                pthread_mutex_lock(&logLock);
                off_t startHere = logOffsetNum;
                logOffsetNum += logResponse.length();
                pthread_mutex_unlock(&logLock);
            //pwrite(logSocketDesc, items, 10, (*startHere));
                pwrite(log, logResponse.c_str(), logResponse.length(), startHere);
             }
        
        pthread_mutex_unlock(&mylock);
        close(socket);
        //pthread_mutex_lock(&sockLock);
        threadAvail.push_back(index);
        //pthread_mutex_unlock(&sockLock);
        pthread_exit(NULL);
    }
    pthread_mutex_unlock(&mylock);
///////////////////////////////////////////BEGIN PUT///////////////////////////////////////////////////////////////
    if(PUT){
        ssize_t foundContentLength;
        int32_t contentLength = 0;
        if((foundContentLength = buff.find("Content-Length:")) != int(string::npos)) {
            string substring = buff.substr(foundContentLength);
            contentLength = stoi(substring.substr(16, buff.find("\n")-1));
        }
        //cout << "CONTENTLENGTH: " << contentLength << endl;
        
        int newFileDesc = open(fileName.c_str(), O_TRUNC | O_WRONLY | O_CREAT, 0666);
        if(newFileDesc < 0){
            //////////TODO///////////
            if(log > 0){
                string logHead = "FAIL: PUT " + fileName + " HTTP/1.1 -- 404\n========\n";
                pthread_mutex_lock(&logLock);
                off_t startHere = logOffsetNum;
                logOffsetNum += logHead.length();
                pthread_mutex_unlock(&logLock);
                //pwrite(logSocketDesc, items, 10, (*startHere));
                pwrite(log, logHead.c_str(), logHead.length(), startHere);
            }
            string eResponse = "HTTP/1.1 404 Not Found\r\n";
            eResponse += "Content-Length: 0\r\n\r\n";
            pthread_mutex_lock(&mylock);
            write(socket, eResponse.c_str(), eResponse.size());
            pthread_mutex_unlock(&mylock);
            close(newFileDesc);
            close(socket);
            //pthread_mutex_lock(&sockLock);
            threadAvail.push_back(index);
            //pthread_mutex_unlock(&sockLock);
            pthread_exit(NULL);
        }
        //////////////////////////////// STARTING LOG IN PUT //////////////////////////////////////////////////
        off_t beginLogWrite = 0;
        if(log >0){
            if (contentLength != 0){
                int numBytes;
                int numLinesHex = contentLength/20;
                //cout << "NUM LINES" << numLinesHex << endl;
                int numLinesHexMod = contentLength%20;
                numBytes = 40 + to_string(contentLength).length();// reserve the logfile header
                numBytes += numLinesHex*69;// reserve the number of full lines of hex
                if(numLinesHexMod != 0){
                    numBytes += 9 + numLinesHexMod*3;// reserve a possible extra line
                }
                numBytes += 9;
                pthread_mutex_lock(&logLock);
                beginLogWrite = logOffsetNum;
                logOffsetNum +=  numBytes;
                pthread_mutex_unlock(&logLock);
                
                
                //cout << "HERE IS THE numBYTES: " << numBytes << endl;
                string logHeader = "PUT " + fileName + " length " + to_string(contentLength);
                
                pwrite(log, logHeader.c_str(), logHeader.length(), beginLogWrite);
                beginLogWrite+=logHeader.length();
                //cout << "HERE IS THE BEGINNING OF THE WRITE" << beginLogWrite << endl;
            }else{
                string logHeader = "PUT " + fileName + " length " + to_string(contentLength) + "\n========\n";
                int numBytes = logHeader.length();
                pthread_mutex_lock(&logLock);
                //beginLogWrite = logOffsetNum;
                //logOffsetNum +=  numBytes;
                pthread_mutex_unlock(&logLock);
                pwrite(log, logHeader.c_str(), logHeader.length(), beginLogWrite);
            }
        }
        off_t *beginHere = &beginLogWrite;
        /////////////////////////////// ENDING LOG /////////////////////////////////////////////////////////////////
        
        char* fBuff = (char *) malloc(THREAD_BUFFER_SIZE*sizeof(char));
        ssize_t fileSize;
        pthread_mutex_lock(&mylock);
        //string putContents;

        int logZeroOffset = 0;
        
        while(contentLength > 0){// need to account for writes if no content length
            fileSize = read(socket, fBuff, THREAD_BUFFER_SIZE);
            if(log >0 && contentLength != 0){
                logZeroOffset = logWritePUT(fBuff, beginHere, log, logZeroOffset, fileSize);
            }
            write(newFileDesc, fBuff, fileSize);
            //memset(fBuff, '\0', fileSize);
            contentLength-=THREAD_BUFFER_SIZE;
        }
        pthread_mutex_unlock(&mylock);
        string response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
        if(write(socket, response.c_str(), response.size())<0){
            pthread_mutex_lock(&coutLock);
            //cout << " WE FOUND IT BOYS3" << endl;
            pthread_mutex_unlock(&coutLock);
        }
        pthread_mutex_unlock(&mylock);
        
        close(socket);
        //pthread_mutex_lock(&sockLock);
        threadAvail.push_back(index);
        //pthread_mutex_unlock(&sockLock);
        pthread_mutex_lock(&coutLock);
        //cout << "------------------------------------------------------" << endl;
        //cout << "THREAD ID: " << index << " HAS FINISHED IT'S PUT" << endl;
        //cout << "------------------------------------------------------" << endl;
        pthread_mutex_unlock(&coutLock);
        
    
        free(fBuff);
        pthread_exit(NULL);
        
    }else{/////////////////////////////////////////////// GET GET GET//////////////////////////////////////////////////
        int fileDesc;
        
        
        pthread_mutex_lock(&coutLock);
        //cout << "ABOUT TO START GET" << endl;
        pthread_mutex_unlock(&coutLock);
        pthread_mutex_lock(&mylock);
        if((fileDesc = open(fileName.c_str(), O_RDONLY, 0666))<0){
            string eResponse;
            string erCode;
            if(errno == ENOENT){
                erCode = "404";
                eResponse = "HTTP/1.1 404 Not Found\r\n";
                eResponse += "Content-Length: 0\r\n\r\n";
            }else{
                erCode = "403";
                eResponse = "HTTP/1.1 403 Forbidden\r\n";
                eResponse += "Content-Length: 0\r\n\r\n";
            }
            
            if(log > 0){
                string logHead = "FAIL: GET " + fileName + " HTTP/1.1 -- " + erCode + "\n========\n";
                pthread_mutex_lock(&logLock);
                off_t startHere = logOffsetNum;
                logOffsetNum += logHead.length();
                pthread_mutex_unlock(&logLock);
                //pwrite(logSocketDesc, items, 10, (*startHere));
                pwrite(log, logHead.c_str(), logHead.length(), startHere);
            }
            write(socket, eResponse.c_str(), eResponse.size());
            close(fileDesc);
            pthread_mutex_unlock(&mylock);
            close(socket);
            //pthread_mutex_lock(&sockLock);
            threadAvail.push_back(index);
            //pthread_mutex_unlock(&sockLock);
            pthread_exit(NULL);
        }
        if(log>0){
            string logHeader = "GET " + fileName + " length 0\n========\n";
            pthread_mutex_lock(&logLock);
            off_t reservePlace = logOffsetNum;
            logOffsetNum = logOffsetNum + logHeader.size();
            pthread_mutex_unlock(&logLock);
            pwrite(log, logHeader.c_str(), logHeader.length(), reservePlace);
        }
        //pthread_mutex_unlock(&mylock);
        //cout << "ALLOCATING THE FILE BUFFER" << endl;
        char* fBuff = (char *) malloc(BUFFER_SIZE*sizeof(char));
        ssize_t readLen;
        //pthread_mutex_lock(&mylock);
        string response = "HTTP/1.1 200 OK\r\n";
        off_t fileLen = lseek(fileDesc, 0, SEEK_END);
        response += "Content-Length: " + to_string(fileLen) + "\r\n\r\n";
        write(socket, response.c_str(), response.size());
        fileDesc = open(fileName.c_str(), O_RDONLY, 0666);
        while((readLen = read(fileDesc, fBuff, BUFFER_SIZE)) > 0){
            //cout << "Writing the buffer to the socket" << endl;
            //pthread_mutex_lock(&coutLock);
            //cout << "HERES THE FILE BUFFER: " << fBuff << endl;
            //pthread_mutex_unlock(&coutLock);
            write(socket, fBuff, readLen);
        }
        //cout << "EXITING THE WHILE" << endl;
        free(fBuff);
        pthread_mutex_unlock(&mylock);
        close(fileDesc);
        close(socket);
        pthread_mutex_lock(&coutLock);
        //cout << "THREAD ID: " << index << " IS DONE WITH ITS GET" << endl;
        pthread_mutex_unlock(&coutLock);
        //pthread_mutex_lock(&sockLock);
        threadAvail.push_back(index);
        //pthread_mutex_unlock(&sockLock);
        pthread_exit(NULL);
    }// end of GET
}


int main(int argc, char* argv[]){
  struct sockaddr_in servaddr;
  struct sockaddr_in cliaddr;
  char* buffer = (char *) malloc(BUFFER_SIZE*sizeof(char));
  int port = 80;
  int numThreads = 4;
  string address, logfile;
  int input, logFileDesc;
  bool NFlag, lFlag, addFlag = false;
  if(argc == 1){
    cout << "Not enough arguments" << endl;
    exit(1);
  }
  while((input = getopt(argc, argv, "N:l:")) != -1){
    switch(input) {
    case 'N':
      numThreads = atoi(optarg);
      NFlag = true;
      //cout << "Threads flag caught in getOPT" << endl;
      break;
    
    case 'l':
      logfile = string(optarg);
      //cout << logfile << endl;
      lFlag = true;
      //cout << "log flag caught in getOPT" << endl;
      break;
    }
  }
  for(int i = 1; i < argc; i++){
    if((strncmp("-N", argv[i], 2)==0) && NFlag){
      i++;
      //cout << "For loop caught the thread flag, moving on" << endl;
      cout << "Number of threads: " << argv[i] << endl;
      continue;
    }else if((strncmp("-l", argv[i], 2)==0) && lFlag){
      i++;
      //cout << "For loop caught the log flag. moving on" << endl;
        logFileDesc = open(logfile.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0666);
        if(logFileDesc < 0){
            cout << "FAILS TO OPEN LOGFILE" << endl;
            perror("ERROR");
            exit(1);
        }
      cout << "LOGFILE: " << logfile << endl;
      continue;
    }else{
      if(!addFlag){

    if(strncmp("localhost", argv[i], 10) == 0){
      //cout << "Address is localhost assigning 127.0.0.1: " << argv[i] << endl;
      address = "127.0.0.1";
    }else{
      //cout << "Address is not localhost: " << argv[i] << endl;
      address = argv[i];
    }
    addFlag = true;
    //cout << "Address: " << address << endl;
      }else{
    port = atoi(argv[i]);
   // cout << "Port: " << port << endl;
      }
    }
  }
  if(!addFlag){
    cout << "No address given" << endl;
    exit(1);
  }
  ////////////// END OF ARGUMENT PARSING-CREATING THREADS //////////////////////
    //cout << "DO WE GET HERE?" << endl;
    pthread_t *tid = (pthread_t *)malloc(numThreads*sizeof(pthread_t));
    //vector<int> threadAvail;
    //cout << "ABOUT TO CREATE THREADS" << endl;
  for(int i = 0; i < numThreads; i++){
      threadAvail.push_back(i);
     // pthread_create(&tid[i], NULL, threadWait, &tid[i]);
//      pthread_join(tid[i], NULL);
  }
  if(pthread_mutex_init(&mylock, NULL) != 0){
    cout << "MUTEX FAILED 1" << endl;
    return 1;
  }
    if(pthread_mutex_init(&sockLock, NULL) != 0){
        cout << "MUTEX FAILED 2" << endl;
        return 1;
    }
        if(pthread_mutex_init(&coutLock,NULL) != 0){
            cout << "MUTEX FAILED 3" << endl;
            return 1;
            
        }
    if(pthread_mutex_init(&logLock, NULL) != 0){
        cout << "MUTEX FAILED 4" << endl;
        return 1;
    }
  ////////////////////THREAD CREATED - WORKING ON SERVER CREATION ////////////

  int serverDesc;
  if((serverDesc = (socket(AF_INET, SOCK_STREAM, 0))) < 0){
    perror("ERROR");
    cout << "Unsuccessful socket creation." << endl;
    exit(1);
  }
  int one = 1;
  setsockopt(serverDesc, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
  servaddr.sin_family = AF_INET;
  inet_aton(address.c_str(), &servaddr.sin_addr);
  servaddr.sin_port = htons(port);
  
  /////////////////// PREVIOUS PROGRAM BIND DIDNT WORK ////////////////////////
  int bindAddr;
  socklen_t len = sizeof(servaddr);
  bindAddr = ::bind(serverDesc, (struct sockaddr *) &servaddr, len);
  if(bindAddr < 0){
    perror("ERROR");
    exit(1);
  }
  ///////////////// WORKS WITH 127.0.0.1 BUT NOT WITH LOCALHOST//////////////
    
  int listenRes;
  listenRes = listen(serverDesc, 128);
  if(listenRes < 0){
    perror("ERROR");
    //exit(1);
  }
  int socketNum;
  socklen_t socketLen = sizeof(cliaddr);
    //char *subbuff=(char *)malloc(BUFFER_SIZE*sizeof(char));
    
    //cout << "About to enter while loop" << endl;
    socketInfo *infoPool = (socketInfo *)malloc(numThreads*sizeof(socketInfo));
  while(1){
      // Need to figure out how to
      
      //cout << "Waiting for the client......." << endl;
      if((socketNum = accept(serverDesc, (struct sockaddr*) &cliaddr, &socketLen))<0){
          perror("ERROR");
          exit(1);
    
      }
      pthread_mutex_lock(&sockLock);
      
      //cout << "Receiving from Client..." << endl;
      int nbytes = recv(socketNum, buffer, BUFFER_SIZE, 0);
      if(nbytes == -1){
          perror("ERROR");
            //      continue;
          exit(1);
      }
      string strBuff = string(buffer);
      //cout << "HERES THE BUFFER: \n" << buffer << endl;
      pthread_mutex_lock(&coutLock);
      //cout << "------------------------------------------------------" << endl;
      //cout << "NUMBER OF THREADS AVAILABLE AT THE MOMENT: " << threadAvail.size() << endl;
      //cout << "------------------------------------------------------" << endl;
      pthread_mutex_unlock(&coutLock);
      //      cout << threadAvail << endl;
      while(threadAvail.empty()){sleep(1);};
      int i =threadAvail.front();
      infoPool[i].socket = socketNum;
      infoPool[i].buff=strBuff;
      if(lFlag){
          infoPool[i].logDescriptor = logFileDesc;
      }else{
          infoPool[i].logDescriptor = -1;
      }
      pthread_mutex_lock(&coutLock);
      //cout << "------------------------------------------------------" << endl;
      //cout << "THREAD ID: " << i << " IS ABOUT TO START SOCKET: " << socketNum << endl;
      //cout << "------------------------------------------------------" << endl;
      //cout << "HERES THE BUFFER: \n" << strBuff << endl;
      pthread_mutex_unlock(&coutLock);
      infoPool[i].index = i;
      threadAvail.erase(threadAvail.begin());
      pthread_mutex_unlock(&sockLock);
      pthread_create(&tid[i], NULL, threadPass, (void *)&infoPool[i]);
  }
    free(buffer);
    close(logFileDesc);
    //free(subbuff);
    close(serverDesc);
    pthread_mutex_destroy(&mylock);
    return 0;
}
