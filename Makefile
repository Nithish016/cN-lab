# ==============================================================================
# MULTI-CLIENT CHAT APPLICATION - MAKEFILE (WINDOWS / MINGW GCC)
# Computer Networks Lab Project
# ==============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -O2 -Ishared
LDFLAGS = -lws2_32

SERVER_TARGET = server.exe
CLIENT_TARGET = client.exe

SERVER_SRC = server/server.c
CLIENT_SRC = client/client.c
SHARED_HDR = shared/common.h server/server.h client/client.h

.PHONY: all clean directories run-server run-client

all: directories $(SERVER_TARGET) $(CLIENT_TARGET)
	@echo Build successful! Run ./server.exe and ./client.exe

directories:
	@if not exist "received_files" mkdir "received_files"
	@if not exist "files\received_files" mkdir "files\received_files"

$(SERVER_TARGET): $(SERVER_SRC) $(SHARED_HDR)
	$(CC) $(CFLAGS) $(SERVER_SRC) -o $(SERVER_TARGET) $(LDFLAGS)
	@copy /Y $(SERVER_TARGET) server\$(SERVER_TARGET) >nul 2>&1

$(CLIENT_TARGET): $(CLIENT_SRC) $(SHARED_HDR)
	$(CC) $(CFLAGS) $(CLIENT_SRC) -o $(CLIENT_TARGET) $(LDFLAGS)
	@copy /Y $(CLIENT_TARGET) client\$(CLIENT_TARGET) >nul 2>&1

run-server: $(SERVER_TARGET)
	./$(SERVER_TARGET) 8080

run-client: $(CLIENT_TARGET)
	./$(CLIENT_TARGET)

clean:
	@if exist $(SERVER_TARGET) del /Q $(SERVER_TARGET)
	@if exist $(CLIENT_TARGET) del /Q $(CLIENT_TARGET)
	@if exist server\$(SERVER_TARGET) del /Q server\$(SERVER_TARGET)
	@if exist client\$(CLIENT_TARGET) del /Q client\$(CLIENT_TARGET)
	@if exist *.obj del /Q *.obj
	@echo Cleanup complete.
