#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#define __init __attribute__((constructor))
#define __exit __attribute__((destructor))
#define __unused __attribute__((unused))
#define __packed __attribute__((__packed__))
#define __aligned(x) __attribute__((__aligned__(x)))
#define __deprecated __attribute__((__deprecated__))
#define __printf(a, b) __attribute__ ((format (printf, a, b)))
#define __fallthrough __attribute__((__fallthrough__))




#endif /* COMMON_UTILS_H */