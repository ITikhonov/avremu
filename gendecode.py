decode=['int decode(uint16_t i) {']
enum=[]
decoders=[]

for x in open('inst.table'):
	if x=='\n': continue
	mask,opcode,name,pat=x.split()
	decoders.append((name,pat))
	if pat.startswith('D'): sign='-'
	else: sign=''
	decode.append("\tif((i&%s)==%s) return %sINST_%s;"%(mask,opcode,sign,name))
	enum.append(name)

decode.append("\t return -1;\n}")

decode.insert(0,('enum {INST_UNKNOWN,INST_')+(',INST_'.join(enum))+'};\n\n')
decode.insert(1,('char *inst_names[]={"UNKNOWN","')+('","'.join(enum))+'"};\n\n')
decode.insert(1,('avr_inst inst_funcs[]={avr_UNIMPL,avr_')+(',avr_'.join(enum))+'};\n\n')

def make_decoder(p):
	v=p.split('_')
	if v[0]=='D': v.pop(0)
		

	args=[]
	pos=16
	for x in v:
		c=x[0]
		if c=='i':
			n=int(x[1:])
		elif c=='a':
			n=int(x[1:])
			a=('u',pos-n,(1<<n)-1,n)
			if len(args)==0: args.append([a])
			else: args[0].append(a)
		elif c=='b':
			n=int(x[1:])
			a=('u',pos-n,(1<<n)-1,n)
			if len(args)==1: args.append([a])
			else: args[1].append(a)
		elif c=='s':
			n=int(x[1:])
			args.append([('s',pos-n,(1<<n)-1,n)])
		else:
			n=int(x)
			args.append([('u',pos-n,(1<<n)-1,n)])
		pos-=n
	assert pos==0
	return args

def translate_decoder(pre,d):
	args=[]
	print d
	for n,x in zip('AB',d):
		if len(x)==2:
			a,b=x
			_,ash,am,aw=a
			_,bsh,bm,bw=b
			r="#define ARG_%s_%s ((((i>>%u)&0x%x)<<%u)|((i>>%u)&0x%x))"%(pre,n,ash,am,bw,bsh,bm)
		elif len(x)==3:
			a,b,c=x
			_,ash,am,aw=a
			_,bsh,bm,bw=b
			_,csh,cm,cw=c
			r="#define ARG_%s_%s ((((i>>%u)&0x%x)<<%u)|(((i>>%u)&0x%x)<<%u)|((i>>%u)&0x%x))"%(pre,n,ash,am,bw,bsh,bm,cw,csh,cm)
		else:
			sign,shift,mask,w=x[0]
			r="#define ARG_%s_%s ((i>>%u)&0x%x)"%(pre,n,shift,mask)
		args.append(r)
	return args
			
	

f=open('decode.inc','w')
f.write('\n'.join(decode))
f.close()

f=open('args.inc','w')

for n,p in decoders:
	d=make_decoder(p)
	if d:
		s=translate_decoder(n,d)
		f.write('\n'.join(s)+'\n')



