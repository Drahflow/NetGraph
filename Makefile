all: netgraph

%: %.cpp
	g++ $< -o $@ -lSDL -lm -Wall -W -ggdb
