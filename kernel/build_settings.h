/*
 * 
 *  XJ380 编译设置头文件
 *  Copyright(C) XINGJI Interactive Software 2017-2026 All rights reserved.
 *
 */

/*

    这是一个冰箱，用来防止代码隔夜变质导致出现玄学bug的情况

*/

/* 

⣿⣿⣿⠟⠛⠛⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡟⢋⣩⣉⢻⣿⣿
⣿⣿⣿⠀⣿⣶⣕⣈⠹⠿⠿⠿⠿⠟⠛⣛⢋⣰⠣⣿⣿⠀⣿⣿
⣿⣿⣿⡀⣿⣿⣿⣧⢻⣿⣶⣷⣿⣿⣿⣿⣿⣿⠿⠶⡝⠀⣿⣿
⣿⣿⣿⣷⠘⣿⣿⣿⢏⣿⣿⣋⣀⣈⣻⣿⣿⣷⣤⣤⣿⡐⢿⣿
⣿⣿⣿⣿⣆⢩⣝⣫⣾⣿⣿⣿⣿⡟⠿⠿⠦⠀⠸⠿⣻⣿⡄⢻
⣿⣿⣿⣿⣿⡄⢻⣿⣿⣿⣿⣿⣿⣿⣿⣶⣶⣾⣿⣿⣿⣿⠇⣼
⣿⣿⣿⣿⣿⣿⡄⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡟⣰⣿
⣿⣿⣿⣿⣿⣿⠇⣼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢀⣿⣿
⣿⣿⣿⣿⣿⠏⢰⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢸⣿⣿
⣿⣿⣿⣿⠟⣰⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⣿⣿
⣿⣿⣿⠋⣴⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡄⣿⣿
⣿⣿⠋⣼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⢸⣿
             POPO猫
          用于辟邪的小猫
 
*/

#pragma once

#ifndef _BUILD_SETTINGS_H_
#define _BUILD_SETTINGS_H_

#if defined(__has_include)
#if __has_include(<generated/autoconf.h>)
#include <generated/autoconf.h>
#endif
#endif

// 编译版本类型
#define DEBUG_VERSION 0
#define RELEASE_VERSION 1

#ifndef CONFIG_OS_VERSION
#define CONFIG_OS_VERSION "BetaVersion"
#endif

#ifndef CONFIG_KN_VERSION
#define CONFIG_KN_VERSION "XSK 2.1.0"
#endif

#ifndef CONFIG_OS_EDITION
#define CONFIG_OS_EDITION "XINGJI XJ380 Singularity 1.0.0"
#endif

#ifndef CONFIG_STACK_SIZE
#define CONFIG_STACK_SIZE 262144
#endif

#if defined(CONFIG_BUILD_EDITION_RELEASE)
#define BUILD_EDITION RELEASE_VERSION
#else
#define BUILD_EDITION DEBUG_VERSION
#endif

#define OS_VERSION CONFIG_OS_VERSION
#define KN_VERSION CONFIG_KN_VERSION
#define OS_EDITION CONFIG_OS_EDITION

#define ENVP_SYSTEM_VERSION "SYSTEM_VERSION=" CONFIG_OS_EDITION

#define STACK_SIZE (CONFIG_STACK_SIZE * 1UL)

#endif
