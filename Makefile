all: Asst2.c
	gcc -g3 -pthread -o detector Asst2.c -lm

clean:
	rm detector
