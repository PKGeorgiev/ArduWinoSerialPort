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
		0,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		0);

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
				Sleep(ARDUINO_WAIT_TIME);
			}
		}
	}

	COMMTIMEOUTS oldCto, cto;
	if (GetCommTimeouts(this->hSerial, &cto)) {
		oldCto = cto;
		cto.ReadIntervalTimeout = 0;
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
	DWORD      dwRes = 0;
	DWORD      dwCommEvent = 0;
	DWORD      dwStoredFlags = 0;
	BOOL      fWaitingOnStat = FALSE;
	OVERLAPPED osStatus = { 0 };
	BOOL bWaitingOnStatusHandle = false;
	DWORD dwOvRes = 0;

	dwStoredFlags = EV_RXCHAR;
	if (!SetCommMask(hSerial, dwStoredFlags))
		// error setting communications mask; abort
		setError();

	osStatus.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (osStatus.hEvent == NULL)
	{
		// error creating event; abort
		setError();
		return;
	}

	for (; ; ) {
		// Issue a status event check if one hasn't been issued already.
		if (!fWaitingOnStat) {
			if (!WaitCommEvent(hSerial, &dwCommEvent, &osStatus)) {
				if (GetLastError() == ERROR_IO_PENDING)
					bWaitingOnStatusHandle = TRUE;
				else
				{
					// error in WaitCommEvent; abort
					setError();
					break;
				}
			}
			else 
			{
				// WaitCommEvent returned immediately.
				// Deal with status event as appropriate.
				// ReportStatusEvent(dwCommEvent);
			}
		}

		// Check on overlapped operation.
		if (bWaitingOnStatusHandle) {
			// Wait a little while for an event to occur.
			dwRes = WaitForSingleObject(osStatus.hEvent, INFINITE);
			switch (dwRes)
			{
				// Event occurred.
				case WAIT_OBJECT_0:
					if (!GetOverlappedResult(hSerial, &osStatus, &dwOvRes, FALSE))
					{
						// An error occurred in the overlapped operation;
						// call GetLastError to find out what it was
						// and abort if it is fatal.
						setError();
						break;
					}
					else
					{
						// Status event is stored in the event flag
						// specified in the original WaitCommEvent call.
						// Deal with the status event as appropriate.
						//ReportStatusEvent(dwCommEvent);
						ClearCommError(this->hSerial, &this->errors, &this->status);

						if (this->status.cbInQue > 0) {
							readBuffer(this->status.cbInQue);
						}
					}

					// Set fWaitingOnStat flag to indicate that a new
					// WaitCommEvent is to be issued.
					fWaitingOnStat = FALSE;
					break;

				case WAIT_TIMEOUT:
					// Operation isn't complete yet. fWaitingOnStatusHandle flag 
					// isn't changed since I'll loop back around and I don't want
					// to issue another WaitCommEvent until the first one finishes.
					//
					// This is a good time to do some background work.
					//DoBackgroundWork();
					break;

				default:
					// Error in the WaitForSingleObject; abort
					// This indicates a problem with the OVERLAPPED structure's
					// event handle.
					CloseHandle(osStatus.hEvent);
					return;
			}
		}
	}

	CloseHandle(osStatus.hEvent);
}

void ArduWinoSerialPort::rxWorkerFunc(int num)
{
	while (true) {
		if (connected)
		{
			rxReaderFunc();
		}
		else {
			cout << "Sleeping" << endl;
			std::unique_lock<std::mutex> lck(workerMutex);
			cv.wait(lck);
			cout << "Resumed" << endl;
		};
	}
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
		string res;
		int charsToDelete = 0;

		for (size_t i = 0; i < rxBuffer.length(); i++)
		{
			charsToDelete = 1;

			if (this->isDelimiter(rxBuffer[i], delimiters))
			{
				if (delimiters.find('\r') != string::npos) {
					if (i == rxBuffer.length() - 1) {
						break;
					}
					else {
						if (rxBuffer[i + 1] == '\n') {
							charsToDelete = 2;
						}
					}
				}

				string res = rxBuffer.substr(0, i);
				rxBuffer.erase(0, i + charsToDelete);
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

bool ArduWinoSerialPort::print(string& str)
{
	return writeBuffer(str.c_str(), str.length());
}

bool ArduWinoSerialPort::println(string str)
{
	str += '\n';
	return writeBuffer(str.c_str(), str.length());
}

BOOL ArduWinoSerialPort::writeBuffer(const char* lpBuf, DWORD dwToWrite)
{
	OVERLAPPED osWrite = { 0 };
	DWORD dwWritten;
	DWORD dwRes = 0;
	BOOL  fRes;

	// Create this write operation's OVERLAPPED structure hEvent.
	osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (osWrite.hEvent == NULL)
		// Error creating overlapped event handle.
		return FALSE;

	// Issue write
	if (!WriteFile(hSerial, lpBuf, dwToWrite, &dwWritten, &osWrite)) {
		if (GetLastError() != ERROR_IO_PENDING) {
			// WriteFile failed, but it isn't delayed. Report error.
			fRes = FALSE;
		}
		else
		{
			// Write is pending.
			dwRes = WaitForSingleObject(osWrite.hEvent, INFINITE);
		}

		switch (dwRes)
		{
			// Overlapped event has been signaled. 
			case WAIT_OBJECT_0:
				if (!GetOverlappedResult(hSerial, &osWrite, &dwWritten, FALSE))
					fRes = FALSE;
				else {
					if (dwWritten != dwToWrite) {
						// The write operation timed out. I now need to 
						// decide if I want to abort or retry. If I retry, 
						// I need to send only the bytes that weren't sent. 
						// If I want to abort, I would just set fRes to 
						// FALSE and return.
						fRes = FALSE;
					}
					else
						// Write operation completed successfully.
						fRes = TRUE;
				}
				break;

			default:
				// An error has occurred in WaitForSingleObject. This usually 
			   // indicates a problem with the overlapped event handle.
				fRes = FALSE;
				break;
		}
	}
	else
	{
		// WriteFile completed immediately.

		if (dwWritten != dwToWrite) {
			// The write operation timed out. I now need to 
			// decide if I want to abort or retry. If I retry, 
			// I need to send only the bytes that weren't sent. 
			// If I want to abort, then I would just set fRes to 
			// FALSE and return.
			fRes = FALSE;
		}
		else
			fRes = TRUE;
	}

	CloseHandle(osWrite.hEvent);
	return fRes;
}

size_t ArduWinoSerialPort::readBuffer(DWORD count)
{
	DWORD dwRead = 0;
	BOOL fWaitingOnRead = false;
	OVERLAPPED osReader = { 0 };
	char* lpBuf = new char[count];

	// Create the overlapped event. Must be closed before exiting
	// to avoid a handle leak.
	osReader.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (osReader.hEvent == NULL)
	{
		// Error creating overlapped event; abort.
		setError();
	}

	if (!fWaitingOnRead) {
		// Issue read operation.
		if (!ReadFile(hSerial, lpBuf, count, &dwRead, &osReader)) {
			if (GetLastError() != ERROR_IO_PENDING)
			{
				// read not delayed?
				setError();
				return 0;
			}
			else
			{
				fWaitingOnRead = TRUE;
			}
		}
		else {
			// read completed immediately
			HandleASuccessfulRead(lpBuf, dwRead);
		}
	}

	DWORD dwRes;

	if (fWaitingOnRead && osReader.hEvent != 0) {
		dwRes = WaitForSingleObject(osReader.hEvent, timeout);
		switch (dwRes)
		{
			// Read completed.
			case WAIT_OBJECT_0:
				if (!GetOverlappedResult(hSerial, &osReader, &dwRead, FALSE))
				{
					// Error in communications; report it.
					setError();
				}
				else 
				{
					// Read completed successfully.
					HandleASuccessfulRead(lpBuf, dwRead);
				}
				//  Reset flag so that another opertion can be issued.
				fWaitingOnRead = FALSE;
				break;

			case WAIT_TIMEOUT:
				// Operation isn't complete yet. fWaitingOnRead flag isn't
				// changed since I'll loop back around, and I don't want
				// to issue another read until the first one finishes.
				//
				// This is a good time to do some background work.
				break;

			default:
				// Error in the WaitForSingleObject; abort.
				// This indicates a problem with the OVERLAPPED structure's
				// event handle.
				break;
		}
	}

	return dwRead;
}

void ArduWinoSerialPort::HandleASuccessfulRead(char* buf, size_t bufSize)
{
	std::unique_lock<std::mutex> lck(this->bufferMutex);
	rxBuffer.append(buf, bufSize);
//	cout << "RX: " << rxBuffer << endl;
}