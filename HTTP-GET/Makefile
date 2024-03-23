# If you use threads, add -pthread here.
COMPILERFLAGS = -g -Wall -Wextra -Wno-sign-compare -pthread

# Any libraries you might need linked in.
# LINKLIBS = -lm

# The components of each program.
SERVEROBJECTS = obj/http_server.o
CLIENTOBJECTS = obj/http_client.o

# Every rule listed here as .PHONY is "phony".
.PHONY: all clean

# Default rule
all : obj http_server http_client

# Rule to build http_server
http_server: $(SERVEROBJECTS)
	$(CXX) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

# Rule to build http_client
http_client: $(CLIENTOBJECTS)
	$(CXX) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

# Rule to clean up
clean :
	$(RM) obj/*.o http_server http_client

# Rule to compile C++ source files
obj/%.o: src/%.cpp
	$(CXX) $(COMPILERFLAGS) -c -o $@ $<

# Rule to create the obj directory if it doesn't exist
obj:
	mkdir -p obj
