SVC_NAME=svc.exe
TINKY_NAME=tinky.exe
WINKEY_NAME=winkey.exe
RM=rm -f
CPP=cl
CPPFLAGS= /Iinc /EHsc /Wall /WX /Wv:12 /std:c++17


SVC_HPPS=./inc/common.hpp
SVC_CPPS=./src/svc.cpp
SVC_OBJS=./obj/svc.obj

TINKY_HPPS=./inc/common.hpp
TINKY_CPPS=./src/tinky.cpp
TINKY_OBJS=./obj/tinky.obj

WINKEY_HPPS=./inc/common.hpp ./inc/winkey.hpp ./inc/Logger.hpp ./inc/KeyboardInputLog.hpp
WINKEY_CPPS=./src/winkey.cpp
WINKEY_OBJS=./obj/winkey.obj

all: $(SVC_NAME) $(TINKY_NAME) $(WINKEY_NAME)

$(SVC_NAME): $(SVC_OBJS)  $(SVC_HPPS)
	$(CPP) $(CPPFLAGS) $(SVC_OBJS) /Fe$(SVC_NAME)

$(TINKY_NAME): $(TINKY_OBJS) $(TINKY_HPPS)
	$(CPP) $(CPPFLAGS) $(TINKY_OBJS) /Fe$(TINKY_NAME)

$(WINKEY_NAME): $(WINKEY_OBJS) $(WINKEY_HPPS)
	$(CPP) $(CPPFLAGS) $(WINKEY_OBJS) /Fe$(WINKEY_NAME)

{./src/}.cpp{./obj}.obj:
	$(CPP) $< $(CPPFLAGS) /c /Fo$(*R).obj

re: fclean all

clean:
	$(RM) $(WINKEY_OBJS) $(TINKY_OBJS) $(SVC_OBJS)

fclean: clean
	$(RM) $(WINKEY_NAME) $(TINKY_NAME) $(SVC_NAME)