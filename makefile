chess.so: chess.cpp
	g++ -o $@ -Wall -shared -g -O0 -D_GNU_SOURCE -fPIC -ldl $^

eg1:
	gcc -o sample1 -lpthread -lrt sample1.c

eg2:
	gcc -o sample2 -lpthread -lrt sample2.c

eg3:
	gcc -o sample2 -lpthread -lrt sample2.c

eg4:
	gcc -o sample2 -lpthread -lrt sample2.c