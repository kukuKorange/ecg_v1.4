/**
  ******************************************************************************
  * @file    userinfo.h
  * @brief   User profile (name + age) management
  ******************************************************************************
  */

#ifndef __USERINFO_H
#define __USERINFO_H

#include <stdint.h>

#define USER_NAME_MAX   8

typedef struct {
    char    name[USER_NAME_MAX + 1];   /* null-terminated ASCII */
    uint8_t age;
} UserInfo_t;

extern UserInfo_t user_info;

void UserInfo_Init(void);
void UserInfo_Load(void);
void UserInfo_Save(void);

#endif /* __USERINFO_H */
