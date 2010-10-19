f=open('io.inc','w')

f.write('io_read_func io_read_funcs[0xe0]={')
for x in range(0x20,0x100):
	f.write('avr_IOR%x,'%(x,))
f.write('};\n');

f.write('io_write_func io_write_funcs[0xe0]={')
for x in range(0x20,0x100):
	f.write('avr_IOW%x,'%(x,))
f.write('};\n');
