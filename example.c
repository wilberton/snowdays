/* 
 A simple example showing usage of the modplayer api
 Loads a mod file and saves out the first 30 seconds as a wav file.

 Compile with:
 gcc example.c -o example -std=c99

 Run:
 example <modfile.mod>

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOD_PLAYER_IMPLEMENTATION
#include "modplayer.h"

// some helper functions for writing wav files
static void write_tag(const char* tag, FILE* fp)
{
	fwrite(tag, 1, 4, fp);
}

static void write_int32(int i, FILE* fp)
{
	fwrite(&i, 4, 1, fp);
}

static void write_int16(int i, FILE* fp)
{
	fwrite(&i, 2, 1, fp);
}

int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		printf("Usage: modplayer <modfile.mod>\n");
		exit(0);
	}

	char* modfile = argv[1];

	// initialise our mod-player
    mp_mod_player* modplayer = modplayer_create_from_file(modfile);

	if(modplayer == NULL)
	{
		fprintf(stderr, "Error creating mod. Terminating.\n");
		exit(1);
	}

	unsigned int sample_rate = 44100;
	unsigned int channel_count = 2;

	modplayer_set_stereo(modplayer, true); // this is the default.
	modplayer_set_sample_rate(modplayer, sample_rate); // default is 48000.
	modplayer_set_stereo_width(modplayer, 0.5f); // reduce stereo effect a bit

	float total_time = 30.0f; // record first few seconds of song
	unsigned int total_frames = total_time * sample_rate;

	char filename[32];
	snprintf(filename, 32, "%s.wav", modplayer->mod->name);
	FILE* fp = fopen(filename, "wb");
	if(fp == NULL)
	{
		printf("Failed to open file for writing %s\n", filename);
		exit(1);
	}

	printf("Writing %s\n", filename);

	int frame_size = channel_count * sizeof(short);	
	int data_size = total_frames * frame_size;

	// write wav file header
	write_tag("RIFF", fp);
	write_int32(36 + data_size, fp);
	write_tag("WAVE", fp);
	write_tag("fmt ", fp);
	write_int32(16, fp); // block size
	write_int16(1, fp);	// linear pcm
	write_int16(channel_count, fp); // channel count (mono or stereo)
	write_int32(sample_rate, fp); // sample rate
	write_int32(sample_rate * channel_count * 2, fp); // byte rate: samplerate * numchannels * bytesperchannel
	write_int16(channel_count * 2, fp); // block align: numchannels * bytesperchannel
	write_int16(16, fp); // bits per sample

	write_tag("data", fp);
	write_int32(data_size, fp);

	unsigned int buffer_size_in_frames = 4096;
	short* buffer = (short*)malloc(sizeof(short) * channel_count * buffer_size_in_frames);

	unsigned int frames_remaining = total_frames;
	while(frames_remaining > 0)
	{
		unsigned int num_frames = frames_remaining < buffer_size_in_frames ? frames_remaining : buffer_size_in_frames;

		// decode the frames into our buffer
		modplayer_decode_frames(modplayer, num_frames, buffer);

		// save frames to file
		fwrite(buffer, num_frames, frame_size, fp);
		
		frames_remaining -= num_frames;
	}

	fclose(fp);
	free(buffer);

	// don't forget to free the mod-player
	modplayer_free(modplayer);

	return 0;
}