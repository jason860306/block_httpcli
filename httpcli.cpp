#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

int create_tcp_socket();
char *get_ip(char *host);
char *build_get_query(char *host, char *page);
void usage();

#define HOST "mirrors.163.com"
#define PAGE "/ubuntu-releases/15.10/ubuntu-15.10-desktop-amd64.iso"
#define PORT 80
#define USERAGENT "httpcli 1.0"
#define BUFSIZE BUFSIZ*1024UL

// url: http://mirrors.163.com/ubuntu-releases/15.10/ubuntu-15.10-desktop-amd64.iso

int main(int argc, char **argv)
{
	struct sockaddr_in *remote;
	int sock;
	int tmpres;
	char *ip;
	char *get;
	char buf[BUFSIZE+1];
	char *host;
	char *page;
	FILE *downfile;
#ifdef WIN32
	DWORD tmstart = ::GetTickCount();
#endif
/*
	if(argc == 1){
		usage();
		exit(2);
	}
*/
#ifdef WIN32
	WSADATA  Ws;
	//Init Windows Socket
	if ( WSAStartup(MAKEWORD(2,2), &Ws) != 0 )
	{
		fprintf(stderr, "Init Windows Socket Failed: %d\n", GetLastError());
	    return -1;
	}
#endif

	if (argc > 1)
	{
		host = argv[1];
	} else {
		host = HOST;
	}
	if(argc > 2){
		page = argv[2];
	}else{
		page = PAGE;
	}
	sock = create_tcp_socket();
	ip = get_ip(host);
	fprintf(stderr, "IP is %s\n", ip);
	remote = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in *));
	remote->sin_family = AF_INET;
	tmpres = inet_pton(AF_INET, ip, (void *)(&(remote->sin_addr.s_addr)));
	if( tmpres < 0)
	{
		perror("Can't set remote->sin_addr.s_addr");
		exit(1);
	}
	else if(tmpres == 0)
	{
		fprintf(stderr, "%s is not a valid IP address\n", ip);
		exit(1);
	}
	remote->sin_port = htons(PORT);

	if(connect(sock, (struct sockaddr *)remote, sizeof(struct sockaddr)) < 0){
		perror("Could not connect");
		exit(1);
	}
	get = build_get_query(host, page);
	fprintf(stderr, "Query is:\n<<START>>\n%s<<END>>\n", get);

	//Send the query to the server
	size_t sent = 0;
	while(sent < strlen(get))
	{
		tmpres = send(sock, get+sent, strlen(get)-sent, 0);
		if(tmpres == -1){
			perror("Can't send query");
			exit(1);
		}
		sent += tmpres;
	}
	//now it is time to receive the page
	memset(buf, 0, sizeof(buf));
	downfile = fopen("downfile.bin", "wb+");
	if (NULL == downfile)
	{
		perror("fopen failed");
		exit(1);
	}
	int htmlstart = 0, htmllen = BUFSIZE;
	char * htmlcontent;
	while((tmpres = recv(sock, buf, BUFSIZE, 0)) > 0){
		htmllen = tmpres;
		if(htmlstart == 0)
		{
			/* Under certain conditions this will not work.
			* If the \r\n\r\n part is splitted into two messages
			* it will fail to detect the beginning of HTML content
			*/
			htmlcontent = strstr(buf, "\r\n\r\n");
			if(htmlcontent != NULL){
				htmlstart = 1;
				htmlcontent += 4;
				htmllen -= tmpres - (htmlcontent - buf);
			}
		}else{
			htmlcontent = buf;
		}
		if(htmlstart){
			fwrite(htmlcontent, htmllen, 1, downfile);
		}

		memset(buf, 0, tmpres);
	}
	if(tmpres < 0)
	{
		perror("Error receiving data");
	}
#ifdef WIN32
	fprintf(stdout, "elpse: %ldms\n", (::GetTickCount() - tmstart));
#endif
	free(get);
	free(remote);
	free(ip);
#ifdef WIN32
	closesocket(sock);
#else
	close(sock);
#endif
	fclose(downfile);
	return 0;
}

void usage()
{
	fprintf(stderr, "USAGE: htmlget host [page]\n"
/*			"\thost: the website hostname. eg: coding.debuntu.org\n"*/
			"\tpage: the page to retrieve. eg: index.html, default: /\n");
}


int create_tcp_socket()
{
	int sock;
	if((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
		perror("Can't create TCP socket");
		exit(1);
	}
	return sock;
}


char *get_ip(char *host)
{
	struct hostent *hent;
	int iplen = 15; //XXX.XXX.XXX.XXX
	char *ip = (char *)malloc(iplen+1);
	memset(ip, 0, iplen+1);
	if((hent = gethostbyname(host)) == NULL)
	{
		perror("Can't get IP");
		exit(1);
	}
	if(inet_ntop(AF_INET, (void *)hent->h_addr_list[0], ip, iplen) == NULL)
	{
		perror("Can't resolve host");
		exit(1);
	}
	return ip;
}

char *build_get_query(char *host, char *page)
{
	char *query;
	char *getpage = page;
	const char *tpl = "GET /%s HTTP/1.0\r\nHost: %s\r\nRange: bytes=0-\r\nUser-Agent: %s\r\n\r\n";
	if(getpage[0] == '/'){
		getpage = getpage + 1;
		fprintf(stderr,"Removing leading \"/\", converting %s to %s\n", page, getpage);
	}
	// -5 is to consider the %s %s %s in tpl and the ending \0
	size_t len = strlen(host)+strlen(getpage)+strlen(USERAGENT)+strlen(tpl)-5;
	query = (char *)malloc(len);
#ifdef WIN32
	_snprintf(query, len, tpl, getpage, host, USERAGENT);
#else
	snprintf(query, len, tpl, getpage, host, USERAGENT);
#endif
	return query;
}
