#pragma once

#include "../stdint.h"

#define MFD_CLOEXEC       0x0001U
#define MFD_ALLOW_SEALING 0x0002U
#define MFD_HUGETLB       0x0004U
#define MFD_NOEXEC_SEAL   0x0008U
#define MFD_EXEC          0x0010U

#ifdef __cplusplus
extern "C" {
#endif

int memfd_create(const char *name, unsigned int flags);

#ifdef __cplusplus
}
#endif
