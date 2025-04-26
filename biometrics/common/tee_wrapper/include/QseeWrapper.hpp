/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdbool>
#include <cstdint>

struct QSEECom_handle;

namespace tee_wrapper {

struct QSEEComIonFdData {
    int32_t fd;
    uint32_t cmd_buf_offset;
};

struct QSEEComIonFdInfo {
    QSEEComIonFdData data[4];
};

enum class QSEEComCommandId : uint32_t {
    RPMB_RESERVED = 0x0E,
    RPMB_PROVISION_KEY = 0x0F,
    RPMB_ERASE = 0x10,
    RPMB_CHECK_PROV_STATUS = 0x1B,
    RPMB_MAX = 0xEFFFFFFF
};

enum class KeyManagementUsageType : uint32_t {
    KM_USAGE_DISK_ENCRYPTION = 0x01,
    KM_USAGE_FILE_ENCRYPTION = 0x02,
    KM_USAGE_UFS_ICE_DISK_ENCRYPTION = 0x03,
    KM_USAGE_SDCC_ICE_DISK_ENCRYPTION = 0x04,
    KM_USAGE_MAX
};

struct QSEEComAppInfo {
    bool is_secure_app_64bit;
    uint32_t required_sg_buffer_size;
    uint8_t reserved[64];
};

int initialize();
void deinitialize();

int qseecomStartApp(QSEECom_handle** clnt_handle, const char* path, const char* fname,
                    uint32_t sb_size);

int qseecomShutdownApp(QSEECom_handle** handle);

int qseecomLoadExternalElf(QSEECom_handle** clnt_handle, const char* path, const char* fname);

int qseecomUnloadExternalElf(QSEECom_handle** handle);

int qseecomRegisterListener(QSEECom_handle** handle, uint32_t lstnr_id, uint32_t sb_length,
                            uint32_t flags);

int qseecomUnregisterListener(QSEECom_handle* handle);

int qseecomSendCommand(QSEECom_handle* handle, void* send_buf, uint32_t sbuf_len, void* rcv_buf,
                       uint32_t rbuf_len);

int qseecomSendModifiedCommand(QSEECom_handle* handle, void* send_buf, uint32_t sbuf_len,
                               void* resp_buf, uint32_t rbuf_len, QSEEComIonFdInfo* ifd_data);

int qseecomReceiveRequest(QSEECom_handle* handle, void* buf, uint32_t len);

int qseecomSendResponse(QSEECom_handle* handle, void* send_buf, uint32_t len);

int qseecomSetBandwidth(QSEECom_handle* handle, bool high);

int qseecomAppLoadQuery(QSEECom_handle* handle, char* app_name);

int qseecomSendServiceCommand(void* send_buf, uint32_t sbuf_len, void* resp_buf, uint32_t rbuf_len,
                              QSEEComCommandId cmd_id);

int qseecomCreateKey(KeyManagementUsageType usage, unsigned char* hash32);

int qseecomWipeKey(KeyManagementUsageType usage);

int qseecomClearKey(KeyManagementUsageType usage);

int qseecomUpdateKeyUserInfo(KeyManagementUsageType usage, unsigned char* current_hash32,
                             unsigned char* new_hash32);

int qseecomSendModifiedResponse(QSEECom_handle* handle, void* send_buf, uint32_t sbuf_len,
                                QSEEComIonFdInfo* ifd);

int qseecomScaleBusBandwidth(QSEECom_handle* handle, int mode);

int qseecomGetAppInfo(QSEECom_handle* handle, QSEEComAppInfo* info);

int qseecomSendModifiedCommand64(QSEECom_handle* handle, void* send_buf, uint32_t sbuf_len,
                                 void* resp_buf, uint32_t rbuf_len, QSEEComIonFdInfo* ifd_data);

int qseecomSendModifiedResponse64(QSEECom_handle* handle, void* send_buf, uint32_t sbuf_len,
                                  QSEEComIonFdInfo* ifd);

int qseecomStartAppV2(QSEECom_handle** clnt_handle, const char* fname, unsigned char* trustlet,
                      uint32_t tlen, uint32_t sb_length);

}  // namespace tee_wrapper
