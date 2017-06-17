runme: demo.o gen.o
	gcc -m32 demo.o gen.o -o runme
gen.o: gen.asm
	nasm -f elf32 -F dwarf gen.asm -o gen.o
demo.o: demo.c gen.h
	gcc -m32 -c -g demo.c -o demo.o -Wall -Wextra -Wpedantic
clean:
	rm -f *.o runme
