CXX            	= /usr/bin/g++
CPPFLAGS      	= -g -c
LDFLAGS       	= -g
OMNI_HOME     	= /home/daniel/downloads/omniORB
OMNIIDL       	= $(OMNI_HOME)/build/bin/omniidl
LIBS          	= -lomniORB4 -lomnithread -lomniDynamic4
OBJECTS       	= echoSK.o server.o
IDL_DIR		= ../idl
IDL_FILE	= $(IDL_DIR)/echo.idl

all server: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o server server.o echoSK.o $(LIBS)

server.o: server.cc
	$(CXX) $(CPPFLAGS) server.cc -I.

echoSK.o: echoSK.cc echo.hh
	$(CXX) $(CPPFLAGS) echoSK.cc -I.

echoSK.cc: $(IDL_FILE)
	$(OMNIIDL) -bcxx $(IDL_FILE)

run: server
	# Start Naming service with command 'omniNames -start -always' as root
	./server -ORBInitRef NameService=corbaname::localhost

clean clean-up:
	rm -rf *.o
	rm -rf *.hh
	rm -rf *SK.cc
	rm -rf server
