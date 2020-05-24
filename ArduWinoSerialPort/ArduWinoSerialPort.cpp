#include "ArduWinoSerialPort.h"
#include <string>

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

bool ArduWinoSerialPort::isConnected()
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
                    std::unique_lock<std::mutex> lck(this->bufferMutex);
                    rxBuffer.append(buf, dwIncommingReadSize);                    
                    //cout << "\nRX: " << rxBuffer << "\n";
                }
            } else
            {
                setError();
            }
        } while (dwIncommingReadSize > 0);
        
        cvRxBuffer.notify_all();
    }
    else {
        setError();
    }
}

void ArduWinoSerialPort::rxWorkerFunc(int num) 
{
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

DWORD ArduWinoSerialPort::setError() 
{
    close();
    connected = false;
    lastError = GetLastError();  
    return lastError;
}

void ArduWinoSerialPort::close() 
{
    if (this->connected)
    {
        this->connected = false;
        CloseHandle(this->hSerial);
    }
}

//bool ArduWinoSerialPort::operator!() {
//    return open() != 0;
//}

ArduWinoSerialPort::operator bool() 
{
    return open() == 0;
}

int ArduWinoSerialPort::available() 
{
    std::lock_guard<std::mutex> lck(this->bufferMutex);
    return rxBuffer.length();
}

int ArduWinoSerialPort::read()
{
    std::lock_guard<std::mutex> lck(this->bufferMutex);
    
    if (rxBuffer.length() == 0)
        return -1;
    else
    {
        int ch = rxBuffer[0];
        rxBuffer.erase(0, 1);
        return ch;
    }
}

int ArduWinoSerialPort::timedRead()
{
    if (timedAvailable() > 0)
        return read();

    return -1;
}

long long ArduWinoSerialPort::millis() 
{
    auto start = std::chrono::system_clock::now();
    auto start_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(start);
    auto value = start_ms.time_since_epoch();
    return value.count();
}

int ArduWinoSerialPort::timedAvailable() 
{
    int timeout = this->timeout;
    int avb = 0;

    std::unique_lock<std::mutex> lck(bytesAvailableMutex);

    do 
    {
        avb = available();

        if (avb > 0)
            return avb;
        else
        {
            cv.wait_for(lck, std::chrono::milliseconds(100));
        }

        timeout -= 100;
    } while (timeout > 0);

    return available();
}

bool ArduWinoSerialPort::isDelimiter(char ch, string delimiters) 
{
    return delimiters.find(ch) != string::npos;
}

string ArduWinoSerialPort::readStringUntilTimeout(string delimiters) 
{
    if (timedAvailable() > 0)
    {
        std::lock_guard<std::mutex> lck(this->bufferMutex);

        for (size_t i = 0; i < rxBuffer.length(); i++)
        {
            if (this->isDelimiter(rxBuffer[i], delimiters))
            {
                string res = rxBuffer.substr(0, i);
                rxBuffer.erase(0, i + 1);
                return res;
            }
        }
    }

    return string("");
}

string ArduWinoSerialPort::readStringUntil(string delimiters) 
{
    string ret;
    int c = timedRead();
    while (c >= 0 && !isDelimiter(c, delimiters))
    {
        ret += (char)c;
        c = timedRead();
    }
    return ret;
}

bool ArduWinoSerialPort::print(string &str) 
{
    return writeBytes(str.c_str(), str.length());
}

bool ArduWinoSerialPort::println(string str) 
{
    str += '\n';
    return writeBytes(str.c_str(), str.length());
}