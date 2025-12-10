CC := clang
CFLAGS := -g -Wall -Werror -Wno-unused-function -Wno-unused-variable

all: grintorrent

clean:
	rm -f grintorrent

grintorrent: grintorrent.c ui.c ui.h
	$(CC) $(CFLAGS) -o grintorrent grintorrent.c ui.c -lform -lncurses -lpthread

zip:
	@echo "Generating grintorrent.zip file to submit to Gradescope..."
	@zip -q -r grintorrent.zip . -x .git/\* .vscode/\* .clang-format .gitignore grintorrent
	@echo "Done. Please upload grintorrent.zip to Gradescope."
