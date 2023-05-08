//++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Header file for WAV file utilities
//
//++++++++++++++++++++++++++++++++++++++++++++++++++


#ifndef _WAV_UTILS_H
#define _WAV_UTILS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

//++++++++++++++++++++++++++++++++++++++++++++++++++
// WAV file header structure
// From: https://stackoverflow.com/questions/23030980/creating-a-stereo-wav-file-using-c
// The header of a wav file Based on:
// https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
// 
// Size of this header: 44 bytes
//++++++++++++++++++++++++++++++++++++++++++++++++++

typedef struct _WAV_File_Header
{
    char        RIFFChunkID[4];		// "RIFF"
    uint32_t    ChunkSize;			// size of entire file; 4 + (8 + fmtSubchunk1Size) + (8 + dataSubchunk2Size) = 36+data_size
    char        WAVEFormat[4];		// "WAVE"

    char        fmtSubchunk1ID[4];	// "fmt " (note space at end)
    uint32_t    fmtSubchunk1Size;   // set at 16 bytes for the 'fmt' chunk
    uint16_t    AudioFormat;        // 1 for PCM (default)
    uint16_t    NumChannels;        // 1 (mono) or 2 (stereo)
    uint32_t    SampleRate;         // Common rates: 8000, 16000, 20000, 22050, 24000, 44100, 48000; can be others
    uint32_t    ByteRate;           // (SampleRate * NumChannels * BitsPerSample / 8)
    uint16_t    BlockAlign;         // (NumChannels * BitsPerSample / 8)
    uint16_t    BitsPerSample;      // Usually 16, 24, or 32; must be divisible by 8

    char        dataSubchunk2ID[4]; // "data"
    uint32_t    dataSubchunk2Size;  // NumSamples * NumChannels * BitsPerSample / 8;

} strWAVFileHeader;


class cWAVops
{
private:
    strWAVFileHeader    WavHeader;
    FILE*               Fp;
    uint8_t             FileAccessType;     // 0 = not open yet, 1 = read, 2 = write
    uint32_t            NumSamples;
    uint32_t            SamplesAccessed;    // Samples already read or written
    uint16_t            BytesPerSample;
    uint16_t            SignExtShift;       // Bits to shift up then down to sign extend into 32b container

public:
    cWAVops()
    {
        WavHeader.ChunkSize = 0;
        WavHeader.fmtSubchunk1Size = 0;
        WavHeader.AudioFormat = 0;
        WavHeader.NumChannels = 0;
        WavHeader.SampleRate = 0;
        WavHeader.ByteRate = 0;
        WavHeader.BlockAlign = 0;
        WavHeader.BitsPerSample = 0;
        WavHeader.dataSubchunk2Size = 0;
        Fp = NULL;
        FileAccessType = 0;
        NumSamples = 0;
        SamplesAccessed = 0;
        BytesPerSample = 0;
        SignExtShift = 0;
    }
    ~cWAVops()
    {
        CloseFile();
    }

    int8_t OpenReadFile(char* const InFileName);    // Populates sample frequency, number of samples, file name, and file pointer

    inline uint32_t GetNumSamples() { return NumSamples; }
    inline uint16_t GetBitsPerSample() { return WavHeader.BitsPerSample; }
    inline uint32_t GetSampleRate() { return  WavHeader.SampleRate; }
    inline bool IsMono() { bool Ret = (WavHeader.NumChannels == 1); return Ret; }

    uint16_t WriteNVals(uint16_t NumVals, int32_t* ValBuf);       // write NumVals of samples coming from ValBuf;
    int8_t OpenMonoWriteFile(char* const OutFileName, uint16_t SampleRate, uint16_t BitsPerSample, uint32_t NumSamples);
    uint16_t ReadNVals(uint16_t NumVals, int32_t* ValBuf);        // Returns number of samples actually read

    inline void CloseFile() 
    { 
        if(Fp != NULL)
        {
            fclose(Fp);
            Fp = NULL;
        }
    }
};

typedef enum WavReadErrors
{
    WavReadNoError = 0,
    WavReadFpAlreadyOpen,
    WavReadBadAudioFormat,
    WavReadRIFFmissing,
    WavReadWAVEmissing,
    WavRead_fmt_missing,
    WavRead_data_missing,
    WavReadBadSubchunk1Size,
    WavReadFileOpenBad

} eWavReadErrors;

typedef enum WavWriteErrors
{
    WavWriteNoError = 0,
    WavWriteFileAlreadyOpen,
    WavWriteFileOpenBad

} eWavWriteErrors;


#endif		// _WAV_UTILS_H

