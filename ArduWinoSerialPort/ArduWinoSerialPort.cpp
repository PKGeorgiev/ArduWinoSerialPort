#include "ArduWinoSerialPort.h"

using namespace std;

ArduWinoSerialPort::ArduWinoSerialPort(const char* portName, int portSpeed)
{
    this->connected = false;
    this->portName = portName;
    this->portSpeed = portSpeed;
    this->rxWorker = new std::thread(&ArduWinoSerialPort::rxWorkerFunc, this, 0);
}

ArduWinoSerialPort::~ArduWinoSerialPort()
{
    //Check if we are connected before trying to disconnect
    if (this->connected)
    {
        //We're no longer connected
        this->connected = false;
        //Close the serial handler
        CloseHandle(this->hSerial);
        cv.notify_all();
        rxWorker->join();
        delete rxWorker; 
    }
}

DWORD ArduWinoSerialPort::open() 
{
    if (this->connected) 
    {
        return 0;
    }

    rxBuffer.clear();

    //Try to connect to the given port throuh CreateFile
    this->hSerial = CreateFileA(this->portName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    //Check if the connection was successfull
    if (this->hSerial == INVALID_HANDLE_VALUE)
    {
        return setError();
    }
    else
    {
        //Try to get the current
        if (!GetCommState(this->hSerial, &dcbSerialParams))
        {
            return setError();
        }
        else
        {            
            dcbSerialParams.BaudRate = portSpeed;
            dcbSerialParams.ByteSize = 8;
            dcbSerialParams.StopBits = ONESTOPBIT;
            dcbSerialParams.Parity = NOPARITY;
            dcbSerialParams.fAbortOnError = true;

            //Setting the DTR to Control_Enable ensures that the Arduino is properly
            //reset upon establishing a connection
            dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;            

            //Set the parameters and check for their proper application
            if (!SetCommState(hSerial, &dcbSerialParams))
            {
                return setError();
            }
            else
            {                
                //Purge any remaining characters in the buffers 
                if (!PurgeComm(this->hSerial, PURGE_RXCLEAR | PURGE_TXCLEAR)) {
                    return GetLastError();
                }
                //We wait 2s as the arduino board will be reseting
                //Sleep(ARDUINO_WAIT_TIME);
            }
        }
    }

    COMMTIMEOUTS oldCto, cto;
    if (GetCommTimeouts(this->hSerial, &cto)) {
        oldCto = cto;
        cto.ReadIntervalTimeout = MAXDWORD;
        cto.ReadTotalTimeoutConstant = 0;
        cto.ReadTotalTimeoutMultiplier = 0;
        if (!SetCommTimeouts(this->hSerial, &cto)) {
            return setError();
        }
    }
    else {
        return setError();
    }

    if (!SetCommMask(this->hSerial, EV_RXCHAR)) {
        return setError();
    }

    this->connected = true;
    cv.notify_all();

    return 0;
}

bool ArduWinoSerialPort::IsConnected()
{
    return this->connected;
}

void ArduWinoSerialPort::rxReaderFunc() 
{
    char buf[MAX_DATA_LENGTH]{ 0 };
    DWORD dwEventMask = 0;

    if (WaitCommEvent(this->hSerial, &dwEventMask, NULL))
    {
        DWORD dwIncommingReadSize{ 0 };

        do
        {
            if (ReadFile(this->hSerial, buf, MAX_DATA_LENGTH - 1, &dwIncommingReadSize, NULL) != 0)
            {
                if (dwIncommingReadSize > 0)
                {
                    std::lock_guard<std::mutex> lock1(this->mutex);  
                    rxStream << buf;
                    rxBuffer.append(buf, dwIncommingReadSize);
                    cout << "\nRX: " << rxBuffer << "\n";
                }
            } else
            {
                setError();
            }
        } while (dwIncommingReadSize > 0);
    }
    else {
        setError();
    }
}

void ArduWinoSerialPort::rxWorkerFunc(int num) {
    while (true) {
        if (connected)
        {
            rxReaderFunc();
        } else {
            cout << "Sleeping" << endl;
            std::unique_lock<std::mutex> lck(workerMutex);
            cv.wait(lck);
            cout << "Resumed" << endl;
        };
    }
}

bool ArduWinoSerialPort::writeBytes(const char* buffer, unsigned int buf_size)
{
    DWORD bytesSend;

    if (!WriteFile(this->hSerial, (void*)buffer, buf_size, &bytesSend, 0))
    {
        ClearCommError(this->hSerial, &this->errors, &this->status);
        return false;
    }

    /*if (!FlushFileBuffers(this->hSerial)) {
        std::cout << "Error4!";
    }*/

    return true;
}

DWORD ArduWinoSerialPort::setError() {
    close();
    connected = false;
    lastError = GetLastError();  
    return lastError;
}

void ArduWinoSerialPort::close() {
    if (this->connected)
    {
        this->connected = false;
        CloseHandle(this->hSerial);
    }
}