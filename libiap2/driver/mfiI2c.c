/*
 *  File: mfiI2c.c
 *  Package: MFIDriver
 *  Abstract: MFi Authentication Chip I2C Driver Implementation
 */

#include "mfiI2c.h"
#include "iAP2Log.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <sys/types.h>

/*
****************************************************************
**
**  Constants and Definitions
**
****************************************************************
*/

#define MFI_DEVICE_NAME                     "/dev/i2c-0"
#define MFI_I2C_ADDRESS                     0x10
#define MFI_MAX_RETRY_COUNT                 255
#define MFI_RETRY_DELAY_US                  5000        /* 5ms */
#define MFI_CHALLENGE_DATA_LENGTH_MAX       32
#define MFI_ACCESSORY_CERT_PAGE_LENGTH_MAX  128
#define MFI_AUTH_TIMEOUT_MS                 2000        /* 2 seconds */
#define MFI_AUTH_POLL_INTERVAL_MS           200

/* MFi Chip Register Addresses */
typedef enum {
    kMFIRegDeviceVersion                    = 0x00,
    kMFIRegAuthRevision                     = 0x01,
    kMFIRegAuthMajorVersion                 = 0x02,
    kMFIRegAuthMinorVersion                 = 0x03,
    kMFIRegDeviceId                         = 0x04,
    kMFIRegErrorCode                        = 0x05,
    kMFIRegControlStatus                    = 0x10,
    kMFIRegChallengeResponseDataLength      = 0x11,
    kMFIRegChallengeResponseData            = 0x12,
    kMFIRegChallengeDataLength              = 0x20,
    kMFIRegChallengeData                    = 0x21,
    kMFIRegAccessoryCertificateDataLength   = 0x30,
    kMFIRegAccessoryCertificateData1        = 0x31,
    kMFIRegAccessoryCertificateData2        = 0x32,
    kMFIRegAccessoryCertificateData3        = 0x33,
    kMFIRegAccessoryCertificateData4        = 0x34,
    kMFIRegAccessoryCertificateData5        = 0x35,
    kMFIRegSelfTestStatus                   = 0x40,
    kMFIRegDeviceCertificateSerialNumber    = 0x4E,
    kMFIRegSleep                            = 0x60

} MFIRegisterAddress_t;

/*
****************************************************************
**
**  Static Variables
**
****************************************************************
*/

static int gMFIFileDescriptor = -1;

/*
****************************************************************
**
**  Private Function Declarations
**
****************************************************************
*/

static int _mfiI2CRead(uint8_t reg, uint8_t *data, uint16_t dataLen);
static int _mfiI2CWrite(uint8_t reg, uint8_t *data, uint16_t dataLen);
static uint16_t _readBigEndian16(const uint8_t *buffer, int position);

/*
****************************************************************
**
**  mfiOpen
**
**  Input:
**      None
**
**  Output:
**      None
**
**  Return:
**      int     File descriptor on success, -1 on failure
**
****************************************************************
*/
int mfiOpen(void)
{
    if (gMFIFileDescriptor > 0) {
        iAP2LogDbg("[MFI] Device already open, fd=%d", gMFIFileDescriptor);
        return gMFIFileDescriptor;
    }

    gMFIFileDescriptor = open(MFI_DEVICE_NAME, O_RDWR);

    if (gMFIFileDescriptor < 0) {
        iAP2LogError("[MFI] Failed to open device %s, errno=%d", MFI_DEVICE_NAME,
                     errno);
        return -1;
    }

    if (ioctl(gMFIFileDescriptor, I2C_SLAVE, MFI_I2C_ADDRESS) < 0) {
        iAP2LogError("[MFI] Failed to set I2C slave address 0x%02X, errno=%d",
                     MFI_I2C_ADDRESS, errno);
        close(gMFIFileDescriptor);
        gMFIFileDescriptor = -1;
        return -1;
    }

    iAP2LogDbg("[MFI] Device opened successfully, fd=%d", gMFIFileDescriptor);
    return gMFIFileDescriptor;
}

/*
****************************************************************
**
**  mfiClose
**
**  Input:
**      None
**
**  Output:
**      None
**
**  Return:
**      None
**
****************************************************************
*/
void mfiClose(void)
{
    if (gMFIFileDescriptor > 0) {
        iAP2LogDbg("[MFI] Closing device, fd=%d", gMFIFileDescriptor);
        close(gMFIFileDescriptor);
        gMFIFileDescriptor = -1;
    } else {
        iAP2LogDbg("[MFI] Device already closed");
    }
}

/*
****************************************************************
**
**  _mfiI2CRead
**
**  Input:
**      reg:        Register address to read from
**      data:       Pointer to buffer for read data
**      dataLen:    Length of data to read
**
**  Output:
**      data:       Filled with read data
**
**  Return:
**      int         Number of bytes read on success, -1 on failure
**
****************************************************************
*/
static int _mfiI2CRead(uint8_t reg, uint8_t *data, uint16_t dataLen)
{
    int ret;
    int i;

    if (gMFIFileDescriptor < 0) {
        iAP2LogError("[MFI] Device not open in _mfiI2CRead");
        return -1;
    }

    if (!data || dataLen == 0) {
        iAP2LogError("[MFI] Invalid parameters in _mfiI2CRead");
        return -1;
    }

    /* Write register address */
    for (i = MFI_MAX_RETRY_COUNT; i > 0; i--) {
        ret = write(gMFIFileDescriptor, &reg, 1);

        if (ret != 1) {
            usleep(MFI_RETRY_DELAY_US);
        } else {
            break;
        }
    }

    if (i == 0) {
        iAP2LogError("[MFI] Failed to write register address 0x%02X after %d retries",
                     reg, MFI_MAX_RETRY_COUNT);
        return -1;
    }

    /* Read data from register */
    for (i = MFI_MAX_RETRY_COUNT; i > 0; i--) {
        ret = read(gMFIFileDescriptor, data, dataLen);

        if (ret != dataLen) {
            usleep(MFI_RETRY_DELAY_US);
        } else {
            break;
        }
    }

    if (i == 0) {
        iAP2LogError("[MFI] Failed to read %u bytes from register 0x%02X after %d retries",
                     dataLen, reg, MFI_MAX_RETRY_COUNT);
        return -1;
    }

    return ret;
}

/*
****************************************************************
**
**  _mfiI2CWrite
**
**  Input:
**      reg:        Register address to write to
**      data:       Pointer to data to write
**      dataLen:    Length of data to write
**
**  Output:
**      None
**
**  Return:
**      int         Number of bytes written on success, -1 on failure
**
****************************************************************
*/
static int _mfiI2CWrite(uint8_t reg, uint8_t *data, uint16_t dataLen)
{
    int ret;
    uint16_t writeLen = 1 + dataLen;
    uint8_t *writeData = (uint8_t *)malloc(writeLen);

    if (writeData == NULL) {
        iAP2LogError("[MFI] Failed to allocate memory for write data");
        return -1;
    }

    if (gMFIFileDescriptor < 0) {
        free(writeData);
        iAP2LogError("[MFI] Device not open in _mfiI2CWrite");
        return -1;
    }

    if (!data && dataLen > 0) {
        free(writeData);
        iAP2LogError("[MFI] Invalid data pointer in _mfiI2CWrite");
        return -1;
    }

    writeData[0] = reg;

    if (dataLen > 0) {
        memcpy(writeData + 1, data, dataLen);
    }

    for (int i = MFI_MAX_RETRY_COUNT; i > 0; i--) {
        ret = write(gMFIFileDescriptor, writeData, writeLen);

        if (ret != writeLen) {
            usleep(MFI_RETRY_DELAY_US);
        } else {
            break;
        }
    }

    free(writeData);

    if (ret != writeLen) {
        iAP2LogError("[MFI] Failed to write %u bytes to register 0x%02X after %d retries",
                     dataLen, reg, MFI_MAX_RETRY_COUNT);
        return -1;
    }

    return ret;
}

/*
****************************************************************
**
**  _readBigEndian16
**
**  Input:
**      buffer:     Pointer to buffer containing big-endian data
**      position:   Starting position in buffer
**
**  Output:
**      None
**
**  Return:
**      uint16_t    Converted 16-bit value
**
****************************************************************
*/
static uint16_t _readBigEndian16(const uint8_t *buffer, int position)
{
    return (uint16_t)(((uint16_t)buffer[position + 1]) |
                      (((uint16_t)buffer[position]) << 8));
}

/*
****************************************************************
**
**  mfiGetInfo
**
**  Input:
**      info:   Pointer to buffer to store chip information
**
**  Output:
**      info:   Filled with chip information
**
**  Return:
**      int     Number of bytes read on success, -1 on failure
**
****************************************************************
*/
int mfiGetInfo(uint8_t *info)
{
    uint8_t deviceVersion;
    uint8_t authRevision;
    uint8_t authMajorVersion;
    uint8_t authMinorVersion;
    uint8_t deviceId[3];

    if (gMFIFileDescriptor < 0) {
        iAP2LogError("[MFI] Device not open in mfiGetInfo");
        return -1;
    }

    if (!info) {
        iAP2LogError("[MFI] NULL info buffer in mfiGetInfo");
        return -1;
    }

    if (_mfiI2CRead(kMFIRegDeviceVersion, &deviceVersion, 1) <= 0 ||
        _mfiI2CRead(kMFIRegAuthRevision, &authRevision, 1) <= 0 ||
        _mfiI2CRead(kMFIRegAuthMajorVersion, &authMajorVersion, 1) <= 0 ||
        _mfiI2CRead(kMFIRegAuthMinorVersion, &authMinorVersion, 1) <= 0 ||
        _mfiI2CRead(kMFIRegDeviceId, deviceId, 3) <= 0) {
        iAP2LogError("[MFI] Failed to read chip information");
        return -1;
    }

    /* Pack information into output buffer */
    info[0] = deviceVersion;
    info[1] = authRevision;
    info[2] = authMajorVersion;
    info[3] = authMinorVersion;
    info[4] = deviceId[0];
    info[5] = deviceId[1];
    info[6] = deviceId[2];
    info[7] = 0; /* Reserved */
    return 8;
}

/*
****************************************************************
**
**  mfiGetDeviceCertificateSerialNumber
**
**  Input:
**      serialNumber:   Pointer to buffer for serial number
**
**  Output:
**      serialNumber:   Filled with serial number
**
**  Return:
**      int             32 on success, -1 on failure
**
****************************************************************
*/
int mfiGetDeviceCertificateSerialNumber(uint8_t *serialNumber)
{
    if (gMFIFileDescriptor < 0) {
        iAP2LogError("[MFI] Device not open in mfiGetDeviceCertificateSerialNumber");
        return -1;
    }

    if (!serialNumber) {
        iAP2LogError("[MFI] NULL serialNumber buffer in mfiGetDeviceCertificateSerialNumber");
        return -1;
    }

    int ret = _mfiI2CRead(kMFIRegDeviceCertificateSerialNumber, serialNumber, 32);

    if (ret <= 0) {
        iAP2LogError("[MFI] Failed to read device certificate serial number");
        return -1;
    }

    return 32;
}

/*
****************************************************************
**
**  mfiReadAuthControlStatus
**
**  Input:
**      status: Pointer to buffer for authentication status
**
**  Output:
**      status: Filled with authentication status
**
**  Return:
**      int     1 on success, -1 on failure
**
****************************************************************
*/
int mfiReadAuthControlStatus(uint8_t *status)
{
    if (gMFIFileDescriptor < 0) {
        iAP2LogError("[MFI] Device not open in mfiReadAuthControlStatus");
        return -1;
    }

    if (!status) {
        iAP2LogError("[MFI] NULL status buffer in mfiReadAuthControlStatus");
        return -1;
    }

    int ret = _mfiI2CRead(kMFIRegControlStatus, status, 1);

    if (ret <= 0) {
        iAP2LogError("[MFI] Failed to read auth control status");
        return -1;
    }

    return 1;
}

/*
****************************************************************
**
**  mfiWriteAuthControlStatus
**
**  Input:
**      status: Authentication status to write
**
**  Output:
**      None
**
**  Return:
**      int     0 on success, -1 on failure
**
****************************************************************
*/
int mfiWriteAuthControlStatus(uint8_t status)
{
    if (gMFIFileDescriptor < 0) {
        iAP2LogError("[MFI] Device not open in mfiWriteAuthControlStatus");
        return -1;
    }

    int ret = _mfiI2CWrite(kMFIRegControlStatus, &status, 1);

    if (ret <= 0) {
        iAP2LogError("[MFI] Failed to write auth control status 0x%02X", status);
        return -1;
    }

    return 0;
}

/*
****************************************************************
**
**  mfiReadSelfTestStatus
**
**  Input:
**      status: Pointer to buffer for self-test status
**
**  Output:
**      status: Filled with self-test status
**
**  Return:
**      int     1 on success, -1 on failure
**
****************************************************************
*/
int mfiReadSelfTestStatus(uint8_t *status)
{
    if (gMFIFileDescriptor < 0) {
        iAP2LogError("[MFI] Device not open in mfiReadSelfTestStatus");
        return -1;
    }

    if (!status) {
        iAP2LogError("[MFI] NULL status buffer in mfiReadSelfTestStatus");
        return -1;
    }

    int ret = _mfiI2CRead(kMFIRegSelfTestStatus, status, 1);

    if (ret <= 0) {
        iAP2LogError("[MFI] Failed to read self-test status");
        return -1;
    }

    return 1;
}

/*
****************************************************************
**
**  mfiReadChallengeResponseData
**
**  Input:
**      responseData:   Pointer to buffer for challenge response
**      len:            Length of data to read
**
**  Output:
**      responseData:   Filled with challenge response data
**
**  Return:
**      int             Length read on success, -1 on failure
**
****************************************************************
*/
int mfiReadChallengeResponseData(uint8_t *responseData, uint16_t len)
{
    if (gMFIFileDescriptor < 0) {
        iAP2LogError("[MFI] Device not open in mfiReadChallengeResponseData");
        return -1;
    }

    if (!responseData || len == 0) {
        iAP2LogError("[MFI] Invalid parameters in mfiReadChallengeResponseData");
        return -1;
    }

    int ret = _mfiI2CRead(kMFIRegChallengeResponseData, responseData, len);

    if (ret <= 0) {
        iAP2LogError("[MFI] Failed to read challenge response data, len=%u", len);
        return -1;
    }

    return len;
}

/*
****************************************************************
**
**  mfiReadChallengeResponseDataLength
**
**  Input:
**      None
**
**  Output:
**      None
**
**  Return:
**      uint16_t        Length of challenge response data
**
****************************************************************
*/
uint16_t mfiReadChallengeResponseDataLength(void)
{
    uint8_t lengthBuffer[2] = {0};
    uint16_t responseLength = 0;

    if (gMFIFileDescriptor < 0) {
        iAP2LogError("[MFI] Device not open in mfiReadChallengeResponseDataLength");
        return 0;
    }

    if (_mfiI2CRead(kMFIRegChallengeResponseDataLength, lengthBuffer, 2) > 0) {
        responseLength = _readBigEndian16(lengthBuffer, 0);
    } else {
        iAP2LogError("[MFI] Failed to read challenge response data length");
    }

    return responseLength;
}

/*
****************************************************************
**
**  mfiReadChallengeData
**
**  Input:
**      challengeData:  Pointer to buffer for challenge data
**
**  Output:
**      challengeData:  Filled with challenge data
**
**  Return:
**      int             32 on success, -1 on failure
**
****************************************************************
*/
int mfiReadChallengeData(uint8_t *challengeData)
{
    if (gMFIFileDescriptor < 0) {
        iAP2LogError("[MFI] Device not open in mfiReadChallengeData");
        return -1;
    }

    if (!challengeData) {
        iAP2LogError("[MFI] NULL challengeData buffer in mfiReadChallengeData");
        return -1;
    }

    int ret = _mfiI2CRead(kMFIRegChallengeData, challengeData, 32);

    if (ret <= 0) {
        iAP2LogError("[MFI] Failed to read challenge data");
        return -1;
    }

    return 32;
}

/*
****************************************************************
**
**  mfiWriteChallengeData
**
**  Input:
**      challengeData:  Pointer to challenge data to write
**      len:            Length of challenge data
**
**  Output:
**      None
**
**  Return:
**      int             0 on success, -1 on failure
**
****************************************************************
*/
int mfiWriteChallengeData(uint8_t *challengeData, uint16_t len)
{
    if (gMFIFileDescriptor < 0) {
        iAP2LogError("Failed to write challenge data to chip");
        return -1;
    }

    if (challengeData == NULL) {
        iAP2LogError("Challenge data is NULL");
        return -1;
    }

    if (len != MFI_CHALLENGE_DATA_LENGTH_MAX) {
        iAP2LogError("Invalid challenge data length, expected %d, got %d",
                     MFI_CHALLENGE_DATA_LENGTH_MAX, len);
        return -1;
    }

    return (_mfiI2CWrite(kMFIRegChallengeData, challengeData, len) > 0) ? 0 : -1;
}

/*
****************************************************************
**
**  mfiReadAccessoryCertificateData
**
**  Input:
**      certData:       Pointer to buffer for certificate data
**      pLen:           Pointer to store certificate length
**
**  Output:
**      certData:       Filled with certificate data
**      pLen:           Set to certificate length
**
**  Return:
**      int             Certificate length on success, -1 on failure
**
****************************************************************
*/
int mfiReadAccessoryCertificateData(uint8_t *certData, uint16_t *pLen)
{
    uint8_t lengthBuffer[2] = {0};
    uint16_t certificateLength = 0;
    int i;

    if (gMFIFileDescriptor < 0) {
        iAP2LogError("[MFI] Device not open in mfiReadAccessoryCertificateData");
        return -1;
    }

    if (!certData || !pLen) {
        iAP2LogError("[MFI] Invalid parameters in mfiReadAccessoryCertificateData");
        return -1;
    }

    if (_mfiI2CRead(kMFIRegAccessoryCertificateDataLength, lengthBuffer, 2) <= 0) {
        iAP2LogError("[MFI] Failed to read certificate data length");
        return -1;
    }

    certificateLength = _readBigEndian16(lengthBuffer, 0);

    /* Read certificate data in 128-byte pages */
    for (i = 0; i < certificateLength / MFI_ACCESSORY_CERT_PAGE_LENGTH_MAX; i++) {
        if (_mfiI2CRead(kMFIRegAccessoryCertificateData1 + i,
                        certData + i * MFI_ACCESSORY_CERT_PAGE_LENGTH_MAX,
                        MFI_ACCESSORY_CERT_PAGE_LENGTH_MAX) <= 0) {
            iAP2LogError("[MFI] Failed to read certificate page %d", i);
            return -1;
        }
    }

    /* Read remaining bytes if any */
    if (certificateLength % MFI_ACCESSORY_CERT_PAGE_LENGTH_MAX) {
        if (_mfiI2CRead(kMFIRegAccessoryCertificateData1 + i,
                        certData + i * MFI_ACCESSORY_CERT_PAGE_LENGTH_MAX,
                        certificateLength % MFI_ACCESSORY_CERT_PAGE_LENGTH_MAX) <= 0) {
            iAP2LogError("[MFI] Failed to read certificate remaining bytes");
            return -1;
        }
    }

    *pLen = certificateLength;
    return certificateLength;
}

/*
****************************************************************
**
**  mfiAuthenticationCertificate
**
**  Input:
**      x509Certificate:    Pointer to buffer for X.509 certificate
**      pLen:               Pointer to store certificate length
**
**  Output:
**      x509Certificate:    Filled with X.509 certificate data
**      pLen:               Set to certificate length
**
**  Return:
**      int                 Certificate length on success, -1 on failure
**
****************************************************************
*/
int mfiAuthenticationCertificate(uint8_t *x509Certificate, uint16_t *pLen)
{
    if (!x509Certificate || !pLen) {
        iAP2LogError("[MFI] Invalid parameters in mfiAuthenticationCertificate");
        return -1;
    }

    int ret = mfiReadAccessoryCertificateData(x509Certificate, pLen);

    if (ret < 0) {
        iAP2LogError("[MFI] Failed to read authentication certificate");
    }

    return ret;
}

/*
****************************************************************
**
**  mfiAuthenticationResponse
**
**  Input:
**      challengeData:          Pointer to challenge data
**      challengeResponseData:  Pointer to buffer for response data
**      pLen:                   Pointer to store response length
**
**  Output:
**      challengeResponseData:  Filled with challenge response
**      pLen:                   Set to response length
**
**  Return:
**      int                     Response length on success, negative on failure
**
****************************************************************
*/
int mfiAuthenticationResponse(uint8_t *challengeData, uint16_t challengeDataLen,
                              uint8_t *challengeResponseData,
                              uint16_t *pLen)
{
    int ret;
    uint8_t authControlStatus = 0;
    int timeoutCount = 0;
    uint16_t responseLength = 0;
    /* Write challenge data to chip */
    ret = mfiWriteChallengeData(challengeData, challengeDataLen);

    if (ret != 0) {
        iAP2LogError("Failed to write challenge data to chip");
        return -1;
    }

    /* Start authentication process */
    ret = mfiWriteAuthControlStatus(1);

    if (ret != 0) {
        iAP2LogError("Failed to write auth control status to chip");
        return -2;
    }

    /* Wait for authentication to complete */
    while (timeoutCount < (MFI_AUTH_TIMEOUT_MS / MFI_AUTH_POLL_INTERVAL_MS)) {
        ret = mfiReadAuthControlStatus(&authControlStatus);

        if (ret <= 0) {
            iAP2LogError("Failed to read auth control status from chip");
            return -3;
        }

        /* Check if authentication is complete and successful */
        if ((authControlStatus & 0x80) == 0 &&
            ((authControlStatus >> 4) & 0x07) == 1) {
            break;
        }

        usleep(MFI_AUTH_POLL_INTERVAL_MS * 1000);
        timeoutCount++;
    }

    if (timeoutCount >= (MFI_AUTH_TIMEOUT_MS / MFI_AUTH_POLL_INTERVAL_MS)) {
        iAP2LogError("Authentication process timed out");
        return -4;
    }

    /* Read response length */
    responseLength = mfiReadChallengeResponseDataLength();

    if (responseLength == 0) {
        iAP2LogError("Failed to read challenge response data length");
        return -5;
    }

    /* Read response data */
    ret = mfiReadChallengeResponseData(challengeResponseData, responseLength);

    if (ret <= 0) {
        iAP2LogError("Failed to read challenge response data from chip");
        return -6;
    }

    *pLen = responseLength;
    return responseLength;
}