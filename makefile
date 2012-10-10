CC=g++

chess.so: chess.cpp
	echo "Compiling chess.cpp..."
	$(CC) -o $@ -Wall -shared -g -O0 -D_GNU_SOURCE -fPIC -ldl $^

eg:
	echo "Compiling sample..."
	gcc -o $(source) -lpthread -lrt $(source).c

test:
	echo "Computing results..."
	count=1 ; while [[ $$count -le 1000 ]] ; do \
		./run.sh $(source) >> $(result) ; \
		((count = count + 1)); \
	done

testsuite1: chess.so
	rm -f result1
	rm -f result2
	rm -f result3
	rm -f result4

	make eg source=sample1
	make test source=sample1 result=result1
	make test source=sample1 result=result2
	make test source=sample1 result=result3
	make test source=sample1 result=result4

	diff -s result1 result2
	diff -s result1 result3
	diff -s result1 result4
	diff -s result2 result3
	diff -s result2 result4
	diff -s result3 result4

clean:
	rm -f chess.so
	rm -f result1
	rm -f result2
	rm -f result3
	rm -f result4
	rm -f sample1
	rm -f sample2