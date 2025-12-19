CC := clang
CFLAGS := -g -Wall -Werror -Wno-unused-function -Wno-unused-variable

UI_LIBS := -lform -lncurses
SYS_LIBS := -lpthread -lcrypto -lm

all: client_test

clean:
	rm -f grintorrent file_test client_test message_test

# Main application (optional future target)
grintorrent: ./src/grintorrent.c ./src/ui.c
	$(CC) $(CFLAGS) -o grintorrent \
	./src/grintorrent.c ./src/ui.c \
	$(UI_LIBS) $(SYS_LIBS)

file_test: ./tests/file_test.c ./src/file.c ./src/htable.c
	$(CC) $(CFLAGS) -Wno-deprecated-declarations \
	-o file_test \
	./tests/file_test.c ./src/file.c ./src/htable.c \
	$(SYS_LIBS)

client_test: ./src/client.c ./src/message.c ./src/htable.c ./src/file.c ./src/ui.c ./src/ui_adapter.c
	$(CC) $(CFLAGS) -Wno-deprecated-declarations \
	-o grintorrent \
	./src/client.c \
	./src/message.c \
	./src/htable.c \
	./src/file.c \
	./src/ui.c \
	./src/ui_adapter.c \
	$(UI_LIBS) $(SYS_LIBS)

message_test: ./tests/message_test.c ./src/message.c
	$(CC) $(CFLAGS) -o message_test \
	./tests/message_test.c ./src/message.c

zip:
	@echo "Generating grintorrent.zip file to submit to Gradescope..."
	@zip -q -r grintorrent.zip . \
	-x .git/\* .vscode/\* .clang-format .gitignore grintorrent
	@echo "Done. Please upload grintorrent.zip to Gradescope."
