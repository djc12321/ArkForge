#pragma once
/* The SDK omits strverscmp; link the unchanged upstream musl implementation. */
#ifndef __ASSEMBLER__
int strverscmp(const char *,const char *);
#endif
