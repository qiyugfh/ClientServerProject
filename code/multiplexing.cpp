#include "multiplexing.h"

//--------------------------select------------------
MySelect::MySelect(int max, int listenfd) : Multiplexing(max, listenfd) {
	this->allfds = new fd_set[this->io_max];
	this->clifds = new int[this->io_max];
	if (NULL == this->allfds || NULL == this->clifds) {
		perror("initialization failed!");
		exit(-1);
	}
	this->cli_cnt = 0;
	this->maxfd = 0;

	//��ʼ���ͻ�����������
	int i;
	for (i = 0; i < io_max; i++) {
		this->clifds[i] = -1;
	}
}

void MySelect::server_do_multiplexing() {
	int  nready = 0;
	int i = 0;

	while (1) {
		//���³�ʼ��fd_set���� -- ������poll��ͬ
		init_event();

		/*��ʼ��ѯ���մ������˺Ϳͻ����׽���*/
		nready = wait_event();
		if (-1 == nready) return;
		if (0 == nready) continue;

		if (FD_ISSET(this->listenfd, this->allfds)) {
			/*�����ͻ�������*/
			if (!accept_client_proc())  //������������
				continue;
			if (--nready <= 0) //˵����ʱ�������¼�����С�ڵ���1�����Բ����ٴ�������Ŀͻ�������Ϣ
				continue;
		}

		/*���ܴ���ͻ�����Ϣ*/
		handle_client_msg();
	}
}

void MySelect::client_do_multiplexing() {
	char sendline[MAXLINE], recvline[MAXLINE];
	int n;
	this->maxfd = -1;
	int nready = -1;
	if (this->io_max < 2) {
		perror("please increase the max number of io!");
		exit(1);
	}

	//�������������
	if (!add_event(this->listenfd, -1)) {
		perror("add event error!");
		exit(1);
	}

	//��ӱ�׼����������
	if (!add_event(STDIN_FILENO, -1)) {
		perror("add event error!");
		exit(1);
	}

	while (1) {
		//���³�ʼ��fd_set���� -- ������poll��ͬ
		init_event();

		//�ȴ��¼�����
		nready = wait_event();
		if (-1 == nready) return;
		if (0 == nready) continue;

		//�Ƿ��пͻ���Ϣ׼����
		if (FD_ISSET(this->listenfd, this->allfds)) {
			n = read(this->listenfd, recvline, MAXLINE);
			if (n <= 0) {
				fprintf(stderr, "client: server is closed.\n");
				close(this->listenfd);
				return;
			}

			write(STDOUT_FILENO, recvline, n);
		}
		//���Ա�׼�����Ƿ�׼����
		if (FD_ISSET(STDIN_FILENO, this->allfds)) {
			n = read(STDIN_FILENO, sendline, MAXLINE);
			if (n <= 0) {
				shutdown(this->listenfd, SHUT_WR);
				continue;
			}
			write(this->listenfd, sendline, n);
		}
	}
}

bool MySelect::init_event() {
	FD_ZERO(this->allfds); //���������ļ�������

						   /*��Ӽ����׽���*/
	FD_SET(this->listenfd, this->allfds);
	this->maxfd = this->listenfd;

	int i;
	int  clifd = -1;
	/*��ӿͻ����׽���*/
	for (i = 0; i < this->cli_cnt; i++) {
		clifd = this->clifds[i];
		/*ȥ����Ч�Ŀͻ��˾��*/
		if (clifd != -1) {
			FD_SET(clifd, this->allfds);
		}
		this->maxfd = (clifd > this->maxfd ? clifd : this->maxfd);
	}
}

bool MySelect::accept_client_proc() {
	struct sockaddr_in cliaddr;
	socklen_t cliaddrlen;
	cliaddrlen = sizeof(cliaddr);
	int connfd;

	//�����µ�����
	if ((connfd = accept(this->listenfd, (struct sockaddr*)&cliaddr, &cliaddrlen)) == -1) {
		if (errno == EINTR)
			return false;
		else {
			perror("accept error:");
			exit(1);
		}
	}
	fprintf(stdout, "accept a new client: %s:%d\n", inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
	return add_event(connfd, -1); //����µ�������
}

bool MySelect::add_event(int connfd, int event) { //��select��event��û������
												  //���µ�������������ӵ�������
	int i = 0;
	for (i = 0; i < io_max; i++) {
		if (this->clifds[i] < 0) {
			this->clifds[i] = connfd;
			this->cli_cnt++;
			break;
		}
	}

	if (i == io_max) {
		fprintf(stderr, "too many clients.\n");
		return false;
	}

	//���µ���������ӵ���������������
	FD_SET(connfd, this->allfds);
	if (connfd > this->maxfd) this->maxfd = connfd;
	return true;
}

void MySelect::handle_client_msg() {
	int i = 0, n = 0;
	int clifd;
	char buf[MAXLINE];
	memset(buf, 0, MAXLINE);

	//������Ϣ
	for (i = 0; i <= this->cli_cnt; i++) {
		clifd = this->clifds[i];
		if (clifd < 0) {
			continue;
		}

		/*�жϿͻ����׽����Ƿ�������*/
		if (FD_ISSET(clifd, this->allfds)) {
			//���տͻ��˷��͵���Ϣ
			n = read(clifd, buf, MAXLINE);

			if (n <= 0) {
				/*n==0��ʾ��ȡ��ɣ��ͻ����ر��׽���*/
				FD_CLR(clifd, this->allfds);
				close(clifd);
				this->clifds[i] = -1;
				continue;
			}

			//��д����
			write(STDOUT_FILENO, buf, n);
			write(clifd, buf, n);
			return;
		}
	}
}

int MySelect::wait_event() {
	struct timeval tv;

	/*ÿ�ε���selectǰ��Ҫ���������ļ���������ʱ�䣬��Ϊ�¼��������ļ���������ʱ�䶼���ں��޸���*/
	tv.tv_sec = 30;
	tv.tv_usec = 0;

	/*��ʼ��ѯ���մ������˺Ϳͻ����׽���*/
	int nready = select(this->maxfd + 1, this->allfds, NULL, NULL, &tv);
	if (nready == -1) {
		fprintf(stderr, "select error:%s.\n", strerror(errno));
	}
	if (nready == 0) {
		fprintf(stdout, "select is timeout.\n");
	}
	return nready;
}

//-----------------poll-------------------------
MyPoll::MyPoll(int max, int listenfd) : Multiplexing(max, listenfd) {
	this->clientfds = new Pollfd[this->io_max];
	//��ʼ���ͻ�����������
	int i;
	for (i = 0; i < io_max; i++) {
		this->clientfds[i].fd = -1;
	}
	this->max_index = -1;
}

void MyPoll::server_do_multiplexing() {
	int sockfd;
	int i;
	int nready;
	this->max_index = -1;

	//ע�⣺��Ҫ����������������ڵ�һ��λ��
	if (!add_event(this->listenfd, POLLIN)) {
		perror("add listen event error!");
		return;
	}

	//ѭ������
	while (1) {
		//�ȴ��¼�����ȡ�����������ĸ���
		nready = wait_event();
		if (nready == -1) {
			return;
		}
		if (nready == 0) {
			continue;
		}

		//���Լ����������Ƿ�׼����
		if (this->clientfds[0].revents & POLLIN) {
			if (!accept_client_proc())  //������������
				continue;
			if (--nready <= 0) //˵����ʱ�������¼�����С�ڵ���1�����Բ����ٴ�������Ŀͻ�������Ϣ
				continue;
		}

		//����ͻ�����
		handle_client_msg();
	}
}

void MyPoll::client_do_multiplexing() {
	char    sendline[MAXLINE], recvline[MAXLINE];
	int n;
	this->max_index = -1;
	int nready = -1;
	if (this->io_max < 2) {
		perror("please increase the max number of io!");
		exit(1);
	}

	//�������������
	if (!add_event(this->listenfd, POLLIN)) {
		perror("add event error!");
		exit(1);
	}

	//��ӱ�׼����������
	if (!add_event(STDIN_FILENO, POLLIN)) {
		perror("add event error!");
		exit(1);
	}

	while (1) {
		//�ȴ��¼�����
		nready = wait_event();
		if (-1 == nready) return;
		if (0 == nready) continue;

		//�Ƿ��пͻ���Ϣ׼����
		if (this->clientfds[0].revents & POLLIN) {
			n = read(this->listenfd, recvline, MAXLINE);
			if (n <= 0) {
				fprintf(stderr, "client: server is closed.\n");
				close(this->listenfd);
				return;
			}
			write(STDOUT_FILENO, recvline, n);
		}
		//���Ա�׼�����Ƿ�׼����
		if (this->clientfds[1].revents & POLLIN) {
			n = read(STDIN_FILENO, sendline, MAXLINE);
			if (n <= 0) {
				shutdown(this->listenfd, SHUT_WR);
				continue;
			}
			write(this->listenfd, sendline, n);
		}
	}
}

bool MyPoll::accept_client_proc() {
	struct sockaddr_in cliaddr;
	socklen_t cliaddrlen;
	cliaddrlen = sizeof(cliaddr);
	int connfd;

	//�����µ�����
	if ((connfd = accept(this->listenfd, (struct sockaddr*)&cliaddr, &cliaddrlen)) == -1) {
		if (errno == EINTR)
			return false;
		else {
			perror("accept error:");
			exit(1);
		}
	}
	fprintf(stdout, "accept a new client: %s:%d\n", inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
	return add_event(connfd, POLLIN); //����µ�������
}

bool MyPoll::add_event(int connfd, int event) {
	//���µ�������������ӵ�������
	int i;
	for (i = 0; i < io_max; i++) {
		if (this->clientfds[i].fd < 0) {
			this->clientfds[i].fd = connfd;
			break;
		}
	}

	if (i == io_max) {
		fprintf(stderr, "too many clients.\n");
		return false;
	}

	//���µ���������ӵ���������������
	this->clientfds[i].events = event;
	if (i > this->max_index) this->max_index = i;
	return true;
}

void MyPoll::handle_client_msg() {
	int i, n;
	char buf[MAXLINE];
	memset(buf, 0, MAXLINE);

	//������Ϣ
	for (i = 1; i <= this->max_index; i++) {
		if (this->clientfds[i].fd < 0)
			continue;

		//���Կͻ��������Ƿ�׼����
		if (this->clientfds[i].revents & POLLIN) {
			//���տͻ��˷��͵���Ϣ
			n = read(this->clientfds[i].fd, buf, MAXLINE);
			if (n <= 0) {
				close(this->clientfds[i].fd);
				this->clientfds[i].fd = -1;
				continue;
			}

			write(STDOUT_FILENO, buf, n);
			//��ͻ��˷���buf
			write(this->clientfds[i].fd, buf, n);
		}
	}
}

int MyPoll::wait_event() {
	/*��ʼ��ѯ���մ������˺Ϳͻ����׽���*/
	int nready = nready = poll(this->clientfds, this->max_index + 1, INFTIM);
	if (nready == -1) {
		fprintf(stderr, "poll error:%s.\n", strerror(errno));
	}
	if (nready == 0) {
		fprintf(stdout, "poll is timeout.\n");
	}
	return nready;
}

//------------------------epoll---------------------------
MyEpoll::MyEpoll(int max, int listenfd) : Multiplexing(max, listenfd) {
	this->events = new Epoll_event[this->io_max];
	//����һ��������
	this->epollfd = epoll_create(this->io_max);
}

void MyEpoll::server_do_multiplexing() {
	int i, fd;
	int nready;
	char buf[MAXLINE];
	memset(buf, 0, MAXLINE);

	//��Ӽ����������¼�
	if (!add_event(this->listenfd, EPOLLIN)) {
		perror("add event error!");
		exit(1);
	}
	while (1) {
		//��ȡ�Ѿ�׼���õ��������¼�
		nready = wait_event();
		this->nready = nready;
		if (-1 == nready) return;
		if (0 == nready) continue;

		//���б���
		/**�����poll��select����ͬ����Ϊ������ֱ���жϼ������¼��Ƿ������
		������Ҫһ��forѭ�����������forѭ��+�ж�������poll�� if (FD_ISSET(this->listenfd, this->allfds))��
		select�е�if (this->clientfds[0].revents & POLLIN)
		����ֻ�Ǿ���д�ĸ�poll��select�еĽṹ���ƣ�����ʵ�ʴ����У���Ӧ����ôд����ôд�����һ��forѭ��**/
		for (i = 0; i < nready; i++) {
			fd = events[i].data.fd;
			//���������������ͺ��¼����ͽ��д���
			if ((fd == this->listenfd) && (events[i].events & EPOLLIN)) {  //�����¼�
																		   /*�����ͻ�������*/
				if (!accept_client_proc())  //������������
					continue;
				if (--nready <= 0) //˵����ʱ�������¼�����С�ڵ���1�����Բ����ٴ�������Ŀͻ�������Ϣ
					continue;
			}
		}

		//����ͻ����¼�
		handle_client_msg();
	}
	close(epollfd);
}

bool MyEpoll::accept_client_proc() {
	struct sockaddr_in cliaddr;
	socklen_t cliaddrlen;
	cliaddrlen = sizeof(cliaddr);
	int connfd;

	//�����µ�����
	if ((connfd = accept(this->listenfd, (struct sockaddr*)&cliaddr, &cliaddrlen)) == -1) {
		if (errno == EINTR)
			return false;
		else {
			perror("accept error:");
			exit(1);
		}
	}
	fprintf(stdout, "accept a new client: %s:%d\n", inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
	return add_event(connfd, EPOLLIN); //����µ�������
}

void MyEpoll::client_do_multiplexing() {
	char    sendline[MAXLINE], recvline[MAXLINE];
	int n;
	int nready = -1;
	int i, fd;
	if (this->io_max < 2) {
		perror("please increase the max number of io!");
		exit(1);
	}

	//�������������
	if (!add_event(this->listenfd, POLLIN)) {
		perror("add event error!");
		exit(1);
	}

	//��ӱ�׼����������
	if (!add_event(STDIN_FILENO, POLLIN)) {
		perror("add event error!");
		exit(1);
	}

	while (1) {
		//�ȴ��¼�����
		nready = wait_event();
		if (-1 == nready) return;
		if (0 == nready) continue;

		for (i = 0; i < nready; i++) {
			fd = events[i].data.fd;
			//���������������ͺ��¼����ͽ��д���
			if ((fd == this->listenfd) && (events[i].events & EPOLLIN)) {  //�����¼�
				n = read(this->listenfd, recvline, MAXLINE);
				if (n <= 0) {
					fprintf(stderr, "client: server is closed.\n");
					close(this->listenfd);
					return;
				}
				write(STDOUT_FILENO, recvline, n);
			}
			else {
				n = read(STDIN_FILENO, sendline, MAXLINE);
				if (n <= 0) {
					shutdown(this->listenfd, SHUT_WR);
					continue;
				}
				write(this->listenfd, sendline, n);
			}
		}
	}
}

bool MyEpoll::add_event(int connfd, int event) {
	//���µ���������ӵ���������������
	Epoll_event ev;
	ev.events = event;
	ev.data.fd = connfd;
	return epoll_ctl(this->epollfd, EPOLL_CTL_ADD, connfd, &ev) == 0;
}

void MyEpoll::handle_client_msg() {
	int i, fd;
	char buf[MAXLINE];
	memset(buf, 0, MAXLINE);

	//������Ϣ
	for (i = 0; i <= this->nready; i++) {
		fd = this->events[i].data.fd;
		if (fd == this->listenfd)
			continue;

		if (events[i].events & EPOLLIN) {
			int n = read(fd, buf, MAXLINE);
			if (n <= 0) {
				perror("read error:");
				close(fd);
				delete_event(fd, EPOLLIN);
			}
			else {
				write(STDOUT_FILENO, buf, n);
				//��ͻ��˷���buf
				write(fd, buf, strlen(buf));
			}
		}
	}
}

int MyEpoll::wait_event() {
	/*��ʼ��ѯ���մ������˺Ϳͻ����׽���*/
	int nready = epoll_wait(this->epollfd, this->events, this->io_max, INFTIM);;
	if (nready == -1) {
		fprintf(stderr, "poll error:%s.\n", strerror(errno));
	}
	if (nready == 0) {
		fprintf(stdout, "poll is timeout.\n");
	}
	return nready;
}

bool MyEpoll::delete_event(int fd, int state) {
	Epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;
	return epoll_ctl(this->epollfd, EPOLL_CTL_DEL, fd, &ev) == 0;
}