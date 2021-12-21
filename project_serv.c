#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define MAXLINE 511
#define MAX_SOCK 2 // 게임 최대 참가자 수는 2명

char *EXIT_STRING = "exit";
char *START_STRING = "Connected to chat_server\n";
int num_chat = 0; // 채팅 참가자 수
int clisock_list[MAX_SOCK]; // 채팅에참가자 소켓번호 목록
int listen_sock;
int symbol[2] = { -1, -1 };
int state = 0;
int regame = 0;
int new = 0;
int win1 = 0, win2 = 0;

char result[3][3][50] = { "상대 입력 : 바위\n비겼습니다.\n", "상대 입력 : 가위\n이겼습니다.\n", "상대 입력 : 보\n졌습니다.\n",
			"상대 입력 : 바위\n졌습니다.\n", "상대 입력 : 가위\n비겼습니다.\n", "상대 입력 : 보\n이겼습니다.\n",
			"상대 입력 : 바위\n이겼습니다.\n", "상대 입력 : 가위\n졌습니다.\n", "상대 입력 : 보\n비겼습니다.\n" };

// 새로운 채팅 참가자 처리
void addClient(int s, struct sockaddr_in *newcliaddr);
void removeAllClient(void); // 채팅 탈퇴 처리 함수
int set_nonblock(int sockfd); // 소켓을 넌블럭으로 설정
int tcp_listen(int host, int port, int backlog); // 소켓 생성 및 listen
void errquit(char *mesg) {
	perror(mesg);
	exit(1);
}

int main(int argc, char *argv[]) {
	char buf[MAXLINE];
	char answer[2][MAXLINE];
	int i, j, count;
	int nbyte[2];
	int accp_sock, clilen;
	struct sockaddr_in cliaddr;

	if (argc != 2) {
		printf("사용법 : %s port \n", argv[0]);
		exit(0);
	}

	listen_sock = tcp_listen(INADDR_ANY, atoi(argv[1]), 5);
	if (listen_sock == -1)
		errquit("tcp_listen fail");
	if (set_nonblock(listen_sock) == -1)
		errquit("set_nonblock fail");

	for (count = 0; ; count++) {
		if (count == 100000) {
			putchar('.');
			fflush(stdout);
			count = 0;
		}

		clilen = sizeof(cliaddr);
		accp_sock = accept(listen_sock, (struct sockaddr *)&cliaddr, &clilen);
		if (accp_sock == -1 && errno != EWOULDBLOCK)
			errquit("accept fail");
		else if (accp_sock > 0) {
			// 통신용 소켓은 넌블럭 모드가 아님
			if (is_nonblock(accp_sock) != 0 && set_nonblock(accp_sock) < 0)
				errquit("set_nonblock fail");

			addClient(accp_sock, &cliaddr);
			send(accp_sock, START_STRING, strlen(START_STRING), 0);
			printf("%d번째 사용자 추가.\n", num_chat);
		}

		if (num_chat < 2) // 2명 들어오면 게임 시작
			continue;

		if (state == 0) {
			for (i = 0; i < num_chat; i++) {
				if (new == 0) {
					strcpy(buf, "게임을 시작합니다.\n");
					send(clisock_list[i], buf, strlen(buf), 0);
				}

				if (win1 != 2 && win2 !=2) {
					strcpy(buf, "1(바위), 2(가위), 3(보) 중 하나를 입력하시오.\n");
					send(clisock_list[i], buf, strlen(buf), 0);
				}
			}

			if (new == 0)
				printf("게임 시작\n");

			state++;
		}

		nbyte[0] = recv(clisock_list[0], answer[0], MAXLINE, 0);
		answer[0][nbyte[0]] = 0;
		nbyte[1] = recv(clisock_list[1], answer[1], MAXLINE, 0);
		answer[1][nbyte[1]] = 0;

		for (i = 0; i < num_chat; i++) {
			if (nbyte[i] != -1) {
				if (atoi(&answer[i][nbyte[i] - 2]) == 0)
					symbol[i] = 9;
				else if (atoi(&answer[i][nbyte[i] - 2]) == 1)
					symbol[i] = 0;
				else if (atoi(&answer[i][nbyte[i] - 2]) == 2)
					symbol[i] = 1;
				else if (atoi(&answer[i][nbyte[i] - 2]) == 3)
					symbol[i] = 2;
				else if (atoi(&answer[i][nbyte[i] - 2]) == 4) {
					removeAllClient();
					printf("서버 종료\n");
					exit(0);
				}
					
			}
		}

		if (win1 == 2 || win2 == 2) {
			if (regame != 0) {
				strcpy(buf, "승자입니다.\n다시 하시겠습니까?\n");
				if (win1 == 2)
					send(clisock_list[0], buf, strlen(buf), 0);
				else if (win2 == 2)
					send(clisock_list[1], buf, strlen(buf), 0);

				strcpy(buf, "패자입니다.\n다시 하시겠습니까?\n");
				if (win2 == 2)
					send(clisock_list[0], buf, strlen(buf), 0);
				else if (win1 == 2)
					send(clisock_list[1], buf, strlen(buf), 0);

				strcpy(buf, "0(다시하기), 4(종료) 중에 입력하시오.\n");
				for(i = 0; i < num_chat; i++)
					send(clisock_list[i], buf, strlen(buf), 0);
			}

			state++;
			regame = 0;

			if (symbol[0] != -1 && symbol[1] != -1) {
				symbol[0] = -1;
				symbol[1] = -1;
				state = 0;
				win1 = 0;
				win2 = 0;
				new = 0;
			}
		}
		else if (symbol[0] != -1 && symbol[1] != -1) {
			printf("입력 완료\n");

			if (symbol[0] - symbol[1] == -1 || symbol[0] - symbol[1] == 2)
				win1++;
			else if (symbol[0] - symbol[1] == 1 || symbol[0] - symbol[1] == -2)
				win2++;

			send(clisock_list[0], result[symbol[0]][symbol[1]], strlen(result[symbol[0]][symbol[1]]), 0);
			send(clisock_list[1], result[symbol[1]][symbol[0]], strlen(result[symbol[1]][symbol[0]]), 0);

			symbol[0] = -1;
			symbol[1] = -1;

			if (win1 == 2 || win2 == 2)
				state++;
				regame++;
			state = 0;
			new++;
		}
	}
}

// 새로운 채팅 참가자 처리
void addClient(int s, struct sockaddr_in *newcliaddr) {
	char buf[20];

	inet_ntop(AF_INET, &newcliaddr->sin_addr, buf, sizeof(buf));
	printf("new client: %s\n", buf);
	// 채팅 클라이언트 목록에 추가
	clisock_list[num_chat] = s;
	num_chat++;
}

// 채팅 탈퇴 처리
// 탈퇴할 때 모두 종료 처리
void removeAllClient(void) {
	int i;
	char buf[MAXLINE] = "exit";

	for (i = 0; i < num_chat; i++) {
		send(clisock_list[i], buf, strlen(buf), 0);
		close(clisock_list[i]);
	}
}

// 소켓이 nonblock 인지 확인
int is_nonblock(int sockfd) {
	int val;

	// 기존의 플래그 값을 얻어온다
	val = fcntl(sockfd, F_GETFL, 0);
	
	// 넌블럭 모드인지 확인
	if (val & O_NONBLOCK)
		return 0;

	return -1;
}

// 소켓을 넌블럭 모드로 설정
int set_nonblock(int sockfd) {
	int val;

	// 기존의 플래그 값을 얻어온다
	val = fcntl(sockfd, F_GETFL, 0);

	if (fcntl(sockfd, F_SETFL, val | O_NONBLOCK) == -1)
		return -1;

	return 0;
}

// listen 소켓 생성 및 listen
int tcp_listen(int host, int port, int backlog) {
	int sd;
	struct sockaddr_in servaddr;

	sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1) {
		perror("socket fail");
		exit(1);
	}

	// servaddr 구조체의 내용 세팅
	bzero((char *)&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(host);
	servaddr.sin_port = htons(port);
	
	if (bind(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		perror("bind fail");
		exit(1);
	}

	// 클라이언트로부터 연결요청을 기다림
	listen(sd, backlog);

	return sd;
}
