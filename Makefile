CFLAGS=-Wall -Werror -O6

run: avr
	./avr example.bin

avr: avr.o usb.o gfs.o

avr.o: avr.c decode.inc io.inc

decode.inc: inst.table gendecode.py
	python gendecode.py

io.inc:
	python genio.py

example.exec: avr.py decode.py example.bin
	python avr.py example.bin

exec: avr.py decode.py blink_slow.bin
	python avr.py blink_slow.bin

example: decode.py
	python decode.py example.bin 18e


compare: dump.txt disasm.txt
	paste dump.txt disasm.txt

dump.txt: decode.py blink_slow.bin
	python decode.py blink_slow.bin > dump.txt

disasm.txt: blink_slow.o
	avr-objdump -z -D blink_slow.o | tail -25 > disasm.txt 
