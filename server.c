/**
 *@filename server_shilei.c
 *@date		2020/5/5
 *@author	shilei
 *@brief	UDP聊天服务器端
 */
#include <stdio.h>	   //printf
#include <arpa/inet.h> //inet_addr htons
#include <sys/types.h>
#include <sys/socket.h> //socket bind listen accept connect
#include <netinet/in.h> //sockaddr_in
#include <stdlib.h>		//exit
#include <unistd.h>		//close
#include <string.h>		//strcat
#include <unistd.h>		//fork
#include <my_global.h>
#include <mysql.h>

#define N 512
#define PORT 1234
#define errlog(errmsg)                                          \
	do                                                          \
	{                                                           \
		perror(errmsg);                                         \
		printf("%s-->%s-->%d\n", __FILE__, __func__, __LINE__); \
		exit(1);                                                \
	} while (0)
/**
  L：表示消息类型为登录
  B：表示消息类型为广播
  Q：表示消息类型为退出
  */
typedef struct msg //定义消息结构体
{
	char type;		 /**< 消息类型 */
	char name[32];	 /**< 消息来源标识*/
	char toname[32]; /**< 消息去向标识*/
	char text[N];	 /**< 消息内容*/
} MSG;

int onlineflag = 0;//在线标志

void process_login(int sockfd, MSG msg, struct sockaddr_in clientaddr); //登录
void process_chat(int sockfd, MSG msg, struct sockaddr_in clientaddr);	//转发消息
void process_quit(int sockfd, MSG msg, struct sockaddr_in clientaddr);	//退出

int finish_with_error(MYSQL *con)
{
	fprintf(stderr, "%s\n", mysql_error(con));
	mysql_close(con);
	return -1;
}

int main()//int argc, const char *argv[]
{
	MSG msg;
	int sockfd;
	struct sockaddr_in serveraddr, clientaddr;
	socklen_t addrlen = sizeof(struct sockaddr);
	char buf[N] = {0};

	// if (argc < 3)
	// {
	// 	printf("argument is too few\n");
	// 	exit(1);
	// }

	//第一步：创建一个套接字
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		errlog("fail to socket");
	}

	//第二步：填充服务器网络信息结构体
	//inet_addr:将点分十进制IP地址转化为网络识别的
	//htons：将主机字节序转化为网络字节序
	//atoi：将数字型字符串转化为整型数据
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(PORT);

	//第三步：将套接字与网络信息结构体绑定
	if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
	{
		errlog("fail to bind");
	}
	while (1)
	{
		if (recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&(clientaddr), &addrlen) <= 0)
		{
			errlog("recvfrom error");
		}
		switch (msg.type)
		{
		case 'L':
			process_login(sockfd, msg, clientaddr);
			break;
		case 'B':
			process_chat(sockfd, msg, clientaddr);
			break;
		case 'Q':
			process_quit(sockfd, msg, clientaddr);
			break;
		}
	}

	close(sockfd);
	return 0;
}

/**
 *@brief  登录信息，更新数据库
 */
void process_login(int sockfd, MSG msg, struct sockaddr_in clientaddr)
{
	onlineflag = 1;
	strcpy(msg.text, msg.name);
	msg.text[strlen(msg.text)] = '\0';
	strcat(msg.text, " 上线了");
	puts(msg.text);
	//连接数据库
	MYSQL *con = mysql_init(NULL);
	if (NULL == con)
	{
		fprintf(stderr, "%s\n", mysql_error(con));
		return;
	}
	if (NULL == mysql_real_connect(con, "localhost", "root", "bJv5m@aCfy2.9GT", "shilei", 0, NULL, 0))
		finish_with_error(con);

	char tmp_sql[100];//mysql命令缓存区
	//插入数据信息
	sprintf(tmp_sql, "SELECT name FROM user where name like \'%s\'", msg.name);
	if (mysql_query(con, tmp_sql))
	{
		finish_with_error(con);
	}
	memset(tmp_sql, 0, sizeof(tmp_sql));
	MYSQL_RES *result1 = mysql_store_result(con);
	if (NULL == result1)
		finish_with_error(con);
	int num_fields1 = mysql_num_fields(result1);
	MYSQL_ROW row1;
	if((row1= mysql_fetch_row(result1))){
		mysql_free_result(result1);
		memset(tmp_sql, 0, sizeof(tmp_sql));
		//更新在线信息
		sprintf(tmp_sql, "update user set state=%d where name=\'%s\'", onlineflag, msg.name);
		if (mysql_query(con, tmp_sql))
			finish_with_error(con);
		printf("已向数据库shilei中user表更新用户%s的状态为在线\n",msg.name);
		memset(tmp_sql, 0, sizeof(tmp_sql));

		//更新IP信息
		sprintf(tmp_sql, "update user set IP=%d where name=\'%s\'", ntohl(clientaddr.sin_addr.s_addr), msg.name);
		if (mysql_query(con, tmp_sql))
			finish_with_error(con);
		memset(tmp_sql, 0, sizeof(tmp_sql));
		//更新port信息
		sprintf(tmp_sql, "update user set port=%d where name=\'%s\'", ntohs(clientaddr.sin_port), msg.name);
		if (mysql_query(con, tmp_sql))
			finish_with_error(con);
		memset(tmp_sql, 0, sizeof(tmp_sql));
		printf("已向数据库shilei中user表用户%s更新IP地址为%s:%d\n",msg.name,inet_ntoa(clientaddr.sin_addr),clientaddr.sin_port);
	}else{
		mysql_free_result(result1);
		sprintf(tmp_sql, "insert into user values (\'%s\',%d,%d,%d)", msg.name, onlineflag, ntohl(clientaddr.sin_addr.s_addr), ntohs(clientaddr.sin_port));
		if (mysql_query(con, tmp_sql))
			finish_with_error(con);
		printf("已向数据库shilei中user表插入IP地址为%s:%d的用户%s\n",inet_ntoa(clientaddr.sin_addr),clientaddr.sin_port,msg.name);
		memset(tmp_sql, 0, sizeof(tmp_sql));
	}

	//遍历数据库
	if (mysql_query(con, "SELECT * FROM user"))
		finish_with_error(con);
	MYSQL_RES *result = mysql_store_result(con);
	if (NULL == result)
		finish_with_error(con);
	int num_fields = mysql_num_fields(result);
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result)))
	{
		if (strcmp(row[0], msg.name) != 0 && atoi(row[1]) == 1)
		{ //广播某某上线信号
			struct sockaddr_in tempaddr;
			tempaddr.sin_family = AF_INET;
			tempaddr.sin_addr.s_addr = htonl(atoi(row[2]));
			tempaddr.sin_port = htons(atoi(row[3]));
			sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&(tempaddr), sizeof(tempaddr));
			printf("已向在线用户组广播用户%s上线信号\n",msg.name);
		}
	}
	mysql_free_result(result);
	mysql_close(con);
}

/**
 *@brief  转发信息（核心）
 */
void process_chat(int sockfd, MSG msg, struct sockaddr_in clientaddr)
{
	// puts(msg.text);
	//连接数据库
	MYSQL *con = mysql_init(NULL);
	if (NULL == con)
	{
		fprintf(stderr, "%s\n", mysql_error(con));
		return;
	}
	if (NULL == mysql_real_connect(con, "localhost", "root", "bJv5m@aCfy2.9GT", "shilei", 0, NULL, 0))
		finish_with_error(con);
	//遍历数据库
	if (mysql_query(con, "SELECT * FROM user"))
		finish_with_error(con);
	MYSQL_RES *result = mysql_store_result(con);
	if (NULL == result)
		finish_with_error(con);
	int num_fields = mysql_num_fields(result);
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result)))
	{
		// printf("1 %d", atoi(row[1]));
		// printf("2 %d", atoi(row[2]));
		// printf("3 %d", atoi(row[3]));
		if (strcmp(row[0], msg.toname) == 0 && atoi(row[1]) == 1) //如果发现某个成员与本msg中的目的人物相同且处于在线状态，则转发给目标人物
		{
			struct sockaddr_in tempaddr;
			tempaddr.sin_family = AF_INET;
			tempaddr.sin_addr.s_addr = htonl(atoi(row[2]));
			tempaddr.sin_port = htons(atoi(row[3]));
			sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&(tempaddr), sizeof(tempaddr));
			printf("已向用户%s转发用户%s的消息:%s\n",msg.toname,msg.name,msg.text);
		}
	}
	mysql_free_result(result);
	mysql_close(con);
}

/**
 *@brief  退出信息
 */
void process_quit(int sockfd, MSG msg, struct sockaddr_in clientaddr)
{
	onlineflag = 0;
	sprintf(msg.text, "%s 下线了", msg.name);
	puts(msg.text);

	MYSQL *con = mysql_init(NULL);
	if (NULL == con)
	{
		fprintf(stderr, "%s\n", mysql_error(con));
		return;
	}
	if (NULL == mysql_real_connect(con, "localhost", "root", "bJv5m@aCfy2.9GT", "shilei", 0, NULL, 0))
		finish_with_error(con);

	char tmp_sql[100];
	//更新在线信息
	sprintf(tmp_sql, "update user set state=%d where name=\'%s\'", onlineflag, msg.name);
	if (mysql_query(con, tmp_sql))
		finish_with_error(con);
	printf("已向数据库shilei中user表更新用户%s的状态为下线\n",msg.name);
	memset(tmp_sql, 0, sizeof(tmp_sql));
	if (mysql_query(con, "SELECT * FROM user"))
		finish_with_error(con);
	MYSQL_RES *result = mysql_store_result(con);
	if (NULL == result)
		finish_with_error(con);
	int num_fields = mysql_num_fields(result);
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result)))
	{
		if (strcmp(row[0], msg.name) != 0 && atoi(row[1]) == 1)
		{ //广播某某下线信号
			struct sockaddr_in tempaddr;
			tempaddr.sin_family = AF_INET;
			tempaddr.sin_addr.s_addr = htonl(atoi(row[2]));
			tempaddr.sin_port = htons(atoi(row[3]));
			sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&(tempaddr), sizeof(tempaddr));
			printf("已向在线用户组广播用户%s下线信号\n",msg.name);
		}
	}
	mysql_free_result(result);
	mysql_close(con);
}
