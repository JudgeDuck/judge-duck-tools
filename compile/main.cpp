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

int system(std::string s) {
	return system(s.c_str());
}

QTextStream qout(stdout);

QString compile(string source_file, string tasklib_file, string output_path, string language) {
	if (output_path.length() == 0) output_path = ".";
	if (output_path[output_path.length() - 1] != '/') output_path += '/';
	
	string GCC = "ulimit -v 524288 && ulimit -f 2048 && ulimit -t 10 && gcc  -pipe  -O2  -MD -fno-omit-frame-pointer -static -Wall -Wno-format -Wno-unused -gstabs -m32 -fno-tree-ch -fno-stack-protector -gstabs -c -o ";
	
	string GXX = "ulimit -v 524288 && ulimit -f 2048 && ulimit -t 10 && g++  -pipe  -O2  -MD -fno-omit-frame-pointer -static -Wall -Wno-format -Wno-unused -gstabs -m32 -fno-tree-ch -fno-stack-protector -fno-exceptions -fno-unwind-tables -fno-rtti -fno-threadsafe-statics -gstabs -c -o ";
	
	string GXX11 = "ulimit -v 524288 && ulimit -f 2048 && ulimit -t 10 && g++  -pipe  -O2 -std=c++11 -MD -fno-omit-frame-pointer -static -Wall -Wno-format -Wno-unused -gstabs -m32 -fno-tree-ch -fno-stack-protector -fno-exceptions -fno-unwind-tables -fno-rtti -fno-threadsafe-statics -gstabs -c -o ";
	
	string GXX_TASKLIB = "ulimit -v 524288 && ulimit -f 2048 && ulimit -t 10 && g++  -pipe  -O2  -MD -fno-omit-frame-pointer -static -Wall -Wno-format -Wno-unused -gstabs -m32 -fno-tree-ch -fno-stack-protector -fno-exceptions -fno-unwind-tables -fno-rtti -fno-threadsafe-statics  -I../judge-duck-libs/libtaskduck_include -DJOS_USER -gstabs -c -o ";
	
	string G = GCC;
	if (language == "C++") {
		G = GXX;
	} else if (language == "C++11") {
		G = GXX11;
	}
	
	string tasklib_option = "";
	if (language == "C") {
		tasklib_option = "-DTASKLIB_USE_C";
	}
	
	qout << "Compiling...\n";
	qout.flush();
	
	string contestant_filename = source_file;
	
	if (language != "C") {
		contestant_filename = contestant_filename + " -fno-use-cxa-atexit";
	}
	contestant_filename = contestant_filename + " -U_FORTIFY_SOURCE";
	if (system((G + output_path + "contestant.o " + contestant_filename + " > " + output_path + "gcc_contestant.log 2>&1").c_str()))
		return "Contestant compile error\n" + localFileContent(output_path + "gcc_contestant.log").left(10240);
	if (system((GXX_TASKLIB + output_path + "tasklib.o " + tasklib_file + " " + tasklib_option + " > " + output_path + "gcc_tasklib.log 2>&1").c_str()))
		return "Tasklib compile error\n" + localFileContent(output_path + "gcc_tasklib.log").left(10240);
	if (system("ulimit -v 524288 && ulimit -f 2048 && ulimit -t 10 && ld -o " + output_path + "contestant.exe -T ../judge-duck-libs/judgeduck.ld -m elf_i386 -nostdlib " + output_path + "contestant.o " + output_path + "tasklib.o ../judge-duck-libs/libtaskduck/libtaskduck.a ../judge-duck-libs/libjudgeduck/libjudgeduck.a ../judge-duck-libs/libstdduck/libstdduck.a ../judge-duck-libs/libopenlibm.a ../judge-duck-libs/libgcc.a ../judge-duck-libs/libstdc++.a ../judge-duck-libs/libgcc_eh.a ../judge-duck-libs/libstdduck/libstdduck.a > " + output_path + "ld.log 2>&1")) {
		if (system("c++filt < " + output_path + "ld.log > " + output_path + "filt.log")) {
			return "Link error and c++filt error\n" + localFileContent(output_path + "ld.log").left(10240);
		} else {
			return "Link error\n" + localFileContent(output_path + "filt.log").left(10240);
		}
	}
	system(("size " + output_path + "contestant.exe > " + output_path + "size.out").c_str());
	FILE *fin = fopen((output_path + "size.out").c_str(), "r");
	unsigned data = 1 << 30, bss = 1 << 30;
	fscanf(fin, "%*s%*s%*s%*s%*s%*s%*s%d%d", &data, &bss);
	if(data + bss > (1280u << 20))
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

