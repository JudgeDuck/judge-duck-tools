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

#define printf(x...) fprintf(stderr, x)

size_t fileSize(const char *fn) {
	struct stat st;
	if (stat(fn, &st))
		return 0;
	return st.st_size;
}

#include <unistd.h>

QString localFileContent(QString fn) {
	QFile file(fn);
	file.open(QIODevice::ReadOnly);
	QTextStream ts(&file);
	return ts.readAll();
}

QString localFileContent(const char *fn) {
	return localFileContent(QString(fn));
}

QString localFileContent(std::string fn) {
	return localFileContent(fn.c_str());
}

string cmd_prefix = "ulimit -v 524288 && ulimit -f 20480 && ulimit -t 10 && ";

int system(std::string s) {
	s = cmd_prefix + s;
	return system(s.c_str());
}

int system(std::string s, QString &out, std::string log_path = "") {
	if (log_path.length() != 0 && log_path[log_path.length() - 1] != '/') {
		log_path += '/';
	}
	string log_file = log_path + "____log.txt";
	int ret = system(s + " > " + log_file + " 2>&1");
	out = localFileContent(log_file).left(10240);
	return ret;
}

QTextStream qout(stdout);

string duck_cc = "gcc -static -pipe -m32 -U_FORTIFY_SOURCE -O2 -c -fno-stack-protector ";
string duck_cxx = "g++ -static -pipe -m32 -U_FORTIFY_SOURCE -O2 -c -fno-stack-protector -fno-exceptions ";

string cxx11_flags = " -std=c++11 ";

// TODO: Remove tasklib !!
string tasklib_cxxflags = " -I../judge-duck-libs/libtaskduck_include -DJOS_USER ";
string tasklib_cxxflags_c = " -DTASKLIB_USE_C ";

string duck_ld = "ld -static -m elf_i386 -nostdlib -T ../judge-duck-libs/judgeduck.ld ";

string GCC_version = "gcc-8";

string duck_ldflags_suf_c = " ../judge-duck-libs/libtaskduck/libtaskduck.a ../judge-duck-libs/libjudgeduck/libjudgeduck.a -L../libc-duck/lib -L../judge-duck-libs/" + GCC_version + " -lm -lc -lgcc -lgcc_eh -lc ";
string duck_ldflags_suf_cxx = " ../judge-duck-libs/libtaskduck/libtaskduck.a ../judge-duck-libs/libjudgeduck/libjudgeduck.a -L../libc-duck/lib -L../judge-duck-libs/" + GCC_version + " -lstdc++ -lm -lc -lgcc -lgcc_eh -lc ";

QString compile(string source_file, string tasklib_file, string output_path, string language) {
	if (output_path.length() == 0) output_path = ".";
	if (output_path[output_path.length() - 1] != '/') output_path += '/';
	
	string compiler = duck_cc;
	if (language == "C++") {
		compiler = duck_cxx;
	} else if (language == "C++11") {
		compiler = duck_cxx + cxx11_flags;
	}
	
	string tasklib_flags = tasklib_cxxflags;
	if (language == "C") {
		tasklib_flags += tasklib_cxxflags_c;
	}
	
	qout << "Compiling...\n";
	qout.flush();
	
	string contestant_src = source_file;
	string contestant_obj = output_path + "contestant.o";
	string tasklib_src = tasklib_file;
	string tasklib_obj = output_path + "tasklib.o";
	string contestant_exe = output_path + "contestant.exe";

	QString log;
	
	if (system(compiler + contestant_src + " -o " + contestant_obj, log, output_path)) {
		return "Contestant compile error\n" + log;
	}
	if (system(duck_cxx + tasklib_flags + tasklib_src + " -o " + tasklib_obj, log, output_path)) {
		return "Tasklib compile error\n" + log;
	}
	
	string ld_logfile = output_path + "ld.log";
	string ldflags_suf = duck_ldflags_suf_cxx;
	if (system(duck_ld + " -o " + contestant_exe + " " + contestant_obj + " " + tasklib_obj + ldflags_suf + " > " + ld_logfile + " 2>&1")) {
		if (system("c++filt < " + ld_logfile, log, output_path)) {
			return "Link error and c++filt error\n" + log;
		} else {
			return "Link error\n" + localFileContent(ld_logfile).left(10240);
		}
	}

	string size_outfile = output_path + "size.out";
	system("size " + contestant_exe + " > " + size_outfile);
	FILE *fin = fopen((output_path + "size.out").c_str(), "r");
	unsigned data = 1 << 30, bss = 1 << 30;
	fscanf(fin, "%*s%*s%*s%*s%*s%*s%*s%d%d", &data, &bss);
	if(data + bss > (2176u << 20))
		return "Too large global variables";
	fclose(fin);
	
	qout << "Compile success!\n";
	qout.flush();
	
	return "";
}

int main(int argc, char **argv) {
	QCoreApplication a(argc, argv);
	
	if(argc != 1 + 4) {
		qout << "usage: compile.exe [source-file] [tasklib-file] [output-path] [lang]\n";
		qout.flush();
		return 1;
	}
	
	string source_file = argv[1];
	string tasklib_file = argv[2];
	string output_path = argv[3];
	string language = argv[4];
	
	qout << compile(source_file, tasklib_file, output_path, language);
	qout.flush();
	
	return 0;
}

