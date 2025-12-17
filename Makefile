CC := clang
CFLAGS := -g -Wall -Werror -Wno-unused-function -Wno-unused-variable

all: client_test

clean:
	rm -f grintorrent file_test client_test message_test

grintorrent: grintorrent.c ui.c ui.h
	$(CC) $(CFLAGS) -o grintorrent grintorrent.c ui.c -lform -lncurses -lpthread -lcrypto -lm

file_test: ./src/file.h ./tests/file_test.c ./src/file.c ./src/htable.c
	$(CC) $(CFLAGS) -Wno-deprecated-declarations -o file_test ./tests/file_test.c ./src/file.c ./src/htable.c -lform -lncurses -lpthread -lcrypto -lm

client_test: ./src/client.c ./src/file.h ./src/socket.h ./src/htable.c ./src/file.c ./src/message.c ./src/message.h 
	$(CC) $(CFLAGS) -Wno-deprecated-declarations -o client_test ./src/client.c ./src/message.c ./src/htable.c ./src/file.c -lcrypto

message_test: ./src/message.h ./src/message.c ./src/socket.h ./tests/message_test.c
	$(CC) $(CFLAGS) -o message_test ./tests/message_test.c ./src/message.c

zip:
	@echo "Generating grintorrent.zip file to submit to Gradescope..."
	@zip -q -r grintorrent.zip . -x .git/\* .vscode/\* .clang-format .gitignore grintorrent
	@echo "Done. Please upload grintorrent.zip to Gradescope."
