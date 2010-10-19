decode=['int decode(uint16_t i) {']
enum=[]

for x in open('inst.table'):
	if x=='\n': continue
	mask,opcode,name,pat=x.split()
	if pat.startswith('D'): sign='-'
	else: sign=''
	decode.append("\tif((i&%s)==%s) return %sINST_%s;"%(mask,opcode,sign,name))
	enum.append(name)

decode.append("\t return -1;\n}")

decode.insert(0,('enum {INST_UNKNOWN,INST_')+(',INST_'.join(enum))+'};\n\n')
decode.insert(1,('char *inst_names[]={"UNKNOWN","')+('","'.join(enum))+'"};\n\n')

open('decode.inc','w').write('\n'.join(decode))




