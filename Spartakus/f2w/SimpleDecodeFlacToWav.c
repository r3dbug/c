/* 
 * Simple FLAC decoder (Java) => (C89)
 * 
 * Copyright (c) 2017 Project Nayuki. (MIT License)
 * https://www.nayuki.io/page/simple-flac-implementation
 * 
 * "Translation" (Java => C89) by RedBug (2025)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * - The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 * - The Software is provided "as is", without warranty of any kind, express or
 *   implied, including but not limited to the warranties of merchantability,
 *   fitness for a particular purpose and noninfringement. In no event shall the
 *   authors or copyright holders be liable for any claim, damages or other
 *   liability, whether in an action of contract, tort or otherwise, arising from,
 *   out of or in connection with the Software or the use or other dealings in the
 *   Software.
 */

//import java.io.*;
//import java.nio.charset.StandardCharsets;
//import java.util.Arrays;
//import java.util.zip.DataFormatException;

#include <stdio.h>
#include <exec/types.h>

UBYTE DecodeFrame(FILE*, int, int, FILE*);

FILE *in, *out;

struct FLACHeaderAndMetadata {
	ULONG MagicString; 	// 32bits, 0x664C6143
	UBYTE last : 1;
	UBYTE type : 7;		// this structure only handles type == 0 cases!
	ULONG length : 24;	// (custom length not implemented)
	ULONG padding1 : 8;
	UBYTE padding2[9];
	ULONG SampleRate : 20;
	UBYTE NumChannels : 3;
	UBYTE SampleDepth : 5;
	ULONG NumSamples : 32;
	UBYTE data[16];
};

void CloseInOut(void)
{
	fclose(in);
	fclose(out);
}

void DecodeFile(FILE *in, FILE *out) 
{
	struct FLACHeaderAndMetadata header;
	int i;
	UBYTE tb;
	
	fread (&header, 1, sizeof(header), in);
	
	if (header.MagicString!=0x664C6143)
	{
		printf("Invalid magic string (%x).\n", header.MagicString);
		CloseInOut();
		return;
	}
	
	printf("sizeof(header): %u\n", sizeof(header));
	printf("type: %u\n", header.type);
	printf("SampleRate: %u (%x)\n", header.SampleRate, header.SampleRate);
	printf("NumChannels: %u\n", header.NumChannels+1);
	printf("SampleDepth: %u\n", header.SampleDepth+1);
	printf("NumSamples: %u\n", header.NumSamples);
	
	if (header.type == 0)
	{
		printf("type == 0\n");
		for (i=0; i<15; i++)
		{
			fread(&tb, 1, 1, in);
			//printf("tb: %x\n", tb);
		}
	}
	else
	{
		printf("type != 0: length: %u\n", header.length);
		for (i=0; i<header.length-20; i++)
		{
			fread(&tb, 1, 1, in);
			//printf("tb: %x\n", tb);
		}	
	}
	/* writing of WAV header not implemented */
	
	while (DecodeFrame(in, header.NumChannels+1, header.SampleDepth+1, out));
	
	CloseInOut();
	return;
	
/*		// Handle FLAC header and metadata blocks
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
	
/*	
	private static void writeLittleInt(int numBytes, int val, OutputStream out) throws IOException {
		for (int i = 0; i < numBytes; i++)
			out.write(val >>> (i * 8));
	}
	
	private static void writeString(String s, OutputStream out) throws IOException {
		out.write(s.getBytes(StandardCharsets.UTF_8));
	}
	
*/	
UBYTE DecodeFrame(FILE* in, int numChannels, int sampleDepth, FILE* out)
{
		// Read a ton of header fields, and ignore most of them
		UBYTE last, act;
		UWORD temp;
		UWORD tb;
		ULONG tl;
		ULONG sync;
		WORD blockSizeCode;
		WORD sampleRateCode;
		WORD chanAsgn;
		int i;
		
		last=0;
ssync:		
		fread(&act, 1, 1, in); 
		if (act == 0xff/*-1*/)
		{
			//printf("act: %x\n", act);
			return FALSE;
		}			
		sync = (((UWORD)last)<<6) | (((UWORD)act)>>2)<<2; //temp << 6 | in.readUint(6);
		//printf("Offset: %u last: %x act: %x sync: %x\n", ftell(in), last, act, sync);
		last = act;
		
		if (sync != 0x3FFE) goto ssync; //return FALSE;
			//throw new DataFormatException("Sync code expected");
		
		//in.readUint(1);
		//in.readUint(1);
		fread(&temp, 1, 2, in);
		blockSizeCode = temp >> 12; // in.readUint(4);
		sampleRateCode = (temp >> 8) & 0xf; //in.readUint(4);
		chanAsgn = (temp >> 4) & 0xf; //in.readUint(4);
		//in.readUint(3);
		//in.readUint(1);
		
		tb=fread(&tb, 1, 1, in);
		//temp = Integer.numberOfLeadingZeros(~(in.readUint(8) << 24)) - 1;
		tl= (ULONG)tb << 24 - 1;
		for (i = 0; i < tl; i++)
			fread(&tb,1,1,in); //in.readUint(8);
		
/*		
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
	return FALSE;
}
	
/*	
	private static void decodeSubframes(BitInputStream in, int sampleDepth, int chanAsgn, int[][] result)
			throws IOException, DataFormatException {
		int blockSize = result[0].length;
		long[][] subframes = new long[result.length][blockSize];
		if (0 <= chanAsgn && chanAsgn <= 7) {
			for (int ch = 0; ch < result.length; ch++)
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
	}
	
	
	private static void decodeSubframe(BitInputStream in, int sampleDepth, long[] result)
			throws IOException, DataFormatException {
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
	}
	
	
	private static void decodeFixedPredictionSubframe(BitInputStream in, int predOrder, int sampleDepth, long[] result)
			throws IOException, DataFormatException {
		for (int i = 0; i < predOrder; i++)
			result[i] = in.readSignedInt(sampleDepth);
		decodeResiduals(in, predOrder, result);
		restoreLinearPrediction(result, FIXED_PREDICTION_COEFFICIENTS[predOrder], 0);
	}
	
	private static final int[][] FIXED_PREDICTION_COEFFICIENTS = {
		{},
		{1},
		{2, -1},
		{3, -3, 1},
		{4, -6, 4, -1},
	};
	
	
	private static void decodeLinearPredictiveCodingSubframe(BitInputStream in, int lpcOrder, int sampleDepth, long[] result)
			throws IOException, DataFormatException {
		for (int i = 0; i < lpcOrder; i++)
			result[i] = in.readSignedInt(sampleDepth);
		int precision = in.readUint(4) + 1;
		int shift = in.readSignedInt(5);
		int[] coefs = new int[lpcOrder];
		for (int i = 0; i < coefs.length; i++)
			coefs[i] = in.readSignedInt(precision);
		decodeResiduals(in, lpcOrder, result);
		restoreLinearPrediction(result, coefs, shift);
	}
	
	
	private static void decodeResiduals(BitInputStream in, int warmup, long[] result) throws IOException, DataFormatException {
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
	}
	
	
	private static void restoreLinearPrediction(long[] result, int[] coefs, int shift) {
		for (int i = coefs.length; i < result.length; i++) {
			long sum = 0;
			for (int j = 0; j < coefs.length; j++)
				sum += result[i - 1 - j] * coefs[j];
			result[i] += sum >> shift;
		}
	}
	
}



final class BitInputStream implements AutoCloseable {
	
	private InputStream in;
	private long bitBuffer;
	private int bitBufferLen;
	
	
	public BitInputStream(InputStream in) {
		this.in = in;
	}
	
	
	public void alignToByte() {
		bitBufferLen -= bitBufferLen % 8;
	}
	
	
	public int readByte() throws IOException {
		if (bitBufferLen >= 8)
			return readUint(8);
		else
			return in.read();
	}
	
	
	public int readUint(int n) throws IOException {
		while (bitBufferLen < n) {
			int temp = in.read();
			if (temp == -1)
				throw new EOFException();
			bitBuffer = (bitBuffer << 8) | temp;
			bitBufferLen += 8;
		}
		bitBufferLen -= n;
		int result = (int)(bitBuffer >>> bitBufferLen);
		if (n < 32)
			result &= (1 << n) - 1;
		return result;
	}
	
	
	public int readSignedInt(int n) throws IOException {
		return (readUint(n) << (32 - n)) >> (32 - n);
	}
	
	
	public long readRiceSignedInt(int param) throws IOException {
		long val = 0;
		while (readUint(1) == 0)
			val++;
		val = (val << param) | readUint(param);
		return (val >>> 1) ^ -(val & 1);
	}
	
	
	public void close() throws IOException {
		in.close();
	}
	
}
*/

void main(int argc, char **argv) 
{

	if (argc != 3) {
		printf("Usage: SimpleDecodeFlacToWav InFile.flac OutFile.wav");
		return;
	}
	
	if (!(in=fopen(argv[1], "rb"))) return;
	if (!(out=fopen(argv[2], "wb"))) return;
	
	DecodeFile(in,out);
}

