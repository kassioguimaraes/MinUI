#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <string.h>

#include "msettings.h"

///////////////////////////////////////
#define DEFAULT_COLORTEMPERATURE 29 // 0-40, 0 = -100, 40 = +100, factory is +45 (29)
#define SETTINGS_VERSION 2
typedef struct Settings {
	int version; // future proofing
	int brightness;
	int enhancemode; // 0 or 1
	int contrast;
	int colortemperature;
	int saturation;
	int headphones;
	int speaker;
	int unused[2]; // for future use
	// NOTE: doesn't really need to be persisted but still needs to be shared
	int jack; 
	int hdmi; 
} Settings;
static Settings DefaultSettings = {
	.version = SETTINGS_VERSION,
	.brightness = 2,
	.enhancemode = 1,
	.colortemperature = DEFAULT_COLORTEMPERATURE,
	.contrast = 0, // 0 to 10
	.saturation = 0, // 0 to 10
	.headphones = 4,
	.speaker = 8,
	.jack = 0,
	.hdmi = 0,
};
static Settings* settings;

#define SHM_KEY "/SharedSettings"
static char SettingsPath[256];
static int shm_fd = -1;
static int is_host = 0;
static int shm_size = sizeof(Settings);

#define JACK_STATE_PATH "/sys/module/snd_soc_sunxi_component_jack/parameters/jack_state" // TODO: doesn't change, always 0
#define HDMI_STATE_PATH "/sys/class/switch/hdmi/cable.0/state"

int getInt(char* path) {
	int i = 0;
	FILE *file = fopen(path, "r");
	if (file!=NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}

void InitSettings(void) {	
	sprintf(SettingsPath, "%s/msettings.bin", getenv("USERDATA_PATH"));
	
	shm_fd = shm_open(SHM_KEY, O_RDWR | O_CREAT | O_EXCL, 0644); // see if it exists
	if (shm_fd==-1 && errno==EEXIST) { // already exists
		puts("Settings client");
		shm_fd = shm_open(SHM_KEY, O_RDWR, 0644);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	}
	else { // host
		puts("Settings host"); // should always be keymon
		is_host = 1;
		// we created it so set initial size and populate
		ftruncate(shm_fd, shm_size);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
		
		int fd = open(SettingsPath, O_RDONLY);
		if (fd>=0) {
			read(fd, settings, shm_size);
			// TODO: use settings->version for future proofing?
			close(fd);
		}
		else {
			// load defaults
			memcpy(settings, &DefaultSettings, shm_size);
		}
		
		// these shouldn't be persisted
		// settings->jack = 0;
		// settings->hdmi = 0;
	}
	
	int jack = getInt(JACK_STATE_PATH);
	int hdmi = getInt(HDMI_STATE_PATH);
	printf("brightness: %i (hdmi: %i)\nspeaker: %i (jack: %i)\n", settings->brightness, hdmi, settings->speaker, jack); fflush(stdout);
	
	// both of these set volume
	SetJack(jack);
	SetHDMI(hdmi);
	
	//screen related settings
	SetBrightness(GetBrightness());
	SetContrast(settings->contrast);
	SetColortemp(settings->colortemperature);
	SetSaturation(settings->saturation);
	// system("echo $(< " BRIGHTNESS_PATH ")");
}
void QuitSettings(void) {
	munmap(settings, shm_size);
	if (is_host) shm_unlink(SHM_KEY);
}
static inline void SaveSettings(void) {
	int fd = open(SettingsPath, O_CREAT|O_WRONLY, 0644);
	if (fd>=0) {
		write(fd, settings, shm_size);
		close(fd);
		sync();
	}
}

int GetBrightness(void) { // 0-10
	return settings->brightness;
}
void SetBrightness(int value) {
	if (settings->hdmi) return;
	
	int raw;
	switch (value) {
		case  0: raw=  4; break;	//  0
		case  1: raw=  6; break;	//  2
		case  2: raw= 10; break;	//  4
		case  3: raw= 16; break;	//  6
		case  4: raw= 32; break;	// 16
		case  5: raw= 48; break;	// 16
		case  6: raw= 64; break;	// 16
		case  7: raw= 96; break;	// 32
		case  8: raw=128; break;	// 32
		case  9: raw=192; break;	// 64
		case 10: raw=255; break;	// 64
	}
	
	SetRawBrightness(raw);
	settings->brightness = value;
	SaveSettings();
}

int GetVolume(void) { // 0-20
	return settings->jack ? settings->headphones : settings->speaker;
}
void SetVolume(int value) {
	if (settings->hdmi) return;
	
	if (settings->jack) settings->headphones = value;
	else settings->speaker = value;
	
	int raw = value * 5;
	SetRawVolume(raw);
	SaveSettings();
}

#define DISP_LCD_SET_BRIGHTNESS  0x102
void SetRawBrightness(int val) { // 0 - 255
	if (settings->hdmi) return;
	
	printf("SetRawBrightness(%i)\n", val); fflush(stdout);
    int fd = open("/dev/disp", O_RDWR);
	if (fd) {
	    unsigned long param[4]={0,val,0,0};
		ioctl(fd, DISP_LCD_SET_BRIGHTNESS, &param);
		close(fd);
	}
}
void SetRawVolume(int val) { // 0 - 100
	printf("SetRawVolume(%i)\n", val); fflush(stdout);
	char cmd[256];
	sprintf(cmd, "amixer sset 'lineout volume' %i%% > /dev/null 2>&1", val);
	// // puts(cmd); fflush(stdout);
	system(cmd);
}

// monitored and set by thread in keymon
int GetJack(void) {
	return settings->jack;
}

void SetJack(int value) {
	// printf("SetJack(%i)\n", value); fflush(stdout);
	
	// char cmd[256];
	// sprintf(cmd, "amixer cset name='Playback Path' '%s' &> /dev/null", value?"HP":"SPK");
	// system(cmd);
	
	settings->jack = value;
	SetVolume(GetVolume());
}


void SetContrast(int value) {
	if (settings->hdmi) return;
	if (value < 0) value = 0;
	if (value > 10) value = 10;
	
	printf("SetContrast(%i) -> %i\n", value, raw); fflush(stdout);
	
	FILE *fd = fopen("/sys/class/disp/disp/attr/enhance_contrast", "w");
	if (fd) {
		fprintf(fd, "%i", raw);
		fclose(fd);
	}
	settings->contrast = value;
	SaveSettings();
}

void SetRawColortemp(int val) { // 0 - 255
	 if (settings->hdmi) return;
	
	printf("SetRawColortemp(%i)\n", val); fflush(stdout);

	FILE *fd = fopen("/sys/class/disp/disp/attr/color_temperature", "w");
	if (fd) {
		fprintf(fd, "%i", val);
		fclose(fd);
	}
}

int ScaleColortemp(int settings_value) {
    if (settings_value < 0) settings_value = 0;
    if (settings_value > 20) settings_value = 20;
    return (settings_value * 10) - 100;
}

int GetColortemp(void) { // 0-10
	return settings->colortemperature;
}

void SetColortemp(int value) {
	SetRawColortemp(ScaleColortemp(value));
	settings->colortemperature = value;
	SaveSettings();
}

int GetSaturation(void) {
	return settings->saturation;
}

void SetSaturation(int value) {
	if (value < 0) value = 0;
	if (value > 10) value = 10;
	
	printf("setSaturation(%i)\n", value); fflush(stdout);
	
	FILE *fd = fopen("/sys/class/disp/disp/attr/enhance_saturation", "w");
	if (fd) {
		fprintf(fd, "%i", value);
		fclose(fd);
	}
	settings->saturation = value;
	SaveSettings();
}

int GetHDMI(void) {	
	// printf("GetHDMI() %i\n", settings->hdmi); fflush(stdout);
	return settings->hdmi;
}
void SetHDMI(int value) {
	// printf("SetHDMI(%i)\n", value); fflush(stdout);
	
	settings->hdmi = value;
	if (value) SetRawVolume(100); // max
	else SetVolume(GetVolume()); // restore
}

int GetMute(void) { return 0; }
void SetMute(int value) {}
