#include <Winsock2.h>
#include <windows.h>

int   beginMain(int argC, char** argV)
{
    int   nRet = 0;
    do {
		WORD wVersionRequested;
		WSADATA wsaData;
		int err;

		/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
		wVersionRequested = MAKEWORD(2, 2);

		err = WSAStartup(wVersionRequested, &wsaData);
		if (err != 0) {
			nRet = 1;
		}
    } while (0);
    return nRet;
}

void endMain()
{
    do {
    } while (0);
}