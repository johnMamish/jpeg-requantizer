all: jpeg.c jpeg-requantizer.c main.c
	gcc -g -O0 -Wall -std=gnu99 main.c jpeg.c jpeg-requantizer.c bit_dispenser.c -o non-roi-recrapify
