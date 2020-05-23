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

#define ARDUINO_WAIT_TIME 2000

class ArduWinoSerialPort
{
private:
    HANDLE hSerial;
    bool connected;
    COMSTAT status{0};
    DWORD errors = 0;
public:
    ArduWinoSerialPort(const char* portName);
    //Close the connection
    ~ArduWinoSerialPort();
    int ReadData(char* buffer, unsigned int nbChar);
    bool WriteData(const char* buffer, unsigned int nbChar);
    bool IsConnected();
};