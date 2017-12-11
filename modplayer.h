/*
	modplayer.h - a single header library for playing protracker mod files

	LICENSE
	See end of file for license information.

	Do this:
		#define MOD_PLAYER_IMPLEMENTATION
	before you include this file in *one* C or C++ file to create the implementation.

	Usage example:

		mp_mod_player* modplayer = modplayer_create_from_file("somemod.mod");

		// or if you have the mod in memory already
		mp_mod_player* modplayer = modplayer_create_from_buffer(pointer_to_mod, mod_length_in_bytes)

		if(modplayer == NULL)
			.oh dear.

		modplayer->set_sample_rate(modplayer, 48000); // this is the default
		modplayer->set_stereo(true); // this is also the default

		// create a buffer for 6 seconds of stereo audio at 48kHz
		short* buffer = (short*)malloc(48000 * 2 * 6 * sizeof(short);
		while(more_frames_needed)
		{
			modplayer_decode_frames(modplayer, num_frames, buffer);
			... do something with the buffer ...
		}

		...

		modplayer_free(modplayer);


	Revision History:
	v1.0	initial release
*/

#ifndef MOD_PLAYER_H
#define MOD_PLAYER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mp_mod_player mp_mod_player;

// load a mod file and initialise a mp_mod_player struct. The return value should be free'd with modplayer_free()
mp_mod_player* modplayer_create_from_file(char* filename);
// load a mod from memory and initialise a mp_mod_player struct. The return value should be free'd with modplayer_free()
mp_mod_player* modplayer_create_from_buffer(unsigned char* buf, unsigned int buflen);
// free a previously created mp_mod_player struct
void modplayer_free(mp_mod_player* modplayer);

// set the output sample rate. default is 48000
void modplayer_set_sample_rate(mp_mod_player* modplayer, unsigned int sample_rate);
// set the number of channels to output. default is 2 channels (i.e. stereo)
void modplayer_set_stereo(mp_mod_player* modplayer, bool is_stereo);
// optionally reduce the stereo width.
// on the Amiga channels 1&4 were panned hard left, and 2&3 were panned hard right
// this might be too wide, so you can reduce it by passing a value <1 to this function
// the default is 1.0 (hard panning), 0.0 would result in a mono output (both channels the same)
void modplayer_set_stereo_width(mp_mod_player* modplayer, float stereo_width);

// reset the song to the start
void modplayer_reset_song_to_beginning(mp_mod_player* modplayer);
// decode a given number of frames and write them to the given buffer.
// the buffer should be large enough to contain frame_count*2 samples (if stereo), or frame_count samples (if mono).
// the frames are output as interleaved (left,right) signed 16-bit integers.
void modplayer_decode_frames(mp_mod_player* modplayer, unsigned int frame_count, short* buffer);
// as above, but output the samples as 32-bit float values instead of 16bit.
void modplayer_decode_frames_f(mp_mod_player* modplayer, unsigned int frame_count, float* buffer);

#ifdef __cplusplus
}
#endif

#endif // MOD_PLAYER_H

#ifdef MOD_PLAYER_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct mp_pattern mp_pattern;
typedef struct mp_line mp_line;
typedef struct mp_channel_note mp_channel_note;
typedef struct mp_channel_state mp_channel_state;
typedef struct mp_sample mp_sample;
typedef struct mp_mod mp_mod;

struct mp_sample
{
	int length;
	int repeat_offset;
	int repeat_length;
	char fine_tune;
	unsigned char loop;
	unsigned char volume;
	char name[23];
	float* sample_data;
};

struct mp_channel_note
{
	unsigned short period;
	unsigned char sample;
	unsigned char effect_type;
	unsigned char effect_param;
};

struct mp_line
{
	mp_channel_note channels[4];
};

struct mp_pattern
{
	mp_line lines[64];
};

struct mp_channel_state
{
	unsigned short period;
	unsigned char sample;
	unsigned char volume;
	unsigned char sample_looped;

	unsigned char vol_slide_active;
	unsigned char pitch_slide_active;
	unsigned char vibrato_active;
	unsigned char tremolo_active;
	unsigned char arpeggio_active;

	char vol_slide;
	short pitch_slide;
	unsigned char vib_rate;
	unsigned char vib_depth;
	unsigned char vib_phase;
	char vol_offset;
	char arpeggio1;
	char arpeggio2;
	char retrigger_rate;
	unsigned char note_cut_idx;

	unsigned char loop_start;
	unsigned char loop_count;

	float pitch_offset; // in semi-tones. used for vibrato and arpeggio effects
	unsigned short target_period; // target period for slide-to-note effect

	float sample_pos;
	float panning; // -1 hard left, +1 hard right
};

struct mp_mod
{
	char* name;
	int song_length;
	int num_samples;
	int num_patterns;
	int num_channels;
	mp_sample* samples;
	mp_pattern* patterns;
	unsigned char pattern_table[128];
};

struct mp_mod_player
{
	// settings
	// output sample rate in hz. default is 48000
	unsigned int output_sample_rate;
	// number of channels to mix to.
	// 1=mono, 2=stereo. (other values not currently supported)
	// default is stereo.
	unsigned int output_channel_count;
	// by default channels 1&4 are mixed hard left and channels 2,3 are mixed hard right
	// use this to reduce the stereo width if you prefer
	// default is 1.0 (= hard panning). 0.0 = mono
	// only useful if output_channel_count = 2
	float stereo_width;

	// mod to play
	mp_mod* mod;

	// data relating to current play position etc
	int pattern_idx;
	int line_idx;
	int tick_idx;
	int frames_until_next_tick;

	int speed; // ticks per line
	int bpm;

	bool do_position_jump; // if true, do a position jump after the current line
	int position_jump_pat_idx;
	int position_jump_line_idx;

	int pattern_delay; // used for pattern-delay effect (EE)

	mp_channel_state* channel_state;
	float* mix_buffer;
	float* final_buffer;
};

enum EffectType
{
	Effect_Arpeggio 		= 0x0,
	Effect_SlideUp			= 0x1,
	Effect_SlideDown		= 0x2,
	Effect_SlideToNote		= 0x3,
	Effect_Vibrato			= 0x4,
	Effect_VolSlide_Port	= 0x5,
	Effect_VolSlide_Vib		= 0x6,
	Effect_Tremolo			= 0x7,
	Effect_SetPan			= 0x8,
	Effect_SetSampleOffset	= 0x9,
	Effect_VolSlide			= 0xA,
	Effect_PositionJump		= 0xB,
	Effect_SetVolume		= 0xC,
	Effect_PatternBreak		= 0xD,
	Effect_Extended			= 0xE,
	Effect_SetSpeed			= 0xF
};

enum ExtendedEffectType
{
	ExtEffect_SetFilter			= 0x0,
	ExtEffect_FineSlideUp		= 0x1,
	ExtEffect_FineSlideDown		= 0x2,
	ExtEffect_Glissando			= 0x3,
	ExtEffect_SetVibWave		= 0x4,
	ExtEffect_SetFineTune		= 0x5,
	ExtEffect_SetJumpLoop		= 0x6,
	ExtEffect_SetTremWave		= 0x7,
	ExtEffect_SetCoursePan		= 0x8,
	ExtEffect_RetriggerNote		= 0x9,
	ExtEffect_FineVolSlideUp	= 0xA,
	ExtEffect_FineVolSlideDown	= 0xB,
	ExtEffect_NoteCut			= 0xC,
	ExtEffect_NoteDelay			= 0xD,
	ExtEffect_PatternDelay		= 0xE,
	ExtEffect_InvertLoop		= 0xF
};

#define mp_min(a,b) ((a) < (b) ? (a) : (b))
#define mp_max(a,b) ((a) > (b) ? (a) : (b))
#define mp_clamp(x, a,b)  ((x) < (a) ? (a) : (x) > (b) ? (b) : (x))

#ifndef M_PI
#define M_PI 3.14159265f
#endif
#define TAU 6.28318531f

static inline float mp_sin(float x)
{
	// wrap input angle to -pi..pi
	if(x > M_PI)
	{
		x = x * (1.0f / TAU);
		x = x - (int)x; // get fractional part of x
		x *= TAU;
		x = x > M_PI ? x - TAU : x;
	}
	else if (x < -M_PI)
	{
		x = -x * (1.0f / TAU);
		x = x - (int)x;
		x *= -TAU;
		x = x < -M_PI ? x + TAU : x;
	}

	// parametric sin approximation (not especially accurate, but good enough for vibrato)
	if (x < 0.0f)
		return 1.27323954f * x + 0.405284735f * x * x;
	else
		return 1.27323954f * x - 0.405284735f * x * x;
}

static inline float mp_pow2(float x)
{
	// cubic approximation for 2^x
	// only valid for x values from -1 .. 1
	x = mp_clamp(x, -1.0f, 1.0f);
	return 0.9988f + x*(0.6927f + x*(0.2503f + x*0.0572f));
}

static inline unsigned char lower_nibble(unsigned char c)
{
	return c & 0xf;
}

static inline unsigned char upper_nibble(unsigned char c)
{
	return c >> 4;
}

static inline int read_short_big_endian(unsigned char* data)
{
	return (data[0] << 8) | data[1];
}

static void read_sample(mp_sample* sam, unsigned char* data)
{
	memcpy(sam->name, &data[0], 22);
	sam->name[22] = '\0';
	sam->length = read_short_big_endian(&data[22]) * 2; // length is given in words (2 bytes), but samples are 1 byte
	sam->fine_tune = ((char)(data[24] << 4)) >> 4; // signed nibble -8..7
	sam->volume = data[25];
	sam->repeat_offset = read_short_big_endian(&data[26]) * 2;
	sam->repeat_length = read_short_big_endian(&data[28]) * 2;
	sam->loop = sam->repeat_length > 2 ? 1 : 0;
}

static void read_pattern(mp_pattern* pat, unsigned char* data)
{
	for(int i=0; i<64; ++i)
	{
		mp_line* line = &pat->lines[i];
		for(int c=0; c<4; ++c)
		{
			mp_channel_note* note = &line->channels[c];
			note->sample = (data[0] & 0xf0) | ((data[2] & 0xf0) >> 4);
			note->period = ((data[0] & 0x0f) << 8) | (data[1]);
			note->effect_type = (data[2] & 0x0f);
			note->effect_param = data[3];

			data += 4;
		}
	}
}

static mp_mod_player* modplayer_create_player(mp_mod* mod)
{
	mp_mod_player* modplayer = (mp_mod_player*)malloc(sizeof(mp_mod_player));
	modplayer->output_sample_rate = 48000;
	modplayer->output_channel_count = 2;
	modplayer->stereo_width = 1.0f;
	modplayer->mod = mod;

	modplayer->pattern_idx = 0;
	modplayer->line_idx = 0;
	modplayer->tick_idx = 0;
	modplayer->frames_until_next_tick = 0;

	modplayer->speed = 6;
	modplayer->bpm = 125;

	modplayer->do_position_jump = false;
	modplayer->pattern_delay = 0;

	int num_channels = mod->num_channels;
	modplayer->channel_state = (mp_channel_state*)malloc(sizeof(mp_channel_state) * num_channels);
	modplayer->mix_buffer = (float*)malloc(sizeof(float) * 1024);
	modplayer->final_buffer = (float*)malloc(sizeof(float) * 1024 * num_channels);
	for(int i=0; i<num_channels; ++i)
	{
		mp_channel_state* state = &modplayer->channel_state[i];
		memset(state, 0x00, sizeof(mp_channel_state));
		// set default panning, channels 1,4 left, channels 2,3 right
		state->panning = (((i+1) & 0x2) == 0) ? -1.0f : 1.0f;	
	}

	modplayer_reset_song_to_beginning(modplayer);

	return modplayer;
}

static void execute_extended_effect(mp_mod_player* modplayer, mp_channel_note* note, mp_channel_state* state)
{
	unsigned char effect_val = note->effect_param;
	unsigned char effect_x  = upper_nibble(effect_val);
	unsigned char effect_y = lower_nibble(effect_val);

	switch(effect_x)
	{
		case ExtEffect_SetFilter:
		//	printf("Set Filter not implemented\n");
			break;
		case ExtEffect_FineSlideUp:
			state->period -= effect_y;
			break;
		case ExtEffect_FineSlideDown:
			state->period += effect_y;
			break;
		case ExtEffect_Glissando:
		//	printf("Glissando not implemented\n");
			break;
		case ExtEffect_SetVibWave:
		//	printf("Set Vib Wave\n");
			break;
		case ExtEffect_SetFineTune:
		//	printf("Set Fine Tune\n");
			break;
		case ExtEffect_SetJumpLoop:
			if(effect_y == 0)
				state->loop_start = modplayer->line_idx;
			else
			{
				// is this the first time we've encountered this loop?
				if(state->loop_count == 0)
					state->loop_count = effect_y;
				else
					state->loop_count--;

				if(state->loop_count > 0)
				{
					modplayer->position_jump_line_idx = state->loop_start;
					modplayer->position_jump_pat_idx = modplayer->pattern_idx;
					modplayer->do_position_jump = true;
				}
			}
			break;
		case ExtEffect_SetTremWave:
		//	printf("Set Trem Wave not implemented\n");
			break;
		case ExtEffect_SetCoursePan:
		//	printf("Set Course Pan not implemented\n");
			break;
		case ExtEffect_RetriggerNote:
			state->retrigger_rate = effect_y;
			break;
		case ExtEffect_FineVolSlideUp:
			state->volume = mp_min(state->volume + effect_y, 64);
			break;
		case ExtEffect_FineVolSlideDown:
			state->volume = state->volume > effect_y ? state->volume - effect_y : 0;
			break;
		case ExtEffect_NoteCut:
			if(effect_y == 0)
				state->volume = 0;
			else
				state->note_cut_idx = effect_y;
			break;
		case ExtEffect_NoteDelay:
		//	printf("Note Delay not implemented\n");
			break;
		case ExtEffect_PatternDelay:
			modplayer->pattern_delay = effect_y * modplayer->speed;
			break;
		case ExtEffect_InvertLoop:
		//	printf("Invert Loop not implmeneted\n");
			break;
		default:
			// huh ??
			break;
	}
}

static void execute_effect(mp_mod_player* modplayer, mp_channel_note* note, mp_channel_state* state)
{
	unsigned char effect_val = note->effect_param;
	unsigned char effect_x  = upper_nibble(effect_val);
	unsigned char effect_y = lower_nibble(effect_val);

	switch(note->effect_type)
	{
		case Effect_Arpeggio:
			if(effect_val != 0)
			{
				state->arpeggio_active = 1;
				state->arpeggio1 = effect_x;
				state->arpeggio2 = effect_y;
			}
			break;
		case Effect_SlideUp:
			state->pitch_slide_active = 1;
			state->pitch_slide = -effect_val;
			state->target_period = 0;
			break;
		case Effect_SlideDown:
			state->pitch_slide_active = 1;
			state->pitch_slide = effect_val;
			state->target_period = 0;
			break;
		case Effect_SlideToNote:
			state->pitch_slide_active = 1;
			if(note->period != 0)
				state->target_period = note->period;
			if(effect_val != 0)
				state->pitch_slide = state->target_period > state->period ? effect_val : -effect_val;
			break;
		case Effect_Vibrato:
			state->vibrato_active = 1;
			if(effect_x != 0)
				state->vib_rate = effect_x;
			if(effect_y != 0)
				state->vib_depth = effect_y;
			break;
		case Effect_Tremolo:
			// not tested. need an example mod
			state->tremolo_active = 1;
			if(effect_x != 0)
				state->vib_rate = effect_x;
			if(effect_y != 0)
				state->vib_depth = effect_y * (modplayer->speed - 1);
			break;
		case Effect_SetPan:
		//	printf("Set Panning not implemented\n");
			break;
		case Effect_SetSampleOffset:
			if(effect_val > 0)
				state->sample_pos = 256.0f * effect_val;
			break;
		case Effect_VolSlide:
		case Effect_VolSlide_Port:
		case Effect_VolSlide_Vib:
			state->vol_slide_active = 1;
			if(effect_x != 0)
				state->vol_slide = effect_x;
			else 
				state->vol_slide = -effect_y;
			break;
		case Effect_PositionJump:
			if(!modplayer->do_position_jump) // don't overwrite line info from a pattern-break command on the same line
				modplayer->position_jump_line_idx = 0;
			modplayer->position_jump_pat_idx = effect_val;
			modplayer->do_position_jump = true;
			break;
		case Effect_SetVolume:
			state->volume = effect_val;
			break;
		case Effect_PatternBreak:
			if(!modplayer->do_position_jump) // don't overwrite pattern info from a pos-jump command on the same line
				modplayer->position_jump_pat_idx = modplayer->pattern_idx + 1;
			modplayer->position_jump_line_idx = effect_x * 10 + effect_y;
			modplayer->do_position_jump = true;
			break;
		case Effect_Extended:
			execute_extended_effect(modplayer, note, state);
			break;
		case Effect_SetSpeed:
			{
				int spd = mp_max(1, effect_val);
				if(spd <= 32)	// set ticks per line
					modplayer->speed = spd;
				else			// set bpm
					modplayer->bpm = spd;
			}
			break;
		default:
			// huh??
			break;
	}
}

static void execute_line(mp_mod_player* modplayer)
{
	mp_mod* mod = modplayer->mod;

	int pattern_idx = mod->pattern_table[modplayer->pattern_idx];
	mp_pattern* pattern = &mod->patterns[pattern_idx];
	mp_line* line = &pattern->lines[modplayer->line_idx];

	int num_channels = mod->num_channels;
	for(int i=0; i<num_channels; ++i)
	{
		mp_channel_note* note = &line->channels[i];
		mp_channel_state* state = &modplayer->channel_state[i];

		// effects are active only for the line they appear on.
		state->vol_slide_active = 0;
		state->tremolo_active = 0;
		state->arpeggio_active = 0;
		state->vol_offset = 0;
		state->retrigger_rate = 0;
		state->note_cut_idx = 0;
		if(note->effect_type != Effect_VolSlide_Port)	
			state->pitch_slide_active = 0;
		if(note->effect_type != Effect_VolSlide_Vib)
		{
			state->vibrato_active = 0;
			state->pitch_offset = 0.0f;
		}
			
		if((note->period != 0 || note->sample != 0) &&
			 note->effect_type != Effect_SlideToNote)
		{
			// trigger new note
			if(note->period != 0)
				state->period = note->period;
			if(note->sample != 0)
				state->sample = note->sample;
			state->sample_pos = 0.0f;
			state->sample_looped = 0;
			state->volume = mod->samples[state->sample].volume;

			if(	note->effect_type != Effect_Vibrato && 
				note->effect_type != Effect_Tremolo && 	
				note->effect_type != Effect_VolSlide_Vib )
			{
				state->vib_phase = 0; // reset vibrato/trem wave
			}
		}

		execute_effect(modplayer, note, state);
	}

	float seconds_per_tick = 1.0f / (0.4f * modplayer->bpm);
	modplayer->frames_until_next_tick = (int)(modplayer->output_sample_rate * seconds_per_tick);
}

static void execute_tick(mp_mod_player* modplayer)
{
	mp_mod* mod = modplayer->mod;

	// handle currently playing effects
	int num_channels = mod->num_channels;
	for(int i=0; i<num_channels; ++i)
	{
		mp_channel_state* state = &modplayer->channel_state[i];
		if(state->vol_slide_active != 0)
		{
			int new_vol = state->volume + state->vol_slide;
			new_vol = mp_clamp(new_vol, 0, 64);
			state->volume = new_vol;
		}
		
		if(state->pitch_slide_active != 0)
		{
			int new_period = state->period + state->pitch_slide;
			if(state->target_period != 0)
			{
				if(state->pitch_slide > 0)
					new_period = mp_min(state->target_period, new_period);
				else
					new_period = mp_max(state->target_period, new_period);
			}
			new_period = mp_clamp(new_period, 20, 20000); // should lookup what min and max period values should be really!
			state->period = new_period;
		}

		if(state->arpeggio_active != 0)
		{
			int tick_idx = modplayer->tick_idx % 3;
			state->pitch_offset = tick_idx == 0 ? 0.0f : tick_idx == 1 ? state->arpeggio1 : state->arpeggio2;
		}

		if(state->vibrato_active != 0 || state->tremolo_active != 0)
		{
			state->vib_phase++;
			float osc_per_tick = state->vib_rate * (1.0f / 64.0f);
			float wave = mp_sin(state->vib_phase * osc_per_tick * 2.0f * M_PI);

			if(state->vibrato_active != 0)
				state->pitch_offset = wave * state->vib_depth * (1.0f / 16.0f);
			else
				state->vol_offset = (char)(wave * state->vib_depth);
		}

		if(state->retrigger_rate > 0)
		{
			if(modplayer->tick_idx % state->retrigger_rate == 0)
				state->sample_pos = 0.0f;
		}

		if(state->note_cut_idx != 0 && state->note_cut_idx == modplayer->tick_idx)
			state->volume = 0;
	}

	float seconds_per_tick = 1.0f / (0.4f * modplayer->bpm);
	modplayer->frames_until_next_tick = (int)(modplayer->output_sample_rate * seconds_per_tick);
}

static void output_channel(mp_mod_player* modplayer, mp_channel_state* state, unsigned int num_frames, float* buffer)
{
	int min_valid_period = 20; // this is to stop badly formed mods from playing sounds when they shouldn't (e.g. setting a sample but no period, then doing a pitch slide. some mods do it...)
	if(state->sample > 0 && state->period > min_valid_period)
	{
		mp_sample* sample = &modplayer->mod->samples[state->sample];
		float sample_pos = state->sample_pos;
		// magic formula for converting from period to sample rate: 
		// rate in hz = Amiga chip freq / 2*period
		float sample_rate = 7159090.5f / (state->period * 2.0f);
		if(state->pitch_offset != 0.0f || sample->fine_tune != 0)
		{
			float semitones = state->pitch_offset + (sample->fine_tune * (1.0f / 8.0f));
			sample_rate *= mp_pow2(semitones * (1.0f / 12.0f));
		}
		
		float sample_step = sample_rate / modplayer->output_sample_rate;
		for(unsigned int i=0; i<num_frames; ++i)
		{
			int sample_end = state->sample_looped > 0 ? sample->repeat_offset + sample->repeat_length : sample->length;
		
			if(sample_pos < sample_end)
			{
				int idx = (int)sample_pos;
				float t = sample_pos - idx;
				// interpolate between adjacent samples
				float s0 = sample->sample_data[idx];
				float s1 = sample->sample_data[mp_min(idx + 1, sample_end-1)];
				float sample_val = s0 + t * (s1 - s0);
				unsigned char volume = state->volume + state->vol_offset;
				volume = mp_min(volume, 64);
				sample_val *= (volume * (1.0f / 64.0f));
									
				buffer[i] = sample_val;
				sample_pos += sample_step;

				// handle sample loop
				if(sample_pos >= sample_end && sample->loop > 0)
				{
					float over = sample_pos - sample_end;
					sample_pos = sample->repeat_offset + over;
					state->sample_looped = 1;
				}
			}
			else
			{
				buffer[i] = 0.0f;
			}			
		}

		state->sample_pos = sample_pos;
	}
	else
	{
		for(unsigned int i=0; i<num_frames; ++i)
			buffer[i] = 0.0f;
	}
}

static void mix_buffer(mp_mod_player* modplayer, float* channel_buffer, float* out_buffer, unsigned int num_frames, float panning)
{
	float channel_gain = modplayer->output_channel_count / (float)modplayer->mod->num_channels;

	if(modplayer->output_channel_count == 1)
	{
		// mono output
		for(unsigned int i=0; i<num_frames; ++i)
		{
			out_buffer[i] += channel_gain * channel_buffer[i];
		}
	}
	else if(modplayer->output_channel_count == 2)
	{
		// simple linear panning
		panning = mp_clamp(panning * modplayer->stereo_width, -1.0f, 1.0f);
		float left_gain = channel_gain * (0.5f + 0.5f * -panning);
		float right_gain = channel_gain * (0.5f + 0.5f * panning);
		
		for(unsigned int i=0; i<num_frames; ++i)
		{
			out_buffer[i*2+0] += left_gain * channel_buffer[i];
			out_buffer[i*2+1] += right_gain * channel_buffer[i];
		}
	}
}

static void output_frames(mp_mod_player* modplayer, unsigned int num_frames, float* buffer)
{
	mp_mod* mod = modplayer->mod;
	
	unsigned int num_channels = mod->num_channels;
	unsigned int out_channels = modplayer->output_channel_count;
	memset(buffer, 0x00, num_frames * out_channels * sizeof(float));
	
	for(unsigned int i=0; i<num_channels; ++i)
	{
		mp_channel_state* state = &modplayer->channel_state[i];

		memset(modplayer->mix_buffer, 0x00, num_frames * sizeof(float));
		
		output_channel(modplayer, state, num_frames, modplayer->mix_buffer);
		mix_buffer(modplayer, modplayer->mix_buffer, buffer, num_frames, state->panning);
	}				
}

////////////// Public Interface ////////////////

mp_mod_player* modplayer_create_from_file(char* filename)
{
	FILE* fp = fopen(filename, "rb");

	if(fp == NULL)
	{
		fprintf(stderr, "Error opening mod file %s\n", filename);
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	size_t len = ftell(fp);
	rewind(fp);

	unsigned char* buf = (unsigned char*)malloc(len);
	fread(buf, len, 1, fp);
	fclose(fp);

	mp_mod_player* modplayer = modplayer_create_from_buffer(buf, (unsigned int)len);
	free(buf);
	return modplayer;
}

mp_mod_player* modplayer_create_from_buffer(unsigned char* buf, unsigned int buflen)
{
	if(buflen < 2048)
	{
		fprintf(stderr, "This doesn't look like a mod file: too short\n");
		return NULL;
	}

	mp_mod* mod = (mp_mod*)malloc(sizeof(mp_mod));
	mod->name = (char*)malloc(21 * sizeof(char));

	memcpy(mod->name, buf, 20);
	mod->name[21] = '\0';

	mod->num_channels = 4; // until we support xm

	int num_samples = 32;
	mod->num_samples = num_samples;
	mod->samples = (mp_sample*)malloc(num_samples * sizeof(mp_sample));
	memset(mod->samples, 0x00, num_samples * sizeof(mp_sample));
	unsigned int sample_data_size = 0;
	unsigned char* sample_def_data = &buf[20];
	// samples are numbered from 1. sample 0 is always blank
	for(int i=1; i<num_samples; ++i)
	{
		read_sample(&mod->samples[i], sample_def_data);
		sample_def_data += 30;
		sample_data_size += mod->samples[i].length;
	}

	unsigned char* song_data = sample_def_data;
	mod->song_length = song_data[0];
	memcpy(mod->pattern_table, &song_data[2], 128);

	int num_patterns = 0;
	for(int i=0; i<mod->song_length; ++i)
	{
		int patternIdx = mod->pattern_table[i] + 1;
		num_patterns = mp_max(patternIdx, num_patterns);
	}

	mod->num_patterns = num_patterns;
			
	char mk[5];
	memcpy(mk, &song_data[130], 4);
	mk[4] = '\0';

	unsigned int expected_file_size = 1082 + 1024*num_patterns + sample_data_size;
	if(buflen < expected_file_size)
	{
		fprintf(stderr, "Error reading mod, file may be corrupted or not a protracker mod\n");
		return NULL;
	}

	unsigned char* pattern_data = &song_data[134];

	// read patterns
	mod->patterns = (mp_pattern*)malloc(sizeof(mp_pattern) * num_patterns);
	for(int i=0; i<num_patterns; ++i)
	{
		read_pattern(&mod->patterns[i], &pattern_data[1024 * i]);
	}

	char* sample_data = (char*)&pattern_data[1024 * num_patterns];
	for(int i=0; i<num_samples; ++i)
	{
		mp_sample* sample = &mod->samples[i];
		if(sample->length > 0)
		{
			int num_frames = sample->length;
			sample->sample_data = (float*)malloc(num_frames * sizeof(float));
			for(int f=0; f<num_frames; ++f)
				sample->sample_data[f] = (1.0f / 128.0f) * sample_data[f];
		}
		else
		{
			sample->sample_data = NULL;
		}
	
		sample_data += sample->length;
	}

	return modplayer_create_player(mod);
}

void modplayer_free(mp_mod_player* modplayer)
{
	if(modplayer == NULL)
		return;

	mp_mod* mod = modplayer->mod;
	if(mod != NULL)
	{
		for(int i=0; i<mod->num_samples; ++i)
		{
			mp_sample* sample = &mod->samples[i];
			if(sample->sample_data != NULL)
				free(sample->sample_data);
		}
		free(mod->name);
		free(mod->samples);
		free(mod->patterns);
		free(mod);
	}

	free(modplayer->channel_state);
	free(modplayer->mix_buffer);
	free(modplayer->final_buffer);
	free(modplayer);
}

void modplayer_set_sample_rate(mp_mod_player* modplayer, unsigned int sample_rate)
{
	modplayer->output_sample_rate = sample_rate;
}

void modplayer_set_stereo(mp_mod_player* modplayer, bool is_stereo)
{
	modplayer->output_channel_count = is_stereo ? 2 : 1;
}

void modplayer_set_stereo_width(mp_mod_player* modplayer, float stereo_width)
{
	modplayer->stereo_width = stereo_width;
}

void modplayer_reset_song_to_beginning(mp_mod_player* modplayer)
{
	modplayer->pattern_idx = 0;
	modplayer->line_idx = 0;
	modplayer->tick_idx = 0;

	execute_line(modplayer);
}

void modplayer_decode_frames_f(mp_mod_player* modplayer, unsigned int frame_count, float* buffer)
{
	unsigned int frames_remaining = frame_count;
	float* out_buf = buffer;
	while(frames_remaining > 0)
	{
		int num_frames = mp_min(frames_remaining, 1024);
		num_frames = mp_min(modplayer->frames_until_next_tick, num_frames);

		output_frames(modplayer, num_frames, out_buf);

		out_buf += num_frames * modplayer->output_channel_count;
		modplayer->frames_until_next_tick -= num_frames;
		frames_remaining -= num_frames;

		if(modplayer->frames_until_next_tick == 0)
		{
			modplayer->tick_idx++;
			if(modplayer->tick_idx == (modplayer->speed + modplayer->pattern_delay))
			{
				modplayer->tick_idx = 0;
				modplayer->pattern_delay = 0;
				modplayer->line_idx++;

				if(modplayer->do_position_jump || modplayer->line_idx >= 64) //modplayer->mod->patterns[modplayer->mod->pattern_table[modplayer->pattern_idx]].length)
				{
					int old_pattern_idx = modplayer->pattern_idx;

					if(modplayer->do_position_jump)
					{
						modplayer->line_idx = modplayer->position_jump_line_idx;
						modplayer->pattern_idx = modplayer->position_jump_pat_idx;
						modplayer->do_position_jump = false;
					}
					else
					{
						modplayer->line_idx = 0;
						modplayer->pattern_idx++;
					}

					if(modplayer->pattern_idx >= modplayer->mod->song_length)
					{
						// end of song;
						modplayer->pattern_idx = 0; // loop
					}

					if(modplayer->pattern_idx != old_pattern_idx)
					{
						// new pattern, so reset the loop points
						for(int i=0; i<modplayer->mod->num_channels; ++i)
						{
							modplayer->channel_state[i].loop_start = 0;
							modplayer->channel_state[i].loop_count = 0;
						}
					}
				}
					
				execute_line(modplayer);				
			}
			else
			{
				execute_tick(modplayer);
			}
		}
	}
}

void modplayer_decode_frames(mp_mod_player *modplayer, unsigned int frame_count, short *buffer)
{
	unsigned int frames_remaining = frame_count;
	short* out_buf = buffer;

	while(frames_remaining > 0)
	{
		int num_frames = mp_min(frames_remaining, 1024);
		modplayer_decode_frames_f(modplayer, num_frames, modplayer->final_buffer);

		for(unsigned int i=0; i<num_frames * modplayer->output_channel_count; ++i)
			out_buf[i] = (short)(modplayer->final_buffer[i] * 32767.0f);

		frames_remaining -= num_frames;
		out_buf += num_frames * modplayer->output_channel_count;
	}
}

#endif // MOD_PLAYER_IMPLEMENTATION

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Andy Gill
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
