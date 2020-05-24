#define DATA_LENGTH 255

#include "ArduWinoSerialPort.h"
#include <iostream>
#include <vector>
#include <thread>
#include <sstream>
#include <fstream>

using namespace std;

const char* portName = "\\\\.\\COM18";

//Declare a global object
ArduWinoSerialPort* arduinoSerial;

char receivedString[DATA_LENGTH];
int (*funcArray[10])(string args);

int main(void)
{
	string line;
	stringbuf sb;
	stringstream ss;
	ofstream os;
	string cmd;

	ArduWinoSerialPort arduinoSerial("\\\\.\\COM18", 9600);
	size_t br = 0;

	while (true) {
		while (!arduinoSerial) {
			cout << "ERROR!" << endl;
			Sleep(1000);
		}
		arduinoSerial.println("Hello!");
		string line = arduinoSerial.readStringUntilTimeout();
		if (line != "") {
			cout << line << endl;
			
		}
		else {
			cout << "Waiting..." << endl;
		}

		//Sleep(100);
		//cout << "Configuring pins" << endl;
		//cmd = "/md/2/o^/md/3/o^/md/4/o^/md/5/o^/md/6/o^";
		//arduinoSerial.println(cmd);

		//while (true) {
		//	cout << "\nSettings pins to HIGH" << endl;

		//	for (size_t i = 2; i <= 6; i++)
		//	{
		//		cout << i << ": begin" << endl;
		//		cmd = "/dw/" + std::to_string(i);
		//		cmd += "/h";	
		//		cout << i << ": begin write" << endl;
		//		arduinoSerial.println(cmd);
		//		cout << i << ": end write" << endl;				
		//		//Sleep(15);
		//		//cout << arduinoSerial.readStringUntilTimeout();
		//		cout << arduinoSerial.readStringUntilTimeout();
		//		cout << arduinoSerial.readStringUntilTimeout();
		//	}

		//	//cmd = "/dw/2/h^/dw/3/h^/dw/4/h^/dw/5/h^/dw/6/h";
		//	//arduinoSerial.println(cmd);
		//	Sleep(500);

		//	//cout << arduinoSerial.readStringUntilTimeout() << endl;

		//	cout << "Setting pins to LOW" << endl;
		//	cmd = "/dw/2/l^/dw/3/l^/dw/4/l^/dw/5/l^/dw/6/l";
		//	arduinoSerial.println(cmd);
		//	Sleep(500);

		//	//cout << arduinoSerial.readStringUntilTimeout();
		//}

	}


}