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
    
    std::thread* rxWorker;
    std::mutex bufferMutex{};
    std::mutex bytesAvailableMutex{};
    std::mutex workerMutex{};
    std::condition_variable cv{};
    std::condition_variable cvRxBuffer{};
    std::string rxBuffer;
    std::string portName;
    DWORD portSpeed = 0;
    DCB dcbSerialParams = { 0 };
    DWORD timeout = 300;
    void rxWorkerFunc(int num);
    void rxReaderFunc();
    DWORD setError();
    bool isDelimiter(char ch, std::string delimiters = "\n");
public:
    ArduWinoSerialPort(const char* portName, int portSpeed = CBR_9600);
    ~ArduWinoSerialPort();
    DWORD open();
    void close();
    bool writeBytes(const char* buffer, unsigned int buf_size);
    bool print(std::string &str);
    bool println(std::string str);
    bool isConnected();       
    operator bool();    
    int available();
    int timedAvailable();
    int read();
    int timedRead();
    long long millis();
    std::string readStringUntilTimeout(std::string delimiters = "\n");
    std::string readStringUntil(std::string delimiters = "\n");
};