#include <QCoreApplication>
// #include <QTcpSocket>
#include <QUdpSocket>
#include <QTextStream>
#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

QUdpSocket sock_b, sock_c;
QTextStream ts;
QDataStream ds;

#define printf(x...) fprintf(stderr, x)

size_t fileSize(const char *fn) {
	struct stat st;
	if (stat(fn, &st))
		return 0;
	return st.st_size;
}

#include <unistd.h>

void sendFile(const char *sfn, const char *tfn) {
	char buf[5120];
	printf("send %s -> %s\n", sfn, tfn);
	string fn = string("f") + tfn;
	// qDebug() << bs.readAll();
	ts << fn.c_str();
	ts.flush();
		// bs.readAll();
	while (1) {
		while (sock_b.waitForReadyRead(1000)) {
			int len = sock_b.readDatagram(buf, sizeof(buf));
			buf[len] = 0;
			if (buf[0] == 'f') {
				printf("sync ok\n"), fflush(stdout);
				goto next;
			} else {
				printf("sync fail %s\n", buf);
			}
		}
	}
next:;
	int sz = fileSize(sfn);
	char *fileAll = new char[sz];
	FILE *fin = fopen(sfn, "rb");
	if (fin) {
		fread(fileAll, 1, sz, fin);
		fclose(fin);
	}
	printf("filesize %d\n", (int) sz), fflush(stdout);
	
	typedef pair<int, int> pii; // (off, len)
	set<pii> todo;
	const int PKSIZE = 1450;
	for(int i = 0; i < sz; i += PKSIZE)
		todo.insert(pii(i, min(sz - i, PKSIZE)));
	// printf("total todo %d\n", (int) todo.size()), fflush(stdout);
	
	while (todo.size()) {
		printf("remaining %d\n", (int) todo.size()), fflush(stdout);
		vector<pii> todo_copy(todo.begin(), todo.end());
		int online = 0;
		for (pii p: todo_copy) {
			if (online >= 20) break;
			++online;
			printf("send %d | %d\n", p.first, online), fflush(stdout);
			memcpy(buf + 4, &p.first, 4);
			memcpy(buf + 8, fileAll + p.first, p.second);
			buf[0] = 'g';
			buf[1] = 'g';
			buf[2] = 'g';
			buf[3] = 'g';
			ds.writeRawData(buf, p.second + 8);
			sock_b.flush();
			// usleep(10000);
			while (sock_b.waitForReadyRead(online >= 20 ? 1000 : 1)) {
				int len = sock_b.readDatagram(buf, sizeof(buf));
				buf[len] = 0;
				if (buf[0] != 'a') {
					printf("ack wrong %s\n", buf), fflush(stdout);
					continue;
				}
				int off; memcpy(&off, buf + 4, 4);
				--online;
				printf("ack %d | %d\n", off, online), fflush(stdout);
				auto it = todo.lower_bound(pii(off, 0));
				if (it != todo.end() && it->first == off) todo.erase(it);
				if (!todo.size()) break;
			}
		}
		if (todo.size()) while (sock_b.waitForReadyRead(1000)) {
			int len = sock_b.readDatagram(buf, sizeof(buf));
			buf[len] = 0;
			if (buf[0] != 'a') {
				printf("ack wrong %s\n", buf), fflush(stdout);
				continue;
			}
			int off; memcpy(&off, buf + 4, 4);
			printf("ack %d\n", off), fflush(stdout);
			auto it = todo.lower_bound(pii(off, 0));
			if(it != todo.end() && it->first == off) todo.erase(it);
			if(!todo.size()) break;
		}
	}
	delete[] fileAll;
	printf("send ok, waiting sync\n"), fflush(stdout);
	while (1) {
		ts << "e";
		ts.flush();
		// bs.readAll();
		while (sock_b.waitForReadyRead(1000)) {
			int len = sock_b.readDatagram(buf, sizeof(buf));
			buf[len] = 0;
			if (buf[0] == 'o') {
				printf("sync ok\n"), fflush(stdout);
				return;
			}
			else printf("sync fail %s\n", buf);
		}
	}
}
void runCmd(string cmd) {
	printf("runCmd %s\n", cmd.c_str());
	cmd = "c" + cmd;
	ts << cmd.c_str();
	ts.flush();
	sock_b.waitForReadyRead();
	// bs.readAll();
	char buf[5120];
	sock_b.waitForReadyRead(1000);
	sock_b.readDatagram(buf, sizeof(buf));
	printf("runCmd ok\n");
}

QString fileContent(string fn) {
	printf("fileContent %s\n", fn.c_str());
	fn = "o" + fn;
	ts << fn.c_str();
	ts.flush();
	QString ret = "";
	while (1) {
		sock_b.waitForReadyRead();
		char buf[2048];
		int len = sock_b.readDatagram(buf, sizeof(buf));
		if (len == 1 && buf[0] == 'F') break;
		//ret += QString(buf + 1, len - 1);
		for (int i = 1; i < len; i++) ret += QChar(buf[i]);
	}
	//sock_b.waitForReadyRead();
	//QString ret = bs.readAll();
	printf("fileContent ok:\n%s", ret.toStdString().c_str());
	return ret;
}

QString localFileContent(QString fn) {
	QFile file(fn);
	if (!file.open(QIODevice::ReadOnly)) {
		return "";
	}
	QTextStream ts(&file);
	return ts.readAll();
}

QTextStream qout(stdout);
long long time_ns; int mem_kb;

QString judgeFile(string ip, int local_port, string input_file, string answer_file, string binary_file) {
	size_t sz = fileSize(binary_file.c_str());
	if (sz <= 0) return "Binary too small";
	if (sz >= 2048 * 1024) return "Binary too large";
	
	qDebug() << sock_b.bind(local_port);
	sock_c.connectToHost(ip.c_str(), 8000);
	qDebug() << sock_c.waitForConnected();
	//bs.setDevice(&sock_b);
	ts.setDevice(&sock_c);
	ds.setDevice(&sock_c);
	
	sendFile(input_file.c_str(), "input.txt");
	sendFile(answer_file.c_str(), "answer.txt");
	
	sendFile(binary_file.c_str(), "judging");
	runCmd("arbiter judging " + to_string(time_ns) + " " + to_string(mem_kb) + " > arbiter.out");
	return fileContent("arbiter.out");
}

int main(int argc, char **argv) {
	QCoreApplication a(argc, argv);
	
	if (argc != 1 + 7) {
		qout << "usage: run [ip] [local-port] [input-file] [answer-file] [binary-file] [time_ns] [mem_kb]\n";
		qout.flush();
		return 1;
	}
	string ip = argv[1];
	int local_port = atoll(argv[2]);
	string input_file = argv[3];
	string answer_file = argv[4];
	string binary_file = argv[5];
	time_ns = atoll(argv[6]);
	mem_kb = atoi(argv[7]);
	
	qout << judgeFile(ip, local_port, input_file, answer_file, binary_file);
	qout.flush();
	
	return 0;
}
