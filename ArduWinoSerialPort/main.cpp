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
ArduWinoSerialPort* arduino;

char receivedString[DATA_LENGTH];
int (*funcArray[10])(string args);

int main(void)
{
    string line;
    stringbuf sb;
    stringstream ss;
    ofstream os;
    string cmd;


    arduino = new ArduWinoSerialPort(portName);

    while (true) {
        while (arduino->open() != 0) {
            cout << "ERROR!" << endl;
            Sleep(1000);
        }
        int bytesReceived = 0;
        int a;
        //cin >> a;
        //do {
        //    bytesReceived = arduino->readSerialPort(receivedString, DATA_LENGTH, 1000);
        //    if (bytesReceived)
        //        cout << receivedString;
        //    cout << "Sleeping..." << endl;
        //    //Sleep(5000);
        //} while (true);
        Sleep(2000);
        
    }


}