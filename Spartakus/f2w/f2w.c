
#include <stdio.h>
#include <stdlib.h>
#include "bitfile.h"

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/dos.h>
#include <exec/exec.h>
/*
#include <dos.h>
#include <dos/dos.h>
#include <pragmas/exec_pragmas.h>
#include <utility/tagitem.h>
#include <time.h>
#include <ctype.h>
#include <proto/timer.h>
#include <devices/timer.h>
*/
#define NUM_CALLS       5

void __asm add64(register __a0 ULONG*, register __d0 ULONG);
void __asm lsr64(register __a0 ULONG*, register __d1 ULONG);
void __asm lsl64(register __a0 ULONG*, register __d1 ULONG);

void __asm add6464(register __a0 ULONG*, register __a1 ULONG*);
void __asm cpy6464(register __a0 ULONG*, register __a1 ULONG*);
void __asm mul323264(register __a0 ULONG*, register __d0 ULONG, register __d1 ULONG);
void __asm mul646432(register __a0 ULONG*, register __a1 ULONG*, register __d0 ULONG);

typedef ULONG LONG64[2];

void printL64(char* s, LONG64* l)
{
	printf("%s: hi: %08x lo: %08x\n", s, (*l)[0], (*l)[1]);
}
 
//FILE *in, *out;
UBYTE* fhc_buffer;
ULONG act_pos, val_pos=0;
ULONG error_count=0;
ULONG debug=12; //173;
ULONG absolute_position=44;	
		
bit_file_t *bfin, *bfout;

int sampleRate = -1;
int numChannels = -1;
int sampleDepth = -1;
long numSamples = -1;

ULONG samples_length=0,
	subframes_length=0,
	result_length=0,
	coefs_length=0;

UBYTE signal;
	
int FIXED_PREDICTION_COEFFICIENTS[5][4] = {
		{NULL, NULL, NULL, NULL},
		{1, NULL, NULL, NULL},
		{2, -1, NULL, NULL},
		{3, -3, 1, NULL},
		{4, -6, 4, -1},
	};

int CXBRK(void) { return 0; }
void __regargs __chkabort(void) { return; }       /* avoid default ctrl-c break */

UBYTE GetSignal(void)
{
	ULONG signals = SetSignal(0,0);
	
	if (signals)
	{
		/* clear signal */
		SetSignal(0,signals); 
	
		switch (signals)
		{
			case SIGBREAKF_CTRL_C : return 1; break;
			case SIGBREAKF_CTRL_D : return 2; break;
			case SIGBREAKF_CTRL_E : return 3; break;
			case SIGBREAKF_CTRL_F : return 4; break;
			default : return 0;
		
		}
	}    	
}

UBYTE WFS(void) /* wait for signal */
{
	ULONG signal;
	while (!(signal=GetSignal())) ;
	return signal;
}
		
UBYTE OpenInOut(char* in, char* out)
{
	//if (!(in=fopen(in, "rb"))) return FALSE;
	//if (!(out=fopen(out, "wb"))) return FALSE;
	printf("in: %s\n", in);
	if (!(bfin=BitFileOpen(in, BF_READ))) { printf("bfin: %u\n", bfin); return FALSE; }
	if (!(bfout=BitFileOpen(out, BF_WRITE))) return FALSE;
	return TRUE;
}


void CloseInOut(void)
{
	//fclose(in);
	//fclose(out);
	BitFileClose(bfin);
	BitFileClose(bfout);
}

ULONG ReadUInt(UBYTE size)
{
	ULONG value, success;
	
	success = BitFileGetBits(bfin, &value, size);
    if (success == EOF)
    {
        perror("reading bits");
        if (0 != BitFileClose(bfin))
        {
            perror("closing bitfile");
        }
        return (EXIT_FAILURE);
    }
    else
    {
        value >>= 32 - size;
		//printf("read bits %0X\n", (unsigned int)value);
    	return value;
	}
}

LONG ReadSignedInt(UBYTE size)
{
	/* this function does not read or return a signed int (LONG) */
	/* (took me 2 days to figure that out ...) */
	LONG value; 
	ULONG success;
	ULONG mask=0;
	int i;
	
	success = BitFileGetBits(bfin, &value, size);
    if (success == EOF)
    {
        perror("reading bits");
        if (0 != BitFileClose(bfin))
        {
            perror("closing bitfile");
        }
        return (EXIT_FAILURE);
    }
    else
    {
		//for (i=0; i<size; i++) mask |= 1<<i;
		//printf("ReadSignedInt(%u): %0X (long: %d) mask: %x\n", size, (unsigned int)value, value, mask);
		
		value >>= 32-size;
		//value &= mask; 
		value <<= 32-size;
		value >>= 32-size;
		
		//printf("ReadSignedInt(%u): %0X (long: %d) mask: %x\n", size, (unsigned int)value, value, mask);
		return value;
	}
/*
public int readSignedInt(int n) throws IOException {
		return (readUint(n) << (32 - n)) >> (32 - n);
	}
*/	
	
}

LONG __asm _CalcRiced(register __d0 ULONG, register __d1 ULONG, register __d2 ULONG);

double LSLL(double v, int p)
{
	ULONG m = (ULONG)1<<p; 
	printf("v: %f m: %u\n", v, m);
	return v * (double)m;
}

double LSRL(double v, int p)
{
	ULONG d = (ULONG)1<<p;
	return v / (double)d;
}

LONG ReadRiceSignedInt(int param) 
{
	/* seems to give correct results up to 28bits */
	/* (>28bits it is totally bogus!) */
	/* Damned ... it needs 29 bits ... */
	
	LONG val = 0, val1, val2;
	LONG r1, r2; //, r3;
	ULONG t;
	//double dval1;
	//ULONG ival1, ival2;
	
	if (param>28) printf("ReadRiceSignedInt(%u) does not work correctly with >28bits!\n", param);
	
	/* values */
	while (ReadUInt(1) == 0)
		val++;
	t = ReadUInt(param);
	
	/* method 1 (C) */	
	val1 = (val << param) | t; //ReadUInt(param);
	r1 = (val1 >> 1) ^ -(val1 & 1); // not sure if >>> (= asr) is needed here ?!
	
	/* method 2 (ASM) */
	r2 = CalcRiced(param, val, t);
	
	/* method 3 (Float) */
	/*dval1 = LSLL((double)val, param);
	ival1 = (LONG)dval1;
	ival2 = ival1 | t;
	r3 = ((LONG)LSRL((double)ival2, 1)) ^ (-(ival2 & 1)); 
	*/

	//printf("val1: %u\n", val1);
	//printf("val: %d t: %d\n", val, t);

	//printf("ReadRiceSignedInt(%u): %d | %d\n", param, r1, r2);

//printf("ReadRiceSignedInt(%u): %d\n", param, r1);
	
	//if (WFS()==1) exit(0); 
	
	return r1;
	
/*
	public long readRiceSignedInt(int param) throws IOException {
		long val = 0;
		while (readUint(1) == 0)
			val++;
		val = (val << param) | readUint(param);
		return (val >>> 1) ^ -(val & 1);
	}
*/
}
	
UBYTE ReadByte(void)
{
	return (UBYTE)BitFileGetChar(bfin);
}

int BitFilePutString(char* s)
{
	int i=0, r=0;
	//printf("BitFilePutString(%s)\n", s);
	while (s[i]!=0) 
	{
		r=BitFilePutChar(s[i], bfout);
		i++;
	}
	return r;
}

void writeLittleInt(int numBytes, ULONG val) 
{
	int i;
	for (i = 0; i < numBytes; i++)
	{
		BitFilePutChar((UBYTE)(val >> (i*8)), bfout);
		if ((act_pos>=44)
			&& 
			(error_count<20)
			&&
		   (
			((UBYTE)(val >> (i*8))) 
			!= 
			fhc_buffer[act_pos]
		   ))
		{
			error_count++;
			printf("Error: position: %u: correct: %x calculated: %x i=%d\n",
				act_pos, fhc_buffer[act_pos], ((UBYTE)(val >> (i*8))), (act_pos-44)>>2
				);
		}
		act_pos++;
	}
}

writeString(char *s) 
{
	BitFilePutString(s);
}

UBYTE NumberOfLeadingZeros(ULONG v)
{
	// Integer in Java is 8 bytes (!?)
	int i=32, found=0;
	while ((!found) && (--i>=0))
	{
		//printf("i: %d\n", i);
		found = (1 << i) & v;
	}
	return 32-(i+1);
}

UBYTE GetCoefsLength(int* coefs)
{
	UBYTE i=0;
	while ((coefs[i]!=NULL) && (i<4)) i++;
	return i;
}

void RestoreLinearPrediction(LONG64* result, int* coefs, int shift) 
{
	static count=0;
	int i, j;
	long sum;
	//double sum;
	LONG tempmul;
	LONG64 lsum; // = { 0, 0 };;
	LONG64 lresult;
	LONG64 ltempmul;
	LONG64 ltempmul2;
	ULONG dp; // debug position
	
	//printf("->absolute_position: %u result_length: %u\n", absolute_position, result_length);
	
	printf("RestoreLinearPrediction(%p, %u) - coefs_length: %d\n", coefs, shift, coefs_length);
	mul323264(ltempmul,0xff,0x10000000);
	//printf("mul323264: hi: %x lo: %x\n", ltempmul[0], ltempmul[1]);
	
	
		
	for (i = coefs_length; i < result_length; i++) {
		
		dp = ((absolute_position - 44)>>2) + i;
		
		if (dp==debug) printf("=====>Debug position: %u\n", dp);
		
		//printf("dp: %u\n", dp);
		sum=0;
		lsum[0] = lsum[1] = 0;
		
		for (j = 0; j < coefs_length; j++)
		{
			
			//if (i<11) printf("result[%d-1-%d = %d]: %d\n", i, j, i - 1 - j, result[i - 1 - j]);
			//tempmul=result[i-1-j]*coefs[j];
			//mul323264(ltempmul,result[i-1-j][1],coefs[j]);
			// hack
//			if ((result[i-1-j][0] & 0xfffff) == 0xfffff) result[i-1-j][0] = 0xffffffff;
			
			mul646432(ltempmul,result[i-1-j],coefs[j]);
			add6464(lsum,ltempmul);
			if (dp==debug) 
			{
				printf("coefs: 0x%x result[][0]: 0x%x result[][1]: 0x%x ", 
				coefs[j], result[i-1-j][0], result[i-1-j][1]);
				printL64("ltempmul", ltempmul);
				printf("lsum: hi: %08x lo: %08x\n", lsum[0], lsum[1]);
			}
//			sum += result[i - 1 - j] * coefs[j];
			
		}
		//if (i<30) printf("lsum: hi: %08x lo: %08x\n", lsum[0], lsum[1]);
		// hack
		//result[i][0] = (result[i][1] & 0x80000000) ? 0xffffffff : 0x0; 
	
		if (dp==debug) printL64("result (BEFORE)", result[i]);
	    if (dp==debug) printL64("lsum", lsum);
	
		lsr64(lsum,shift);
		
		//lresult[0]=0; lresult[1]=result[i];
		//add64(lresult, lsum[1]);
		//add6464(lresult, lsum);
		//result[i][0] = (result[i][1] & 0x80000000) ? 0xffffffff : 0x0; 
		add6464(result[i], lsum);
		if (dp==debug) printL64("result (AFTER)", result[i]);
		
//		result[i] += lsum[1]; //sum >> shift; //sum / 1024.0; //sum >> shift;
		
		if ((i>8) && (i<30)) {
			//printf("%u ", i);
			//printL64("result", result[i]);
			//printf("result[%u]: %d sum: %d shift: %d\n", i, result[i], sum, shift);
			//printf("lsum: hi: %08x lo: %08x\n", lsum[0], lsum[1]);
			//printf("lresult: hi: %x lo: %x\n", lresult[0], lresult[1]);
		}
	}
/*
	for (int i = coefs.length; i < result.length; i++) {
		long sum = 0;
		for (int j = 0; j < coefs.length; j++)
			sum += result[i - 1 - j] * coefs[j];
		result[i] += sum >> shift;
	}
*/
	count++;
	if (count==3) exit(0);
}


void DecodeResiduals(int warmup, LONG64* result) 
{
	int method, paramBits, escapeParam, partitionOrder, numPartitions, partitionSize;
	int i, start, end, param, j, numBits;
	
	printf("DecodeResiduals(%u)\n", warmup);
	//printf("result_length: %u\n", result_length);
	
	method = ReadUInt(2);
	if (method >= 2)
		return;
		//throw new DataFormatException("Reserved residual coding method");
	paramBits = method == 0 ? 4 : 5;
	escapeParam = method == 0 ? 0xF : 0x1F;
	
	partitionOrder = ReadUInt(4);
	numPartitions = 1 << partitionOrder;
	if (result_length % numPartitions != 0) return;
		//throw new DataFormatException("Block size not divisible by number of Rice partitions");
	partitionSize = result_length / numPartitions;
	
	printf("method: %u paramBits: %u partitionOrder: %d numPartitions: %d\n",
		method, paramBits, partitionOrder, numPartitions);
	for (i = 0; i < numPartitions; i++) {
		start = i * partitionSize + (i == 0 ? warmup : 0);
		end = (i + 1) * partitionSize;
		
		param = ReadUInt(paramBits);
		if (param < escapeParam) {
			for (j = start; j < end; j++)
			{
				result[j][0] = 0;
				result[j][1] = ReadRiceSignedInt(param); // 9th element =  0xaa;
				result[j][0] = (result[j][1] & 0x80000000) ? 0xffffffff : 0x0; 
				
				//if (j<start+2) printf("=? %x (param: %u)\n", result[j][1], param);
				//result[j][0] = (result[j][1] & 0x80000000) ? 0xffffffff : 0;
				if (j==debug) { printf("%d: ", j); printL64("-->result[j]", result[j]); }
			}
		} else {
			numBits = ReadUInt(5);
			for (j = start; j < end; j++)
			{
				result[j][0] = 0;
				result[j][1] = ReadSignedInt(numBits); // 9th position 0xaa;
				result[j][0] = (result[j][1] & 0x80000000) ? 0xffffffff : 0x0; 
				//if (i==13) result[j][1]=0xaa;
				//if (j<start+2) printf("=? %x\n", result[j][1]);
				
				//result[j][0] = (result[j][1] & 0x80000000) ? 0xffffffff : 0;
				if (j==debug) { printf("%d: ", j); printL64("-->result[j]", result[j]); }
			}
		}
	}
/*
	int method = in.readUint(2);
	if (method >= 2)
		throw new DataFormatException("Reserved residual coding method");
	int paramBits = method == 0 ? 4 : 5;
	int escapeParam = method == 0 ? 0xF : 0x1F;
	
	int partitionOrder = in.readUint(4);
	int numPartitions = 1 << partitionOrder;
	if (result.length % numPartitions != 0)
		throw new DataFormatException("Block size not divisible by number of Rice partitions");
	int partitionSize = result.length / numPartitions;
	
	for (int i = 0; i < numPartitions; i++) {
		int start = i * partitionSize + (i == 0 ? warmup : 0);
		int end = (i + 1) * partitionSize;
		
		int param = in.readUint(paramBits);
		if (param < escapeParam) {
			for (int j = start; j < end; j++)
				result[j] = in.readRiceSignedInt(param);
		} else {
			int numBits = in.readUint(5);
			for (int j = start; j < end; j++)
				result[j] = in.readSignedInt(numBits);
		}
	}
*/
}
	
void DecodeLinearPredictiveCodingSubframe(int lpcOrder, int sampleDepth, LONG64* result)
{
	int i, precision, shift;
	LONG *coefs;
	
	printf("DecodeLinearPredictiveCodingSubframe(%u)\n", lpcOrder);
	
	for (i = 0; i < lpcOrder; i++)
	{
		//result[i][0] = 0;
		result[i][1] = ReadSignedInt(sampleDepth);
		result[i][0] = (result[i][1] & 0x80000000) ? 0xffffffff : 0;
		//result[i][1] = 0xaa;
		//printf("->result[%u]: %x\n", i, result[i][1]);
		/*if (i==debug)*/ printf("%i: ", i); printL64("->result", result[i]);
	}
	precision = ReadUInt(4) + 1;
	shift = ReadSignedInt(5);
	coefs_length = lpcOrder;
	coefs = AllocVec(sizeof(LONG) * lpcOrder, MEMF_FAST);
	for (i = 0; i < lpcOrder; i++)
	{
		coefs[i] = ReadSignedInt(precision);
		printf("->coefs[%d]: %d (0x%x)\n", i, coefs[i], coefs[i]);
	}
	//printf("coefs_length: %u\n", coefs_length);
	DecodeResiduals(lpcOrder, result);
	RestoreLinearPrediction(result, coefs, shift);

	FreeVec(coefs);
	
/*
	for (int i = 0; i < lpcOrder; i++)
		result[i] = in.readSignedInt(sampleDepth);
	int precision = in.readUint(4) + 1;
	int shift = in.readSignedInt(5);
	int[] coefs = new int[lpcOrder];
	for (int i = 0; i < coefs.length; i++)
		coefs[i] = in.readSignedInt(precision);
	decodeResiduals(in, lpcOrder, result);
	restoreLinearPrediction(result, coefs, shift);
*/
}
	
void DecodeFixedPredictionSubframe(int predOrder, int sampleDepth, LONG64 *result)
{
	int i;
	for (i = 0; i < predOrder; i++)
	{
		//result[i][0] = 0;
		result[i][1] = ReadSignedInt(sampleDepth);
		result[i][0] = (result[i][1] & 0x80000000) ? 0xffffffff : 0;
		/*if (i==debug)*/ printL64("->result[i]", result[i]);
	}
	DecodeResiduals(predOrder, result);
	RestoreLinearPrediction(result, FIXED_PREDICTION_COEFFICIENTS[predOrder], 0);

/*
	for (int i = 0; i < predOrder; i++)
		result[i] = in.readSignedInt(sampleDepth);
	decodeResiduals(in, predOrder, result);
	restoreLinearPrediction(result, FIXED_PREDICTION_COEFFICIENTS[predOrder], 0);
*/
}
	
void DecodeSubframe(int sampleDepth, int numChannels, int blockSize, LONG64* result)
{
	int type;
	int shift;
	int i;
	long templ;
	
	printf("DecodeSubframe(%u)\n", sampleDepth);
	
	ReadUInt(1);
	type = ReadUInt(6);
	shift = ReadUInt(1);
	if (shift == 1) 
	{
		while (ReadUInt(1) == 0)
		{
			shift++;
		}
	}
	sampleDepth -= shift;
	
	printf("type: %u\n", type);
	printf("result_length: %d blockSize: %d\n", result_length, blockSize);
	
	if (type == 0)  // Constant coding
	{
		// result.length <=> blcokSize
		for (i=0; i<result_length/*blockSize*/; i++) 
		{	
			// commenting out this line avoids the crash
			//result[i][0] = 0;
			result[i][1] = ReadSignedInt(sampleDepth);
			result[i][0] = (result[i][1] & 0x80000000) ? 0xffffffff : 0;
		//Arrays.fill(result, 0, numChannels, ReadSignedInt(sampleDepth));
			/*if (i==debug)*/ printL64("->result[i]", result[i]);
		}
	}
	else if (type == 1) 
	{  // Verbatim coding
		for (i = 0; i < numChannels; i++)
		{
			//result[i][0] = 0;
			result[i][1] = ReadSignedInt(sampleDepth);
			result[i][0] = (result[i][1] & 0x80000000) ? 0xffffffff : 0;
			/*if (i==debug)*/ printL64("->result[i]", result[i]);
		}
	} 
	else if ((8 <= type) && (type <= 12))
	{
		DecodeFixedPredictionSubframe(type - 8, sampleDepth, result);
	}
	else if ((32 <= type) && (type <= 63))
	{
		DecodeLinearPredictiveCodingSubframe(type - 31, sampleDepth, result);
	}
	else return;
		//throw new DataFormatException("Reserved subframe type");
	
	for (i = 0; i < numChannels * blockSize; i++)
	{
		lsl64(result[i], shift);
		//*(result + i) <<= shift;
	}

/*
	in.readUint(1);
	int type = in.readUint(6);
	int shift = in.readUint(1);
	if (shift == 1) {
		while (in.readUint(1) == 0)
			shift++;
	}
	sampleDepth -= shift;
	
	if (type == 0)  // Constant coding
		Arrays.fill(result, 0, result.length, in.readSignedInt(sampleDepth));
	else if (type == 1) {  // Verbatim coding
		for (int i = 0; i < result.length; i++)
			result[i] = in.readSignedInt(sampleDepth);
	} else if (8 <= type && type <= 12)
		decodeFixedPredictionSubframe(in, type - 8, sampleDepth, result);
	else if (32 <= type && type <= 63)
		decodeLinearPredictiveCodingSubframe(in, type - 31, sampleDepth, result);
	else
		throw new DataFormatException("Reserved subframe type");
	
	for (int i = 0; i < result.length; i++)
		result[i] <<= shift;
*/
}
	
	
//private static void decodeSubframes(BitInputStream in, int sampleDepth, int chanAsgn, int[][] result)
void DecodeSubframes(int sampleDepth, int chanAsgn, int numChannels, int blockSize, LONG* result)
{
	int ch;
	LONG64 *subframes;
	long side, right;
	int i;
	
	printf("DecodeSubframes(%d, %d, %d, %d, %d)\n", sampleDepth, chanAsgn, numChannels, blockSize, result[0]);
	
	subframes_length = numChannels * blockSize; // ?
	result_length = blockSize;
	subframes = AllocVec(subframes_length * sizeof(LONG64), MEMF_FAST); // ?
	
	
	
	if ((0 <= chanAsgn) && (chanAsgn <= 7)) {
		for (ch = 0; ch < numChannels; ch++)
		{
			//printf("->DecodeSubframe(): ch: %d subframes: %p blocksize: %d subrames_length: %d\n", ch, subframes, blockSize, subframes_length);
			DecodeSubframe(sampleDepth, numChannels, blockSize, subframes + ch*blockSize);
		}
		//printf("hi there ..........\n");
		
		//if (WFS()==1) exit(0); //return FALSE;
	} 
	else if ((8 <= chanAsgn) && (chanAsgn <= 10)) 
	{
		DecodeSubframe(sampleDepth + (chanAsgn == 9 ? 1 : 0), numChannels, blockSize, subframes);
		DecodeSubframe(sampleDepth + (chanAsgn == 9 ? 0 : 1), numChannels, blockSize, subframes + blockSize);
		if (chanAsgn == 8) 
		{
			for (i = 0; i < blockSize; i++)
			{
				subframes[blockSize+i][1] = subframes[i][1] - subframes[blockSize+i][1];
				//*(subframes + blockSize + i) = *(subframes + i) - *(subframes + blockSize + i);
//				if (i<10) printf("subframes[blockSize+i]: %x\n", subframes[blockSize+i][1]);
			}
		} 
		else if (chanAsgn == 9) 
		{
			for (i = 0; i < blockSize; i++)
			{
				add6464(subframes[i],subframes[blockSize+i]);
				//*(subframes + i) += *(subframes + blockSize + i);
//				if (i<10) printf("subframes[i]: %x\n", subframes[i][1]);
			}
		} 
		else if (chanAsgn == 10) 
		{
			for (i = 0; i < blockSize; i++) 
			{
				side = subframes[blockSize+1][1];
				right = subframes[i] - (side >> 1);
				subframes[blockSize+i][1] = right;
				subframes[i][1] = right + side;
				
//				if (i<10) printf("subframes[blockSize+i]: %x\n", subframes[blockSize+i][1]);
//				if (i<10) printf("subframes[i]: %x\n", subframes[i][1]);
				
				/*side = *(subframes + blockSize + 1);
				right = *(subframes + i) - (side >> 1);
				*(subframes + blockSize + i) = right;
				*(subframes + i) = right + side;
				*/
			}
		}
	} else { printf("Exception\n"); return; }
		//throw new DataFormatException("Reserved channel assignment");
	
	for (ch = 0; ch < numChannels; ch++) 
	{
		for (i = 0; i < blockSize; i++)
		{
			result[ch*blockSize+i] = subframes[ch*blockSize+i][1];
//			if (i<10) printf("result<-subframes: %x\n", subframes[ch*blockSize+i][1]);
//			if (i==10) printf("---------------\n");
			//*(result + blockSize + i) = (int) *(subframes + blockSize + i); 
		}
	}
	
	absolute_position += blockSize * numChannels * 2;
	FreeVec(subframes);
	
/*	
	int blockSize = result[0].length;
	long[][] subframes = new long[result.length][blockSize];
	if (0 <= chanAsgn && chanAsgn <= 7) {
		for (ch = 0; ch < result.length; ch++)
			decodeSubframe(in, sampleDepth, subframes[ch]);
	} else if (8 <= chanAsgn && chanAsgn <= 10) {
		decodeSubframe(in, sampleDepth + (chanAsgn == 9 ? 1 : 0), subframes[0]);
		decodeSubframe(in, sampleDepth + (chanAsgn == 9 ? 0 : 1), subframes[1]);
		if (chanAsgn == 8) {
			for (int i = 0; i < blockSize; i++)
				subframes[1][i] = subframes[0][i] - subframes[1][i];
		} else if (chanAsgn == 9) {
			for (int i = 0; i < blockSize; i++)
				subframes[0][i] += subframes[1][i];
		} else if (chanAsgn == 10) {
			for (int i = 0; i < blockSize; i++) {
				long side = subframes[1][i];
				long right = subframes[0][i] - (side >> 1);
				subframes[1][i] = right;
				subframes[0][i] = right + side;
			}
		}
	} else
		throw new DataFormatException("Reserved channel assignment");
	for (int ch = 0; ch < result.length; ch++) {
		for (int i = 0; i < blockSize; i++)
			result[ch][i] = (int)subframes[ch][i];
	}
*/
}
	
UBYTE DecodeFrame(int numChannels, int sampleDepth)
{
	// Read a ton of header fields, and ignore most of them
	int temp, sync;
	int blockSizeCode;
	int sampleRateCode;
	int chanAsgn;
	int blockSize;
	int *samples;
	int r;
	int i, j, val;
	
	printf("DecodeFrame(%u, %u)\n", numChannels, sampleDepth);
	
	temp = ReadByte();
	if (temp == -1) return FALSE;
	sync = temp << 6 | ReadUInt(6);
	if (sync != 0x3FFE) return;
		//throw new DataFormatException("Sync code expected");
	
	ReadUInt(1);
	ReadUInt(1);
	blockSizeCode = ReadUInt(4);
	sampleRateCode = ReadUInt(4);
	chanAsgn = ReadUInt(4);
	ReadUInt(3);
	ReadUInt(1);
	
	temp = NumberOfLeadingZeros(~(ReadUInt(8) << 24)) - 1;
	//temp = Integer.numberOfLeadingZeros(~(in.readUint(8) << 24)) - 1;
	//printf("NumberOfLeadingZeros: %d\n", temp);
	//if (WFS()==1) return FALSE;
	
	for (i = 0; i < temp; i++)
		ReadUInt(8);
	
	if (blockSizeCode == 1)
		blockSize = 192;
	else if (2 <= blockSizeCode && blockSizeCode <= 5)
		blockSize = 576 << (blockSizeCode - 2);
	else if (blockSizeCode == 6)
		blockSize = ReadUInt(8) + 1;
	else if (blockSizeCode == 7)
		blockSize = ReadUInt(16) + 1;
	else if (8 <= blockSizeCode && blockSizeCode <= 15)
		blockSize = 256 << (blockSizeCode - 8);
	else return;
		//throw new DataFormatException("Reserved block size");
	
	if (sampleRateCode == 12)
		ReadUInt(8);
	else if (sampleRateCode == 13 || sampleRateCode == 14)
		ReadUInt(16);
	
	ReadUInt(8);
	
	// Decode each channel's subframe, then skip footer
	samples_length = numChannels * blockSize;
	samples = AllocVec(sizeof(LONG)  * samples_length, MEMF_FAST);
	printf("AllocVec(): samples: %p (length: %u)\n", samples, samples_length);
	//int[][] samples = new int[numChannels][blockSize];
	
	DecodeSubframes(sampleDepth, chanAsgn, numChannels, blockSize, samples);
	
	r=BitFileByteAlign(bfin);	// ?
	//in.alignToByte();
	
	ReadUInt(16);
	
	// Write the decoded samples
	for (i = 0; i < blockSize; i++) 
	{
		for (j = 0; j < numChannels; j++) 
		{
			val = *(samples+j*blockSize+i);
			if ((val_pos>=debug-4) && (val_pos<debug+4)) printf("val[%d]: %x\n", val_pos, val);
			val_pos++;
			if (sampleDepth == 8)
			{
				val += 128;
			}
			writeLittleInt(sampleDepth / 8, val);
		}
	}
	
	FreeVec(samples);
	return TRUE;
	
/*	int temp = in.readByte();
	if (temp == -1)
		return false;
	int sync = temp << 6 | in.readUint(6);
	if (sync != 0x3FFE)
		throw new DataFormatException("Sync code expected");
	
	in.readUint(1);
	in.readUint(1);
	int blockSizeCode = in.readUint(4);
	int sampleRateCode = in.readUint(4);
	int chanAsgn = in.readUint(4);
	in.readUint(3);
	in.readUint(1);
	
	temp = Integer.numberOfLeadingZeros(~(in.readUint(8) << 24)) - 1;
	for (int i = 0; i < temp; i++)
		in.readUint(8);
	
	int blockSize;
	if (blockSizeCode == 1)
		blockSize = 192;
	else if (2 <= blockSizeCode && blockSizeCode <= 5)
		blockSize = 576 << (blockSizeCode - 2);
	else if (blockSizeCode == 6)
		blockSize = in.readUint(8) + 1;
	else if (blockSizeCode == 7)
		blockSize = in.readUint(16) + 1;
	else if (8 <= blockSizeCode && blockSizeCode <= 15)
		blockSize = 256 << (blockSizeCode - 8);
	else
		throw new DataFormatException("Reserved block size");
	
	if (sampleRateCode == 12)
		in.readUint(8);
	else if (sampleRateCode == 13 || sampleRateCode == 14)
		in.readUint(16);
	
	in.readUint(8);
	
	// Decode each channel's subframe, then skip footer
	int[][] samples = new int[numChannels][blockSize];
	decodeSubframes(in, sampleDepth, chanAsgn, samples);
	in.alignToByte();
	in.readUint(16);
	
	// Write the decoded samples
	for (int i = 0; i < blockSize; i++) {
		for (int j = 0; j < numChannels; j++) {
			int val = samples[j][i];
			if (sampleDepth == 8)
				val += 128;
			writeLittleInt(sampleDepth / 8, val, out);
		}
	}
	return true;
*/
}
	
		
void DecodeFile(void) 
{
	ULONG MagicNumber;
	UBYTE last=0;
	int type;
	int length ;
	int i;
	long sampleDataLen;
	
	sampleRate = -1;
	numChannels = -1;
	sampleDepth = -1;
	numSamples = -1;
	
	// Handle FLAC header and metadata blocks
	MagicNumber=ReadUInt(32);
	printf("MagicNumber: %x\n", MagicNumber);

	if (MagicNumber != 0x664C6143) return;

	for (; !last; ) 
	{
		last = ReadUInt(1) != 0;
		type = ReadUInt(7);
		length = ReadUInt(24);
		printf("last: %u\n", last);
		printf("type: %u\n", type);
		printf("length: %u\n", length);
		//type=0;
		if (type == 0) {  // Stream info block
			ReadUInt(16);
			ReadUInt(16);
			ReadUInt(24);
			ReadUInt(24);
			sampleRate = ReadUInt(20);
			numChannels = ReadUInt(3) + 1;
			sampleDepth = ReadUInt(5) + 1;
			numSamples = (long)ReadUInt(18) << 18 | ReadUInt(18);
			printf("type: %u\n", type);
			printf("SampleRate: %u (%x)\n", sampleRate, sampleRate);
			printf("NumChannels: %u\n", numChannels);
			printf("SampleDepth: %u\n", sampleDepth);
			printf("NumSamples: %u\n", numSamples);
			for (i = 0; i < 16; i++)
				ReadUInt(8);
		} else {
			for (i = 0; i < length; i++)
				ReadUInt(8);
		}
	}
	if (sampleRate == -1) return;
		//throw new DataFormatException("Stream info metadata block absent");
	if (sampleDepth % 8 != 0) return;
		//throw new RuntimeException("Sample depth not supported");
	
	// Start writing WAV file headers
	sampleDataLen = numSamples * numChannels * (sampleDepth / 8);
	writeString("RIFF");
	writeLittleInt(4, (int)sampleDataLen + 36);
	writeString("WAVE");
	writeString("fmt ");
	writeLittleInt(4, 16);
	writeLittleInt(2, 0x0001);
	writeLittleInt(2, numChannels);
	writeLittleInt(4, sampleRate);
	writeLittleInt(4, sampleRate * numChannels * (sampleDepth / 8));
	writeLittleInt(2, numChannels * (sampleDepth / 8));
	writeLittleInt(2, sampleDepth);
	writeString("data");
	writeLittleInt(4, (int)sampleDataLen);
	act_pos=44;
	
	while (DecodeFrame(numChannels, sampleDepth));
	
/*	
	if (in.readUint(32) != 0x664C6143)
		throw new DataFormatException("Invalid magic string");
	int sampleRate = -1;
	int numChannels = -1;
	int sampleDepth = -1;
	long numSamples = -1;
	for (boolean last = false; !last; ) {
		last = in.readUint(1) != 0;
		int type = in.readUint(7);
		int length = in.readUint(24);
		if (type == 0) {  // Stream info block
			in.readUint(16);
			in.readUint(16);
			in.readUint(24);
			in.readUint(24);
			sampleRate = in.readUint(20);
			numChannels = in.readUint(3) + 1;
			sampleDepth = in.readUint(5) + 1;
			numSamples = (long)in.readUint(18) << 18 | in.readUint(18);
			for (int i = 0; i < 16; i++)
				in.readUint(8);
		} else {
			for (int i = 0; i < length; i++)
				in.readUint(8);
		}
	}
	if (sampleRate == -1)
		throw new DataFormatException("Stream info metadata block absent");
	if (sampleDepth % 8 != 0)
		throw new RuntimeException("Sample depth not supported");
	
	// Start writing WAV file headers
	long sampleDataLen = numSamples * numChannels * (sampleDepth / 8);
	writeString("RIFF", out);
	writeLittleInt(4, (int)sampleDataLen + 36, out);
	writeString("WAVE", out);
	writeString("fmt ", out);
	writeLittleInt(4, 16, out);
	writeLittleInt(2, 0x0001, out);
	writeLittleInt(2, numChannels, out);
	writeLittleInt(4, sampleRate, out);
	writeLittleInt(4, sampleRate * numChannels * (sampleDepth / 8), out);
	writeLittleInt(2, numChannels * (sampleDepth / 8), out);
	writeLittleInt(2, sampleDepth, out);
	writeString("data", out);
	writeLittleInt(4, (int)sampleDataLen, out);
	
	// Decode FLAC audio frames and write raw samples
	while (decodeFrame(in, numChannels, sampleDepth, out));
*/
}

void main(int argc, char **argv) 
{

	ULONG count=30;
	double dt=923090673; //4615045337.0;
	FILE *fhc;
	
	/*printf("19: %u\n", NumberOfLeadingZeros(19));
	printf("155: %u\n", NumberOfLeadingZeros(155));
	printf("10: %u\n", NumberOfLeadingZeros(10));
	printf("-15: %u\n", NumberOfLeadingZeros(-15));
	*/
	
	//printf("dt: %f\n", dt);
	
	if (argc != 3) {
		printf("Usage: F2W InFile.flac OutFile.wav");
		return;
	}
	
	if (!(OpenInOut(argv[1], argv[2]))) return;
	
	// open correct file for debugging comparing
	fhc=fopen("sc:spartakus/f2w/sample1.wav","rb");
	fhc_buffer=AllocVec(1000000, MEMF_ANY);
	fread(fhc_buffer,1,900000,fhc);
	act_pos=0;
	
	/*
	while (count)
	{
		ReadRiceSignedInt(29);
		count--;
	}
	*/
	DecodeFile();
	
	FreeVec(fhc_buffer);
	fclose(fhc);
	CloseInOut();
}
