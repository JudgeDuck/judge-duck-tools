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

const int PACKET_SIZE = 1400;
const double RTT_ms = 3;
const double RTT_s = RTT_ms * 0.001;

#define MAX_STDOUT_LEN (64 * 1024 * 1024)

void sendPacket(string content) {
	printf("sendPacket %s\n", content.c_str());
	ts << content.c_str();
	ts.flush();
}

void send_payload(char *buf, int sz, char *payload, double time_start) {
	typedef pair<int, int> pii;
	typedef pair<double, pii> pdii;
	set<int> todo_set;
	priority_queue<pdii, vector<pdii>, greater<pdii> > todo_queue;
	
	for (int i = 0; i < sz; i += PACKET_SIZE) {
		todo_set.insert(i);
		todo_queue.push(pdii(time_start, pii(i, min(sz - i, PACKET_SIZE))));
	}
	
	double next_report_t = getUnixTime();
	int n_cur_sent = 0;
	while (!todo_queue.empty()) {
		if (getUnixTime() >= next_report_t) {
			printf("set_size %d, queue_size %d, cur_sent %d\n", (int) todo_set.size(), (int) todo_queue.size(), n_cur_sent);
			next_report_t += 1.0;
			n_cur_sent = 0;
		}
		if (sock_b.waitForReadyRead(0)) {
			int len = sock_b.readDatagram(buf, sizeof(buf));
			if (len <= 5) continue;
			buf[len] = 0;
			if (buf[0] != 'a') {
				printf("ack wrong %s\n", buf), fflush(stdout);
				continue;
			}
			int off; memcpy(&off, buf + 4, 4);
			if (todo_set.count(off) == 0) continue;
			todo_set.erase(off);
		} else {
			pdii p = todo_queue.top();
			
			int off = p.second.first;
			int len = p.second.second;
			if (todo_set.count(off) == 0) {
				todo_queue.pop();
				continue;
			}
			
			double t = p.first;
			double cur_t = getUnixTime();
			if (cur_t < t) {
				continue;
			}
			
			todo_queue.pop();
			todo_queue.push(pdii(t + RTT_s, p.second));
			
			memcpy(buf + 4, &off, 4);
			memcpy(buf + 8, payload + off, len);
			buf[0] = 'g';
			buf[1] = 'g';
			buf[2] = 'g';
			buf[3] = 'g';
			sock_c.write(buf, len + 8);
			sock_c.flush();
			++n_cur_sent;
		}
	}
	
	delete[] payload;
	printf("send ok, waiting sync\n"), fflush(stdout);
	while (1) {
		ts << "e";
		ts.flush();
		bool ok = false;
		while (sock_b.waitForReadyRead(RTT_ms + 1)) {
			int len = sock_b.readDatagram(buf, sizeof(buf));
			buf[len] = 0;
			if (buf[0] == 's') {
				printf("sync ok\n"), fflush(stdout);
				ok = true;
			}
		}
		if (ok) {
			double dt = max((getUnixTime() - time_start) * 1000.0, 1.0);
			printf("Sent %d bytes in %.0lf ms (%.0lf KB/s)\n", sz, dt, sz / (dt * 0.001) / 1024.0), fflush(stdout);
			break;
		}
	}
}

void sendFile(const char *sfn, const char *tfn) {
	char buf[5120];
	printf("send %s -> %s\n", sfn, tfn);
	int sz = fileSize(sfn);
	char *fileAll = new char[sz];
	FILE *fin = fopen(sfn, "rb");
	if (fin) {
		fread(fileAll, 1, sz, fin);
		fclose(fin);
	}
	printf("filesize %d\n", (int) sz), fflush(stdout);
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
	send_payload(buf, sz, fileAll, time_start);
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
	std::string str;
	while (1) {
		sock_b.waitForReadyRead();
		char buf[2048];
		int len = sock_b.readDatagram(buf, sizeof(buf));
		if (len == 1 && buf[0] == 'F') break;
		//ret += QString(buf + 1, len - 1);
		for (int i = 1; i < len; i++) str += buf[i];
	}
	ret = QString::fromStdString(str);
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
	string fn = string("sendobj xxxx") + md5;
	int sz = fileSize(filename);
	char *fileAll = new char[sz];
	FILE *fin = fopen(filename, "rb");
	if (fin) {
		fread(fileAll, 1, sz, fin);
		fclose(fin);
	}
	printf("filesize %d\n", (int) sz), fflush(stdout);
	double time_start = getUnixTime();
	
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
	send_payload(buf, sz, fileAll, time_start);
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
	
	while (1) {
		if (system(("ping " + ip + " -c 1 -t 1 | grep ttl=233 > /dev/null 2>&1").c_str()) == 0) break;
		sleep(1);
	}
	
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

void get_stdout(const char *stdout_save_path) {
	static char buf[2333];
	
	double time_start = getUnixTime();
	
	sock_c.write("stdout-query");
	sock_c.flush();
	
	unsigned stdout_len = 0;
	
	while (sock_b.waitForReadyRead(RTT_ms)) {
		int len = sock_b.readDatagram(buf, sizeof(buf));
		if (len != 8) continue;
		if (strncmp(buf, "slen", 4) != 0) {
			continue;
		} 
		
		memcpy(&stdout_len, buf + 4, 4);
		if (stdout_len > MAX_STDOUT_LEN) {
			stdout_len = MAX_STDOUT_LEN;
		}
		
		break;
	}
	
	printf("stdout_len = %u\n", stdout_len);
	int sz = stdout_len;
	
	char *stdout_content = new char[stdout_len + 1];
	typedef pair<int, int> pii;
	typedef pair<double, pii> pdii;
	set<int> todo_set;
	priority_queue<pdii, vector<pdii>, greater<pdii> > todo_queue;
	
	for (int i = 0; i < sz; i += PACKET_SIZE) {
		todo_set.insert(i);
		todo_queue.push(pdii(time_start, pii(i, min(sz - i, PACKET_SIZE))));
	}
	
	double next_report_t = getUnixTime();
	int n_cur_requested = 0;
	while (!todo_queue.empty()) {
		if (getUnixTime() >= next_report_t) {
			printf("set_size %d, queue_size %d, cur_requested %d\n",
				(int) todo_set.size(), (int) todo_queue.size(), n_cur_requested);
			next_report_t += 1.0;
			n_cur_requested = 0;
		}
		if (sock_b.waitForReadyRead(0)) {
			int len = sock_b.readDatagram(buf, sizeof(buf));
			buf[len] = 0;
			
			if (len <= 18) continue;
			if (strncmp(buf, "s content ", 10) != 0) {
				continue;
			}
			
			unsigned recv_off, recv_len;
			memcpy(&recv_off, buf + 10, 4);
			memcpy(&recv_len, buf + 14, 4);
			
			if (recv_off > stdout_len) {
				continue;
			}
			if (recv_len > stdout_len || recv_off + recv_len > stdout_len) {
				continue;
			}
			if (recv_len != (unsigned) (len - 18)) {
				continue;
			}
			if (min(sz - recv_off, (unsigned) PACKET_SIZE) != recv_len) {
				continue;
			}
			
			if (todo_set.count((int) recv_off) == 0) continue;
			todo_set.erase((int) recv_off);
			
			memcpy(stdout_content + recv_off, buf + 18, recv_len);
		} else {
			pdii p = todo_queue.top();
			
			int off = p.second.first;
			int len = p.second.second;
			if (todo_set.count(off) == 0) {
				todo_queue.pop();
				continue;
			}
			
			double t = p.first;
			double cur_t = getUnixTime();
			if (cur_t < t) {
				continue;
			}
			
			todo_queue.pop();
			todo_queue.push(pdii(t + RTT_s, p.second));
			
			memcpy(buf, "stdout-get", 10);
			memcpy(buf + 10, &off, 4);
			memcpy(buf + 14, &len, 4);
			sock_c.write(buf, 18);
			sock_c.flush();
			++n_cur_requested;
		}
	}
	
	while (sock_b.waitForReadyRead(RTT_ms + 1)) {
		sock_b.readDatagram(buf, sizeof(buf));
	}
	
	double dt = max((getUnixTime() - time_start) * 1000.0, 1.0);
	printf("Received %d bytes in %.0lf ms (%.0lf KB/s)\n", sz, dt, sz / (dt * 0.001) / 1024.0);
	
	// save stdout to file
	FILE *f = fopen(stdout_save_path, "wb");
	if (f) {
		fwrite(stdout_content, sz, 1, f);
		fclose(f);
	}
	delete []stdout_content;
}

void send_done() {
	sock_c.write("done", 4);
	sock_c.flush();
	
	// 10ms ?
	while (sock_b.waitForReadyRead(10)) {
		static char buf[2333];
		sock_b.readDatagram(buf, sizeof(buf));
	}
	
	printf("done\n");
}

int main(int argc, char **argv) {
	QCoreApplication a(argc, argv);
	
	if (argc != 1 + 7 && argc != 1 + 8) {
		qout << "usage: run [ip] [local-port] [input-file] [answer-file] [binary-file] [time_ns] [mem_kb] [[stdout-save-path]]\n";
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
	
	if (argc == 1 + 8) {
		get_stdout(argv[8]);
	}
	
	send_done();
	
	return 0;
}

