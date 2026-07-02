CC = cc
CFLAGS = -Wall -Wextra -Wpedantic -Iinclude

AGENT_SRC = agent/main.c agent/check.c agent/report.c
nixel: $(AGENT_SRC)
	$(CC) $(CFLAGS) -o nixel $(AGENT_SRC)

SERVER_SRC = server/main.c server/parse.c server/store.c third_party/cJSON/cJSON.c
nixel-server: $(SERVER_SRC)
	$(CC) $(CFLAGS) -Ithird_party/cJSON -o nixel-server $(SERVER_SRC) -lsqlite3


all: nixel nixel-server
test: all
	bash tests/smoke.sh
clean:
	rm -f nixel nixel-server
