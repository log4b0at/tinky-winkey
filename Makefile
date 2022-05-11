SVC_NAME=svc.exe
TINKY_NAME=tinky.exe
WINKEY_NAME=winkey.exe
RM=rm -f
CPP=cl
CPPFLAGS= /Iinc /EHsc /Wall /WX /Wv:12

all: $(SVC_NAME) $(TINKY_NAME) $(WINKEY_NAME)

$(SVC_NAME): src\svc.cpp
	$(CPP) src\svc.cpp $(CPPFLAGS)

$(TINKY_NAME): src\tinky.cpp
	$(CPP) src\tinky.cpp $(CPPFLAGS)

$(WINKEY_NAME): src\winkey.cpp inc\Logger.hpp inc\KeyboardInputLog.hpp
	$(CPP) src\winkey.cpp $(CPPFLAGS) /std:c++17

re: fclean all

fclean:
	$(RM) $(WINKEY_NAME) $(TINKY_NAME) $(SVC_NAME)