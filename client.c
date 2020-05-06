/**
 *@filename 6client.c
 *@data		2020/5/5
 *@author	shilei
 *@brief	UDP聊天客户端
 */
#include <stdio.h>
#include <arpa/inet.h> //inet_addr htons
#include <sys/types.h>
#include <sys/socket.h> //socket bind listen accept connect
#include <netinet/in.h> //sockaddr_in
#include <stdlib.h>     //exit
#include <unistd.h>     //close
#include <string.h>     //strcat
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#define N 512
#define PORT 1234
#define errlog(errmsg)                                          \
    do                                                          \
    {                                                           \
        perror(errmsg);                                         \
        printf("%s-->%s-->%d\n", __FILE__, __func__, __LINE__); \
        exit(1);                                                \
    } while (0)

typedef struct msg
{
    char type;
    char name[32];
    char toname[32]; 
    char text[N];
} MSG;

MSG msg;
int sockfd;
struct sockaddr_in serveraddr;
socklen_t addrlen = sizeof(struct sockaddr);

int main(int argc, const char *argv[])
{
    pid_t pid;
    if (argc < 2)
    {
        printf("argument is too few\n");
        exit(1);
    }

    /**第一步：创建一个套接字*/
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        errlog("fail to socket");
    }

    /**第二步：填充服务器网络信息结构体
	  *inet_addr:将点分十进制IP地址转化为网络识别的
	  *htons：将主机字节序转化为网络字节序
	  *atoi：将数字型字符串转化为整型数据
	  */
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
    serveraddr.sin_port = htons(PORT);

    printf("输入用户名:");
    fgets(msg.name, sizeof(msg.name), stdin);
    msg.name[strlen(msg.name) - 1] = '\0';
    msg.toname[0] = '\0';
    msg.text[0] = '\0';
    msg.type = 'L';
    if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&serveraddr, addrlen) < 0)
    {
        errlog("fail to sendto");
    }

    pid = fork();
    if(pid<0)
        printf("error in fork!\n");
    else if (pid == 0) /**子进程从终端获取输入，并发送消息**/
    {
        printf("输入你想与其聊天的用户名:");
        fgets(msg.toname, sizeof(msg.toname), stdin);
        msg.toname[strlen(msg.toname) - 1] = '\0';
        printf("请等待你想与其聊天的用户上线……\n");
        while(1)
		{
			fgets(msg.text,N,stdin);
			msg.text[strlen(msg.text)-1]='\0';
			if(strncmp(msg.text,"quit",4)==0)
			{
				msg.type='Q';
				if(sendto(sockfd,&msg,sizeof(msg),0,(struct sockaddr *)&serveraddr,addrlen)<0)
				{
						errlog("fail to sendto");
				}
				printf("quit !");
				kill(getppid(),SIGKILL);
				exit(1);
			}
			else
			{
				msg.type='B';
				if(sendto(sockfd,&msg,sizeof(msg),0,(struct sockaddr *)&serveraddr,addrlen)<0)
				{
						errlog("fail to sendto");
				}
			}
		}
    }
    else /**父进程接收服务器端的消息并打印**/
    {
        while (1)
        {
            recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&serveraddr, &addrlen); 
			puts(msg.text);
        }
    }
    close(sockfd);

    return 0;
}
