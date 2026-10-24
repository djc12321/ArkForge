#ifndef ARKFORGE_GIT_PROCESS_PROTOCOL_H
#define ARKFORGE_GIT_PROCESS_PROTOCOL_H
#include <stdint.h>
#define ARKFORGE_GIT_MAGIC 0x41464750u
#define ARKFORGE_GIT_MAX_MESSAGE (1024u*1024u)
struct ArkForgeGitRequest { uint32_t magic,kind,argc,envc,bytes,descriptors; };
#endif
