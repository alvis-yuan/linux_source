#ifndef __AUDIO_CLIENT_H__
#define __AUDIO_CLIENT_H__

#include <audio.h>
void GenFixedAudio(int id);
void AudioPlayFixed(int id, short repeats, short delay);
void AudioPlayFile(const char *fname, short repeats, short delay);
void AudioPlayText(const char *text, short repeats, short delay);
void AudioSetVolume(int vol, int play);
void AudioVolumeAdd(int play);
void AudioVolumeSub(int play);
void AudioSetTTSParam(const char *tts_param);
void AudioResUpdate(void);
void AudioDelListNode(short id);
uint8_t need_paly_fixed(int id);
audio_cfg_t *get_audio_cfg_mmap();
#endif
