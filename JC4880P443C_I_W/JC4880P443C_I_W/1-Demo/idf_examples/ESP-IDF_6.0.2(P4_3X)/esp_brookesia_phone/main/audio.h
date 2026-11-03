#ifndef __AUDIO_H__
#define __AUDIO_H__ 

#ifdef __cplusplus
extern "C" {
#endif

// 前向声明NewApp类，以便在C函数中使用
#ifdef __cplusplus
class NewApp;
#endif

void i2s_music(void *args);
void i2s_echo(void *args);

#ifdef __cplusplus
}
#endif

#endif