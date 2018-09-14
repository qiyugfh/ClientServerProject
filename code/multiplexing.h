#pragma once

#ifndef MULTIPLEXING_H
#define MULTIPLEXING_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <unistd.h>
using namespace std;

#define MAXLINE     1024
class Multiplexing {
protected:
	static const int DEFAULT_IO_MAX = 10; //Ĭ�ϵ�����ļ�������
	static const int INFTIM = -1;
	int io_max; //��¼����ļ�������
	int listenfd; //�������
public:
	Multiplexing() { this->io_max = DEFAULT_IO_MAX; }
	Multiplexing(int max, int listenfd) { this->io_max = max; this->listenfd = listenfd; }
	~Multiplexing() {}

	virtual void server_do_multiplexing() = 0; //�����io��·����
	virtual void client_do_multiplexing() = 0; //�ͻ���io��·����
	virtual void handle_client_msg() = 0; //����ͻ�����Ϣ
	virtual bool accept_client_proc() = 0; //���տͻ�������
	virtual bool add_event(int confd, int event) = 0;
	virtual int wait_event() = 0; // �ȴ��¼�
};

//-----------------select-------------------------
class MySelect : public Multiplexing {
private:
	fd_set* allfds;      //�������
	int* clifds;   //�ͻ��˼���
	int maxfd; //��¼��������ֵ
	int cli_cnt; //�ͻ��˸���

public:
	MySelect() : Multiplexing() { allfds = NULL; clifds = NULL; maxfd = 0; cli_cnt = 0; }
	MySelect(int max, int listenfd);
	~MySelect() {
		if (allfds) {
			delete allfds;
			allfds = NULL;
		}
		if (clifds) {
			delete clifds;
			clifds = NULL;
		}
	}

	void server_do_multiplexing();
	void client_do_multiplexing();
	void handle_client_msg();
	bool accept_client_proc();
	bool add_event(int confd, int event);
	bool init_event(); //ÿ�ε���selectǰ��Ҫ���������ļ�������
	int wait_event(); // �ȴ��¼�

};

//-----------------poll-------------------------
typedef struct pollfd Pollfd;
class MyPoll : public Multiplexing {
private:
	Pollfd* clientfds; //poll��ʹ��pollfd�ṹ��ָ��һ�������ӵ��ļ�������
	int max_index; //��¼��ǰclientfds������ʹ�õ�����±�

public:
	MyPoll() : Multiplexing() { clientfds = NULL; max_index = -1; }
	MyPoll(int max, int listenfd);
	~MyPoll() {
		if (clientfds) {
			delete clientfds;
			clientfds = NULL;
		}
	}

	void server_do_multiplexing();
	void client_do_multiplexing();
	void handle_client_msg();
	bool accept_client_proc();
	bool add_event(int confd, int event);
	int wait_event(); // �ȴ��¼�
};

//-----------------epoll-------------------------
typedef struct epoll_event Epoll_event;
class MyEpoll : public Multiplexing {
private:
	int epollfd; //epoll�ľ���������������ļ�������
	Epoll_event *events; //�¼�����
	int nready; //��handle_client_msg�������õ�������handle_client_msg�����ĵ�ǰ�¼��ĸ���
public:
	MyEpoll() : Multiplexing() { events = NULL; epollfd = -1; }
	MyEpoll(int max, int listenfd);
	~MyEpoll() {
		if (events) {
			delete events;
			events = NULL;
		}
	}

	void server_do_multiplexing();
	void client_do_multiplexing();
	void handle_client_msg();
	bool accept_client_proc();
	bool add_event(int confd, int event);
	bool delete_event(int confd, int event);
	int wait_event(); // �ȴ��¼�
};

#endif // !MULTIPLEXING_H