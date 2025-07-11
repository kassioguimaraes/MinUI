#ifndef __msettings_h__
#define __msettings_h__

void InitSettings(void);
void QuitSettings(void);

int GetBrightness(void);
int GetVolume(void);

void SetRawBrightness(int value); // 0-1024
void SetRawVolume(int value); // 0-40

void SetBrightness(int value); // 0-10

int GetColorTemp(void) //0-40;
void SetColorTemp(int value) //0-40;

int GetContrast(void) // 0-10;
void SetContrast(int value) //0-10;

int GetEnhanceMode(void)// 0 or 1
int SetEnhanceMode(int value) // 0 or 1

int GetSaturation(void) // 0-10
int SetSaturation(int value) // 0-10

void SetVolume(int value); // 0-20

int GetJack(void);
void SetJack(int value); // 0-1

int GetHDMI(void);
void SetHDMI(int value); // 0-1


int GetMute(void);
void SetMute(int value); // 0-1

#endif  // __msettings_h__
