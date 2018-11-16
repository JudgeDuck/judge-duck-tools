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

double getUnixTime() {
    struct timespec tv;
    if(clock_gettime(CLOCK_REALTIME, &tv) != 0) return 0;
    return (tv.tv_sec + (tv.tv_nsec / 1000000000.0));
}

size_t fileSize(const char *fn) {
	struct stat st;
	if (stat(fn, &st))
		return 0;
	return st.st_size;
}

#include <unistd.h>

void sendPacket(string content) {
	printf("sendPacket %s\n", content.c_str());
	ts << content.c_str();
	ts.flush();
}

void sendFile(const char *sfn, const char *tfn) {
	char buf[5120];
	printf("send %s -> %s\n", sfn, tfn);
	double time_start = getUnixTime();
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
	
	const int WINDOWSIZE = 64;
	
	const int ECHO_CNT = 300 * 30;
	int n_pkt = 0;
	
	while (todo.size()) {
		printf("remaining %d\n", (int) todo.size()), fflush(stdout);
		vector<pii> todo_copy(todo.begin(), todo.end());
		int online = 0;
		for (pii p: todo_copy) {
			++online;
			if (++n_pkt % ECHO_CNT == 0) {
				printf("send %d | %d\n", p.first, online), fflush(stdout);
			}
			memcpy(buf + 4, &p.first, 4);
			memcpy(buf + 8, fileAll + p.first, p.second);
			buf[0] = 'g';
			buf[1] = 'g';
			buf[2] = 'g';
			buf[3] = 'g';
			ds.writeRawData(buf, p.second + 8);
			if ((WINDOWSIZE - online) % 10 == 0) sock_b.flush();
			if (online < WINDOWSIZE) continue;
			
			while (sock_b.waitForReadyRead(online >= WINDOWSIZE ? 1000 : 0)) {
				int len = sock_b.readDatagram(buf, sizeof(buf));
				buf[len] = 0;
				if (buf[0] != 'a') {
					printf("ack wrong %s\n", buf), fflush(stdout);
					continue;
				}
				int off; memcpy(&off, buf + 4, 4);
				--online;
				if (++n_pkt % ECHO_CNT == 0) {
					printf("ack %d | %d\n", off, online), fflush(stdout);
				}
				auto it = todo.lower_bound(pii(off, 0));
				if (it != todo.end() && it->first == off) todo.erase(it);
				if (!todo.size()) break;
			}
			if (online >= WINDOWSIZE) break;
		}
		sock_b.flush();
		if (todo.size()) while (sock_b.waitForReadyRead(1000)) {
			int len = sock_b.readDatagram(buf, sizeof(buf));
			buf[len] = 0;
			if (buf[0] != 'a') {
				printf("ack wrong %s\n", buf), fflush(stdout);
				continue;
			}
			int off; memcpy(&off, buf + 4, 4);
			if (++n_pkt % ECHO_CNT == 0) {
				printf("ack %d\n", off), fflush(stdout);
			}
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
			if (buf[0] == 's') {
				printf("sync ok\n"), fflush(stdout);
				double dt = max((getUnixTime() - time_start) * 1000.0, 1.0);
				printf("Sent %d bytes in %.0lf ms (%.0lf KB/s)\n", sz, dt, sz / (dt * 0.001) / 1024.0), fflush(stdout);
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

void sendObj(const char *filename, const char *md5) {
	char buf[5120];
	printf("send obj %s %s\n", filename, md5);
	double time_start = getUnixTime();
	string fn = string("sendobj xxxx") + md5;
	int sz = fileSize(filename);
	
	char tmp[4];
	memcpy(tmp, &sz, 4);
	for (int i = 0; i < 4; i++) fn[i + 8] = tmp[i];
	
	// qDebug() << bs.readAll();
	// ts << fn.c_str();
	ds.writeRawData(fn.c_str(), fn.length());
	ts.flush();
		// bs.readAll();
	while (1) {
		while (sock_b.waitForReadyRead(1000)) {
			int len = sock_b.readDatagram(buf, sizeof(buf));
			buf[len] = 0;
			if (buf[0] == 's') {
				printf("sync ok\n"), fflush(stdout);
				goto next;
			} else if (buf[0] == 'g') {
				printf("cache hit !!!\n"), fflush(stdout);
				return;
			} else {
				printf("sync fail %s\n", buf);
			}
		}
	}
next:;
	char *fileAll = new char[sz];
	FILE *fin = fopen(filename, "rb");
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
	
	const int WINDOWSIZE = 64;
	
	const int ECHO_CNT = 300 * 30;
	int n_pkt = 0;
	
	while (todo.size()) {
		printf("remaining %d\n", (int) todo.size()), fflush(stdout);
		vector<pii> todo_copy(todo.begin(), todo.end());
		int online = 0;
		for (pii p: todo_copy) {
			++online;
			if (++n_pkt % ECHO_CNT == 0) {
				printf("send %d | %d\n", p.first, online), fflush(stdout);
			}
			memcpy(buf + 4, &p.first, 4);
			memcpy(buf + 8, fileAll + p.first, p.second);
			buf[0] = 'g';
			buf[1] = 'g';
			buf[2] = 'g';
			buf[3] = 'g';
			ds.writeRawData(buf, p.second + 8);
			if ((WINDOWSIZE - online) % 10 == 0) sock_b.flush();
			if (online < WINDOWSIZE) continue;
			
			while (sock_b.waitForReadyRead(online >= WINDOWSIZE ? 1000 : 0)) {
				int len = sock_b.readDatagram(buf, sizeof(buf));
				buf[len] = 0;
				if (buf[0] != 'a') {
					printf("ack wrong %s\n", buf), fflush(stdout);
					continue;
				}
				int off; memcpy(&off, buf + 4, 4);
				--online;
				if (++n_pkt % ECHO_CNT == 0) {
					printf("ack %d | %d\n", off, online), fflush(stdout);
				}
				auto it = todo.lower_bound(pii(off, 0));
				if (it != todo.end() && it->first == off) todo.erase(it);
				if (!todo.size()) break;
			}
			if (online >= WINDOWSIZE) break;
		}
		sock_b.flush();
		if (todo.size()) while (sock_b.waitForReadyRead(1000)) {
			int len = sock_b.readDatagram(buf, sizeof(buf));
			buf[len] = 0;
			if (buf[0] != 'a') {
				printf("ack wrong %s\n", buf), fflush(stdout);
				continue;
			}
			int off; memcpy(&off, buf + 4, 4);
			if (++n_pkt % ECHO_CNT == 0) {
				printf("ack %d\n", off), fflush(stdout);
			}
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
			if (buf[0] == 's') {
				printf("sync ok\n"), fflush(stdout);
				double dt = max((getUnixTime() - time_start) * 1000.0, 1.0);
				printf("Sent %d bytes in %.0lf ms (%.0lf KB/s)\n", sz, dt, sz / (dt * 0.001) / 1024.0), fflush(stdout);
				return;
			}
			else printf("sync fail %s\n", buf);
		}
	}
}

string getFileMD5(string filename) {
	char tmp_name[11];
	tmp_name[10] = 0;
	for (int i = 0; i < 10; i++) {
		tmp_name[i] = rand() % 26 + 'a';
	}
	system(("md5sum " + filename + " | awk {'print $1'} > /tmp/" + string(tmp_name)).c_str());
	string ret = localFileContent((string("/tmp/") + tmp_name).c_str()).toLatin1().constData();
	if (ret.length() > 0 && ret[ret.length() - 1] == '\n') ret = ret.substr(0, ret.length() - 1);
	return ret;
}

void setObj(char type, const char *md5) {
	printf("setObj %c %s\n", type, md5);
	ts << (string("setobj_") + type + string(md5)).c_str();
	ts.flush();
	sock_b.waitForReadyRead();
	// bs.readAll();
	char buf[5120];
	sock_b.waitForReadyRead(1000);
	sock_b.readDatagram(buf, sizeof(buf));
	printf("setObj ok\n");
}

void sendDataFiles(const char *in_file, const char *ans_file) {
	string in_md5 = getFileMD5(in_file);
	string ans_md5 = getFileMD5(ans_file);
	sendObj(in_file, in_md5.c_str());
	sendObj(ans_file, ans_md5.c_str());
	setObj('I', in_md5.c_str());
	setObj('A', ans_md5.c_str());
}

QString judgeFile(string ip, int local_port, string input_file, string answer_file, string binary_file) {
	size_t sz = fileSize(binary_file.c_str());
	if (sz <= 0) return "Binary too small";
	if (sz >= 20480 * 1024) return "Binary too large";
	
	qDebug() << sock_b.bind(local_port);
	sock_c.connectToHost(ip.c_str(), 8000);
	qDebug() << sock_c.waitForConnected();
	//bs.setDevice(&sock_b);
	ts.setDevice(&sock_c);
	ds.setDevice(&sock_c);
	
	sendPacket("clear");
	
	sendDataFiles(input_file.c_str(), answer_file.c_str());
	
	sendFile(binary_file.c_str(), "judging");
	runCmd("arbiter judging " + to_string(time_ns) + " " + to_string(mem_kb) + " > arbiter.out");
	return fileContent("arbiter.out");
}

void init_rand() {
	FILE *f = fopen("/dev/urandom", "rb");
	int x;
	fread((void *) &x, 4, 1, f);
	srand(x);
	fclose(f);
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
	
	init_rand();
	
	qout << judgeFile(ip, local_port, input_file, answer_file, binary_file);
	qout.flush();
	
	return 0;
}

