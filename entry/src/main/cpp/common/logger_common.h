/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LOGGER_COMMON_H
#define LOGGER_COMMON_H

#include <cstdio>
#include <cstdarg>

// Global debug logging switch.
// When false (default): only LOGE prints.
// When true: LOGI, LOGD, LOGW also print.
extern bool g_agenuiDebugEnabled;

// Performance profiling switch.
// When true: PERF_LOG outputs timing data (LOGE level so always visible).
// When false (default): PERF_LOG is a no-op.
extern bool g_perfEnabled;

// Platform-specific logging
#if defined(AGENUI_PLATFORM_HARMONYOS) || defined(__OHOS__)
    // HarmonyOS logging
    // Uses snprintf to pre-format the message, then passes it as a single
    // %{public}s to OH_LOG_Print. This guarantees all arguments are visible
    // (no privacy filtering) without needing %{public} in every call site.
    #include <hilog/log.h>
    #define APP_LOG_DOMAIN 0x0001
    #define APP_LOG_TAG "Sample_ndkvulkan"

    #define LOGE(...) do { \
        char _agenui_buf[1024]; \
        snprintf(_agenui_buf, sizeof(_agenui_buf), __VA_ARGS__); \
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "%{public}s", _agenui_buf); \
    } while(0)

    #define LOGW(...) do { \
        if (g_agenuiDebugEnabled) { \
            char _agenui_buf[1024]; \
            snprintf(_agenui_buf, sizeof(_agenui_buf), __VA_ARGS__); \
            OH_LOG_Print(LOG_APP, LOG_WARN, APP_LOG_DOMAIN, APP_LOG_TAG, "%{public}s", _agenui_buf); \
        } \
    } while(0)

    #define LOGI(...) do { \
        if (g_agenuiDebugEnabled) { \
            char _agenui_buf[1024]; \
            snprintf(_agenui_buf, sizeof(_agenui_buf), __VA_ARGS__); \
            OH_LOG_Print(LOG_APP, LOG_INFO, APP_LOG_DOMAIN, APP_LOG_TAG, "%{public}s", _agenui_buf); \
        } \
    } while(0)

    #define LOGD(...) do { \
        if (g_agenuiDebugEnabled) { \
            char _agenui_buf[1024]; \
            snprintf(_agenui_buf, sizeof(_agenui_buf), __VA_ARGS__); \
            OH_LOG_Print(LOG_APP, LOG_DEBUG, APP_LOG_DOMAIN, APP_LOG_TAG, "%{public}s", _agenui_buf); \
        } \
    } while(0)

#elif defined(AGENUI_PLATFORM_WINDOWS) || defined(_WIN32) || defined(_WIN64)
    // Windows logging - use standard output
    #define APP_LOG_TAG "Sample_ndkvulkan"

    #define LOGE(...) do { printf("[ERROR] [%s] ", APP_LOG_TAG); printf(__VA_ARGS__); printf("\n"); } while(0)
    #define LOGW(...) do { if (g_agenuiDebugEnabled) { printf("[WARN] [%s] ", APP_LOG_TAG); printf(__VA_ARGS__); printf("\n"); } } while(0)
    #define LOGI(...) do { if (g_agenuiDebugEnabled) { printf("[INFO] [%s] ", APP_LOG_TAG); printf(__VA_ARGS__); printf("\n"); } } while(0)
    #define LOGD(...) do { if (g_agenuiDebugEnabled) { printf("[DEBUG] [%s] ", APP_LOG_TAG); printf(__VA_ARGS__); printf("\n"); } } while(0)
#else
    // Default logging
    #define APP_LOG_TAG "Sample_ndkvulkan"

    #define LOGE(...) do { printf("[ERROR] [%s] ", APP_LOG_TAG); printf(__VA_ARGS__); printf("\n"); } while(0)
    #define LOGW(...) do { if (g_agenuiDebugEnabled) { printf("[WARN] [%s] ", APP_LOG_TAG); printf(__VA_ARGS__); printf("\n"); } } while(0)
    #define LOGI(...) do { if (g_agenuiDebugEnabled) { printf("[INFO] [%s] ", APP_LOG_TAG); printf(__VA_ARGS__); printf("\n"); } } while(0)
    #define LOGD(...) do { if (g_agenuiDebugEnabled) { printf("[DEBUG] [%s] ", APP_LOG_TAG); printf(__VA_ARGS__); printf("\n"); } } while(0)
#endif

// Performance profiling macro. Uses LOGE level (always visible).
// Controlled by g_perfEnabled (default: false).
#define PERF_LOG(...) do { if (g_perfEnabled) { LOGE(__VA_ARGS__); } } while(0)

#endif // LOGGER_COMMON_H
