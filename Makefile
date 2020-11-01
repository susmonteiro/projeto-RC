CC=gcc
LD=gcc
CFLAGS=-Wall
LDFLAGS=

.PHONY: clean

AS: AS.c config.h error.c error.h connection.c connection.h
	$(CC) $(CFLAGS) AS.c error.c connection.c -o AS

pd: pd.c config.h error.c error.h connection.c connection.h
	$(CC) $(CFLAGS) pd.c error.c connection.c -o pd

user: user.c config.h error.c error.h connection.c connection.h
	$(CC) $(CFLAGS) user.c error.c connection.c -o user

clean:
	@echo Cleaning...
	rm -f user AS pd