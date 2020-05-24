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

	ArduWinoSerialPort arduinoSerial(portName, 9600);

	while (true) {
		while (!arduinoSerial) {
			cout << "ERROR!" << endl;
			Sleep(1000);
		}

		string str = arduinoSerial.readStringUntilTimeout();
		if (str != "") {
		    cout << "Here: " << str << endl;

			//arduinoSerial.println("sd");
		    
		}
		else {
		    //cout << arduinoSerial.timedAvailable() << endl;
		    
		}



	}


}