#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*IkonikaTranslationStatusCallback)(const char* Source, const char* Target, int32_t Status);
typedef void (*IkonikaTranslationDownloadCallback)(const char* Source, const char* Target, bool bSuccess,
                                                   const char* Error);
typedef void (*IkonikaTranslationResultCallback)(uint64_t Generation, const char* Text, const char* Result);

void IkonikaTranslationInstall(IkonikaTranslationStatusCallback OnStatus,
                               IkonikaTranslationDownloadCallback OnDownload,
                               IkonikaTranslationResultCallback OnResult);
void IkonikaTranslationCheck(const char* Source, const char* Target);
void IkonikaTranslationDownload(const char* Source, const char* Target);
void IkonikaTranslationTranslate(uint64_t Generation, const char* Text, const char* Source, const char* Target);

#ifdef __cplusplus
}
#endif
