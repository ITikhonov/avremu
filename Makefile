
example: decode.py
	python decode.py example.bin 18e


exec: avr.py decode.py blink_slow.bin
	python avr.py blink_slow.bin

compare: dump.txt disasm.txt
	paste dump.txt disasm.txt

dump.txt: decode.py blink_slow.bin
	python decode.py blink_slow.bin > dump.txt

disasm.txt: blink_slow.o
	avr-objdump -z -D blink_slow.o | tail -25 > disasm.txt 
