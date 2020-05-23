/*
    Enhanced version of SerialClass https://playground.arduino.cc/Interfacing/CPPWindows/
    Author: Unknown
    License: MIT
    Modified by: Petar Georgiev
*/

#pragma once

#include <Windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <sstream>
#include <atomic>
#include <condition_variable>

#define ARDUINO_WAIT_TIME 2000
#define MAX_DATA_LENGTH 255



class ArduWinoSerialPort
{
private:
    HANDLE hSerial;
    bool connected;
    COMSTAT status{0};
    DWORD errors = 0;
    DWORD lastError = 0;
    std::stringstream rxStream;
    std::thread* rxWorker;
    std::mutex mutex;
    std::mutex workerMutex;
    std::condition_variable cv;
    std::string rxBuffer;
    std::string portName;
    DWORD portSpeed = 0;
    DCB dcbSerialParams = { 0 };
    void rxWorkerFunc(int num);
    void rxReaderFunc();
    DWORD setError();
public:
    ArduWinoSerialPort(const char* portName, int portSpeed = CBR_9600);
    ~ArduWinoSerialPort();
    DWORD open();
    void close();
    int ReadData(char* buffer, unsigned int nbChar);
    bool writeBytes(const char* buffer, unsigned int buf_size);
    bool IsConnected();    
    bool operator !();    
    operator bool();    
};

//bool operator !(const ArduWinoSerialPort&);

//extern ArduWinoSerialPort ASerial;