/*
 * Power Manager Header
 */

#ifndef PM_MANAGER_H
#define PM_MANAGER_H

#include <stdbool.h>

void pm_init(void);
void pm_suspend(void);
void pm_resume(void);
bool pm_is_suspended(void);

#endif
