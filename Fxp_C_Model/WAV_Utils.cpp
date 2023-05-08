//++++++++++++++++++++++++++++++++++++++++++++++++++
//
// WAV file utilities
//
//++++++++++++++++++++++++++++++++++++++++++++++++++

#include "WAV_Utils.h"

using namespace std;

int8_t cWAVops::OpenReadFile(char* const InFileName)
{
int8_t RetVal = WavReadNoError;

    if (Fp == NULL)
    {
        fopen_s(&Fp, InFileName, "rb");
        if (Fp != NULL)
        {
            fread(&WavHeader, 1, sizeof(WavHeader), Fp);
            // Check the following:
            if (WavHeader.AudioFormat != 1)
                RetVal = WavReadBadAudioFormat;
            else if (strncmp(WavHeader.RIFFChunkID, "RIFF", 4) != 0)
                RetVal = WavReadRIFFmissing;
            else if (strncmp(WavHeader.WAVEFormat, "WAVE", 4) != 0)
                RetVal = WavReadWAVEmissing;
            else if (strncmp(WavHeader.fmtSubchunk1ID, "fmt ", 4) != 0)
                RetVal = WavRead_fmt_missing;
            else if (strncmp(WavHeader.dataSubchunk2ID, "data", 4) != 0)
                RetVal = WavRead_data_missing;
            else if (WavHeader.fmtSubchunk1Size != 16)
                RetVal = WavReadBadSubchunk1Size;
            // Derive bytes per sample
            BytesPerSample = WavHeader.BitsPerSample >> 3;      // Divide bites by 8 to get bytes
            SignExtShift = 32 - (BytesPerSample<<3);            // Sign extension shift to put into 32b container

            // dataSubchunk2Size = NumSamples * NumChannels * BytesPerSample
            NumSamples = (WavHeader.dataSubchunk2Size / BytesPerSample) / WavHeader.NumChannels;
            SamplesAccessed = 0;        // Reset how many samples have been read
            FileAccessType = 1;         // Mark as read file
        }
        else
            RetVal = WavReadFileOpenBad;
    }
    else
        RetVal = WavReadFpAlreadyOpen;

    return RetVal;
}


// Read BufSamples into ValBuf. Use 32b containers pointed to by ValBuf to store values even if samples are smaller 
// Also keeps track of how many overall samples have been read from this file and limits to sample size
// Returns actual number read
uint16_t cWAVops::ReadNVals(uint16_t BufSamples, int32_t* ValBuf)
{
uint16_t i;
int32_t Temp;

    for (i = 0; (i < BufSamples) && (SamplesAccessed < NumSamples); i++, SamplesAccessed++)
    {
        fread(&Temp, 1, BytesPerSample, Fp);
        Temp = Temp << SignExtShift;       // shift to top
        ValBuf[i] = Temp >> SignExtShift;  // then restore with sign extend
    }
    return i;
}


int8_t cWAVops::OpenMonoWriteFile(char* const OutFileName, uint16_t SampleRate, uint16_t BitsPerSample, uint32_t N)
{
int8_t RetVal = WavReadNoError;
uint32_t DataSize;

    if (Fp == NULL)     // File not open yet
    {
        fopen_s(&Fp, OutFileName, "wb");
        if (Fp != NULL)
        {
        // Set up the header & class members
            NumSamples = N;
            BytesPerSample = BitsPerSample >> 3;    // Divide bits by 8 to get bytes
            SignExtShift = 32 - (BytesPerSample<<3);    // sign extension shift bits for 32b container
            SamplesAccessed = 0;        // reset # of samples written
            FileAccessType = 2;

            WavHeader.RIFFChunkID[0] = 'R';
            WavHeader.RIFFChunkID[1] = 'I';
            WavHeader.RIFFChunkID[2] = 'F';
            WavHeader.RIFFChunkID[3] = 'F';
            DataSize = NumSamples * (BitsPerSample >> 3);
            WavHeader.ChunkSize = 36 + DataSize;
            WavHeader.WAVEFormat[0] = 'W';
            WavHeader.WAVEFormat[1] = 'A';
            WavHeader.WAVEFormat[2] = 'V';
            WavHeader.WAVEFormat[3] = 'E';
            WavHeader.fmtSubchunk1ID[0] = 'f';
            WavHeader.fmtSubchunk1ID[1] = 'm';
            WavHeader.fmtSubchunk1ID[2] = 't';
            WavHeader.fmtSubchunk1ID[3] = ' ';
            WavHeader.fmtSubchunk1Size = 16;    // default
            WavHeader.AudioFormat = 1;          // for PCM
            WavHeader.NumChannels = 1;          // mono
            WavHeader.SampleRate = SampleRate;
            WavHeader.ByteRate = SampleRate * BytesPerSample;   // Assumes mono (SampleRate * NumChannels * BitsPerSample / 8)
            WavHeader.BlockAlign = BytesPerSample;              // Again, assumes mono (NumChannels * BitsPerSample / 8)
            WavHeader.BitsPerSample = BitsPerSample;
            WavHeader.dataSubchunk2ID[0] = 'd';
            WavHeader.dataSubchunk2ID[1] = 'a';
            WavHeader.dataSubchunk2ID[2] = 't';
            WavHeader.dataSubchunk2ID[3] = 'a';
            WavHeader.dataSubchunk2Size = NumSamples * BytesPerSample;      // Again, mono (NumSamples * NumChannels * BitsPerSample / 8)
        // Write the header to start the file
            fwrite(&WavHeader, 1, sizeof(WavHeader), Fp);
        }
        else
            RetVal = WavWriteFileOpenBad;
    }
    else
        RetVal = WavWriteFileAlreadyOpen;

    return RetVal;
}


// write BufSamples of samples coming from ValBuf. Use 32b containers pointed to by ValBuf to hold values even if samples are smaller
// Also keeps track of overall number of samples written and stops when we hit specified number of samples (specified when file was opened)
// Returns actual number written
uint16_t cWAVops::WriteNVals(uint16_t BufSamples, int32_t* ValBuf)
{
uint16_t i;

    for (i = 0; (i < BufSamples) && (SamplesAccessed < NumSamples); i++, SamplesAccessed++)
    {
        fwrite(&ValBuf[i], 1, BytesPerSample, Fp);
    }
    return i;
}

