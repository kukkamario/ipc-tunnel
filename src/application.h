#ifndef APPLICATION_H_
#define APPLICATION_H_
#include <stdbool.h>

bool APPLICATION_Init(void);

void APPLICATION_T0(void);
void APPLICATION_T1(void);
void APPLICATION_T2(void);

void APPLICATION_BG(void);

bool APPLICATION_Running(void);

#endif  // APPLICATION_H_
