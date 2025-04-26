/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/logging.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <atomic>
#include <memory>

#include "QseeWrapper.hpp"

struct QSEECom_handle;

namespace tee_wrapper {
typedef int (*QSEECom_start_app_t)(QSEECom_handle**, const char*, const char*, uint32_t);
typedef int (*QSEECom_shutdown_app_t)(QSEECom_handle**);
typedef int (*QSEECom_load_external_elf_t)(QSEECom_handle**, const char*, const char*);
typedef int (*QSEECom_unload_external_elf_t)(QSEECom_handle**);
typedef int (*QSEECom_register_listener_t)(QSEECom_handle**, uint32_t, uint32_t, uint32_t);
typedef int (*QSEECom_unregister_listener_t)(QSEECom_handle*);
typedef int (*QSEECom_send_cmd_t)(QSEECom_handle*, void*, uint32_t, void*, uint32_t);
typedef int (*QSEECom_send_modified_cmd_t)(QSEECom_handle*, void*, uint32_t, void*, uint32_t,
                                           QSEEComIonFdInfo*);
typedef int (*QSEECom_receive_req_t)(QSEECom_handle*, void*, uint32_t);
typedef int (*QSEECom_send_resp_t)(QSEECom_handle*, void*, uint32_t);
typedef int (*QSEECom_set_bandwidth_t)(QSEECom_handle*, bool);
typedef int (*QSEECom_app_load_query_t)(QSEECom_handle*, char*);
typedef int (*QSEECom_send_service_cmd_t)(void*, uint32_t, void*, uint32_t, QSEEComCommandId);
typedef int (*QSEECom_create_key_t)(KeyManagementUsageType, unsigned char*);
typedef int (*QSEECom_wipe_key_t)(KeyManagementUsageType);
typedef int (*QSEECom_clear_key_t)(KeyManagementUsageType);
typedef int (*QSEECom_update_key_user_info_t)(KeyManagementUsageType, unsigned char*,
                                              unsigned char*);
typedef int (*QSEECom_send_modified_resp_t)(QSEECom_handle*, void*, uint32_t, QSEEComIonFdInfo*);
typedef int (*QSEECom_scale_bus_bandwidth_t)(QSEECom_handle*, int);
typedef int (*QSEECom_get_app_info_t)(QSEECom_handle*, QSEEComAppInfo*);
typedef int (*QSEECom_send_modified_cmd_64_t)(QSEECom_handle*, void*, uint32_t, void*, uint32_t,
                                              QSEEComIonFdInfo*);
typedef int (*QSEECom_send_modified_resp_64_t)(QSEECom_handle*, void*, uint32_t, QSEEComIonFdInfo*);
typedef int (*QSEECom_start_app_V2_t)(QSEECom_handle**, const char*, unsigned char*, uint32_t,
                                      uint32_t);

static void* lib_handle = nullptr;
static std::atomic_flag initialized = ATOMIC_FLAG_INIT;

static QSEECom_start_app_t pLibFunc_start_app = nullptr;
static QSEECom_shutdown_app_t pLibFunc_shutdown_app = nullptr;
static QSEECom_load_external_elf_t pLibFunc_load_external_elf = nullptr;
static QSEECom_unload_external_elf_t pLibFunc_unload_external_elf = nullptr;
static QSEECom_register_listener_t pLibFunc_register_listener = nullptr;
static QSEECom_unregister_listener_t pLibFunc_unregister_listener = nullptr;
static QSEECom_send_cmd_t pLibFunc_send_cmd = nullptr;
static QSEECom_send_modified_cmd_t pLibFunc_send_modified_cmd = nullptr;
static QSEECom_receive_req_t pLibFunc_receive_req = nullptr;
static QSEECom_send_resp_t pLibFunc_send_resp = nullptr;
static QSEECom_set_bandwidth_t pLibFunc_set_bandwidth = nullptr;
static QSEECom_app_load_query_t pLibFunc_app_load_query = nullptr;
static QSEECom_send_service_cmd_t pLibFunc_send_service_cmd = nullptr;
static QSEECom_create_key_t pLibFunc_create_key = nullptr;
static QSEECom_wipe_key_t pLibFunc_wipe_key = nullptr;
static QSEECom_clear_key_t pLibFunc_clear_key = nullptr;
static QSEECom_update_key_user_info_t pLibFunc_update_key_user_info = nullptr;
static QSEECom_send_modified_resp_t pLibFunc_send_modified_resp = nullptr;
static QSEECom_scale_bus_bandwidth_t pLibFunc_scale_bus_bandwidth = nullptr;
static QSEECom_get_app_info_t pLibFunc_get_app_info = nullptr;
static QSEECom_send_modified_cmd_64_t pLibFunc_send_modified_cmd_64 = nullptr;
static QSEECom_send_modified_resp_64_t pLibFunc_send_modified_resp_64 = nullptr;
static QSEECom_start_app_V2_t pLibFunc_start_app_V2 = nullptr;

#define TEE_WRAPPER_LOAD_SYM(handle, func_ptr, symbol_name)                                \
    do {                                                                                   \
        func_ptr = reinterpret_cast<typeof(func_ptr)>(dlsym(handle, #symbol_name));        \
        if (!func_ptr) {                                                                   \
            LOG(ERROR) << "Failed to load symbol: " << #symbol_name << " - " << dlerror(); \
            dlclose(handle);                                                               \
            lib_handle = nullptr;                                                          \
            return -1;                                                                     \
        }                                                                                  \
        LOG(VERBOSE) << "Loaded symbol: " << #symbol_name;                                 \
    } while (0)

#define TEE_WRAPPER_CHECK_INIT(func_name)                                             \
    do {                                                                              \
        if (!initialized.test(std::memory_order_acquire)) {                           \
            LOG(ERROR) << #func_name << " called before successful initialization!";  \
            return -1;                                                                \
        }                                                                             \
        if (!lib_handle) {                                                            \
            LOG(ERROR) << #func_name                                                  \
                       << " called after failed initialization or deinitialization!"; \
            return -1;                                                                \
        }                                                                             \
    } while (0)

#define TEE_WRAPPER_CHECK_FUNC(func_ptr, func_name)                             \
    do {                                                                        \
        if (!func_ptr) {                                                        \
            LOG(ERROR) << "Function pointer for " << #func_name << " is NULL!"; \
            return -1;                                                          \
        }                                                                       \
    } while (0)

int initialize() {
    if (initialized.test_and_set(std::memory_order_acquire)) {
        LOG(INFO) << "Already initialized.";
        return 0;
    }

    LOG(INFO) << "Initializing TeeWrapper...";

    const char* target_lib = "libQSEEComAPI.so";
    lib_handle = dlopen(target_lib, RTLD_LAZY | RTLD_LOCAL);
    if (!lib_handle) {
        LOG(ERROR) << "Failed to dlopen " << target_lib << ": " << dlerror();
        initialized.clear(std::memory_order_release);
        return -1;
    }
    LOG(INFO) << "Successfully dlopened " << target_lib;

    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_start_app, QSEECom_start_app);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_shutdown_app, QSEECom_shutdown_app);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_load_external_elf, QSEECom_load_external_elf);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_unload_external_elf, QSEECom_unload_external_elf);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_register_listener, QSEECom_register_listener);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_unregister_listener, QSEECom_unregister_listener);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_send_cmd, QSEECom_send_cmd);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_send_modified_cmd, QSEECom_send_modified_cmd);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_receive_req, QSEECom_receive_req);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_send_resp, QSEECom_send_resp);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_set_bandwidth, QSEECom_set_bandwidth);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_app_load_query, QSEECom_app_load_query);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_send_service_cmd, QSEECom_send_service_cmd);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_create_key, QSEECom_create_key);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_wipe_key, QSEECom_wipe_key);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_clear_key, QSEECom_clear_key);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_update_key_user_info, QSEECom_update_key_user_info);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_send_modified_resp, QSEECom_send_modified_resp);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_scale_bus_bandwidth, QSEECom_scale_bus_bandwidth);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_get_app_info, QSEECom_get_app_info);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_send_modified_cmd_64, QSEECom_send_modified_cmd_64);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_send_modified_resp_64, QSEECom_send_modified_resp_64);
    TEE_WRAPPER_LOAD_SYM(lib_handle, pLibFunc_start_app_V2, QSEECom_start_app_V2);

    LOG(INFO) << "TeeWrapper initialized successfully.";
    return 0;
}

void deinitialize() {
    if (!initialized.test_and_set(std::memory_order_acquire)) {
        LOG(INFO) << "Not initialized, nothing to deinitialize.";
        initialized.clear(std::memory_order_release);
        return;
    }

    LOG(INFO) << "Deinitializing TeeWrapper...";
    if (lib_handle) {
        dlclose(lib_handle);
        lib_handle = nullptr;
        LOG(INFO) << "Unloaded TEE communication library.";
    }

    pLibFunc_start_app = nullptr;
    pLibFunc_shutdown_app = nullptr;
    pLibFunc_load_external_elf = nullptr;
    pLibFunc_unload_external_elf = nullptr;
    pLibFunc_register_listener = nullptr;
    pLibFunc_unregister_listener = nullptr;
    pLibFunc_send_cmd = nullptr;
    pLibFunc_send_modified_cmd = nullptr;
    pLibFunc_receive_req = nullptr;
    pLibFunc_send_resp = nullptr;
    pLibFunc_set_bandwidth = nullptr;
    pLibFunc_app_load_query = nullptr;
    pLibFunc_send_service_cmd = nullptr;
    pLibFunc_create_key = nullptr;
    pLibFunc_wipe_key = nullptr;
    pLibFunc_clear_key = nullptr;
    pLibFunc_update_key_user_info = nullptr;
    pLibFunc_send_modified_resp = nullptr;
    pLibFunc_scale_bus_bandwidth = nullptr;
    pLibFunc_get_app_info = nullptr;
    pLibFunc_send_modified_cmd_64 = nullptr;
    pLibFunc_send_modified_resp_64 = nullptr;
    pLibFunc_start_app_V2 = nullptr;

    initialized.clear(std::memory_order_release);
    LOG(INFO) << "TeeWrapper deinitialized.";
}

int qseecomStartApp(QSEECom_handle** clnt_handle, const char* path, const char* fname,
                    uint32_t sb_size) {
    TEE_WRAPPER_CHECK_INIT(qseecomStartApp);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_start_app, QSEECom_start_app);
    return pLibFunc_start_app(clnt_handle, path, fname, sb_size);
}

int qseecomShutdownApp(QSEECom_handle** handle) {
    TEE_WRAPPER_CHECK_INIT(qseecomShutdownApp);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_shutdown_app, QSEECom_shutdown_app);
    return pLibFunc_shutdown_app(handle);
}

int qseecomLoadExternalElf(QSEECom_handle** clnt_handle, const char* path, const char* fname) {
    TEE_WRAPPER_CHECK_INIT(qseecomLoadExternalElf);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_load_external_elf, QSEECom_load_external_elf);
    return pLibFunc_load_external_elf(clnt_handle, path, fname);
}

int qseecomUnloadExternalElf(QSEECom_handle** handle) {
    TEE_WRAPPER_CHECK_INIT(qseecomUnloadExternalElf);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_unload_external_elf, QSEECom_unload_external_elf);
    return pLibFunc_unload_external_elf(handle);
}

int qseecomRegisterListener(QSEECom_handle** handle, uint32_t lstnr_id, uint32_t sb_length,
                            uint32_t flags) {
    TEE_WRAPPER_CHECK_INIT(qseecomRegisterListener);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_register_listener, QSEECom_register_listener);
    return pLibFunc_register_listener(handle, lstnr_id, sb_length, flags);
}

int qseecomUnregisterListener(QSEECom_handle* handle) {
    TEE_WRAPPER_CHECK_INIT(qseecomUnregisterListener);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_unregister_listener, QSEECom_unregister_listener);
    return pLibFunc_unregister_listener(handle);
}

int qseecomSendCommand(QSEECom_handle* handle, void* send_buf, uint32_t sbuf_len, void* rcv_buf,
                       uint32_t rbuf_len) {
    TEE_WRAPPER_CHECK_INIT(qseecomSendCommand);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_send_cmd, QSEECom_send_cmd);
    return pLibFunc_send_cmd(handle, send_buf, sbuf_len, rcv_buf, rbuf_len);
}

int qseecomSendModifiedCommand(QSEECom_handle* handle, void* send_buf, uint32_t sbuf_len,
                               void* resp_buf, uint32_t rbuf_len, QSEEComIonFdInfo* ifd_data) {
    TEE_WRAPPER_CHECK_INIT(qseecomSendModifiedCommand);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_send_modified_cmd, QSEECom_send_modified_cmd);
    return pLibFunc_send_modified_cmd(handle, send_buf, sbuf_len, resp_buf, rbuf_len, ifd_data);
}

int qseecomReceiveRequest(QSEECom_handle* handle, void* buf, uint32_t len) {
    TEE_WRAPPER_CHECK_INIT(qseecomReceiveRequest);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_receive_req, QSEECom_receive_req);
    return pLibFunc_receive_req(handle, buf, len);
}

int qseecomSendResponse(QSEECom_handle* handle, void* send_buf, uint32_t len) {
    TEE_WRAPPER_CHECK_INIT(qseecomSendResponse);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_send_resp, QSEECom_send_resp);
    return pLibFunc_send_resp(handle, send_buf, len);
}

int qseecomSetBandwidth(QSEECom_handle* handle, bool high) {
    TEE_WRAPPER_CHECK_INIT(qseecomSetBandwidth);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_set_bandwidth, QSEECom_set_bandwidth);
    return pLibFunc_set_bandwidth(handle, high);
}

int qseecomAppLoadQuery(QSEECom_handle* handle, char* app_name) {
    TEE_WRAPPER_CHECK_INIT(qseecomAppLoadQuery);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_app_load_query, QSEECom_app_load_query);
    return pLibFunc_app_load_query(handle, app_name);
}

int qseecomSendServiceCommand(void* send_buf, uint32_t sbuf_len, void* resp_buf, uint32_t rbuf_len,
                              QSEEComCommandId cmd_id) {
    TEE_WRAPPER_CHECK_INIT(qseecomSendServiceCommand);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_send_service_cmd, QSEECom_send_service_cmd);
    return pLibFunc_send_service_cmd(send_buf, sbuf_len, resp_buf, rbuf_len, cmd_id);
}

int qseecomCreateKey(KeyManagementUsageType usage, unsigned char* hash32) {
    TEE_WRAPPER_CHECK_INIT(qseecomCreateKey);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_create_key, QSEECom_create_key);
    return pLibFunc_create_key(usage, hash32);
}

int qseecomWipeKey(KeyManagementUsageType usage) {
    TEE_WRAPPER_CHECK_INIT(qseecomWipeKey);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_wipe_key, QSEECom_wipe_key);
    return pLibFunc_wipe_key(usage);
}

int qseecomClearKey(KeyManagementUsageType usage) {
    TEE_WRAPPER_CHECK_INIT(qseecomClearKey);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_clear_key, QSEECom_clear_key);
    return pLibFunc_clear_key(usage);
}

int qseecomUpdateKeyUserInfo(KeyManagementUsageType usage, unsigned char* current_hash32,
                             unsigned char* new_hash32) {
    TEE_WRAPPER_CHECK_INIT(qseecomUpdateKeyUserInfo);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_update_key_user_info, QSEECom_update_key_user_info);
    return pLibFunc_update_key_user_info(usage, current_hash32, new_hash32);
}

int qseecomSendModifiedResponse(QSEECom_handle* handle, void* send_buf, uint32_t sbuf_len,
                                QSEEComIonFdInfo* ifd) {
    TEE_WRAPPER_CHECK_INIT(qseecomSendModifiedResponse);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_send_modified_resp, QSEECom_send_modified_resp);
    return pLibFunc_send_modified_resp(handle, send_buf, sbuf_len, ifd);
}

int qseecomScaleBusBandwidth(QSEECom_handle* handle, int mode) {
    TEE_WRAPPER_CHECK_INIT(qseecomScaleBusBandwidth);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_scale_bus_bandwidth, QSEECom_scale_bus_bandwidth);
    return pLibFunc_scale_bus_bandwidth(handle, mode);
}

int qseecomGetAppInfo(QSEECom_handle* handle, QSEEComAppInfo* info) {
    TEE_WRAPPER_CHECK_INIT(qseecomGetAppInfo);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_get_app_info, QSEECom_get_app_info);
    return pLibFunc_get_app_info(handle, info);
}

int qseecomSendModifiedCommand64(QSEECom_handle* handle, void* send_buf, uint32_t sbuf_len,
                                 void* resp_buf, uint32_t rbuf_len, QSEEComIonFdInfo* ifd_data) {
    TEE_WRAPPER_CHECK_INIT(qseecomSendModifiedCommand64);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_send_modified_cmd_64, QSEECom_send_modified_cmd_64);
    return pLibFunc_send_modified_cmd_64(handle, send_buf, sbuf_len, resp_buf, rbuf_len, ifd_data);
}

int qseecomSendModifiedResponse64(QSEECom_handle* handle, void* send_buf, uint32_t sbuf_len,
                                  QSEEComIonFdInfo* ifd) {
    TEE_WRAPPER_CHECK_INIT(qseecomSendModifiedResponse64);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_send_modified_resp_64, QSEECom_send_modified_resp_64);
    return pLibFunc_send_modified_resp_64(handle, send_buf, sbuf_len, ifd);
}

int qseecomStartAppV2(QSEECom_handle** clnt_handle, const char* fname, unsigned char* trustlet,
                      uint32_t tlen, uint32_t sb_length) {
    TEE_WRAPPER_CHECK_INIT(qseecomStartAppV2);
    TEE_WRAPPER_CHECK_FUNC(pLibFunc_start_app_V2, QSEECom_start_app_V2);
    return pLibFunc_start_app_V2(clnt_handle, fname, trustlet, tlen, sb_length);
}

}  // namespace tee_wrapper
