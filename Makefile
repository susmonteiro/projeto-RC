CC=gcc
LD=gcc
CFLAGS=-g -Wall -Wextra
LDFLAGS=

all: AS FS pd user

AS: AS.c config.h error.c error.h connection.c connection.h
	$(CC) $(CFLAGS) AS.c error.c connection.c -o AS

FS: FS.c config.h error.c error.h connection.c connection.h
	$(CC) $(CFLAGS) FS.c error.c connection.c -o FS	

pd: pd.c config.h error.c error.h connection.c connection.h
	$(CC) $(CFLAGS) pd.c error.c connection.c -o pd

user: user.c config.h error.c error.h connection.c connection.h
	$(CC) $(CFLAGS) user.c error.c connection.c -o user

clean:
	@echo Cleaning...
	rm -f AS FS pd user