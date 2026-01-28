/*
 *  File: mfiI2c.h
 *  Package: MFIDriver
 *  Abstract: MFi Authentication Chip I2C Driver Interface
 */

#ifndef MFIDriver_mfiI2c_h
#define MFIDriver_mfiI2c_h

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
****************************************************************
**
**  MFI Chip I2C Driver Interface
**
****************************************************************
*/

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
**  Note: Opens the MFi chip I2C device for communication
**
****************************************************************
*/
int mfiOpen(void);

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
**  Note: Closes the MFi chip I2C device
**
****************************************************************
*/
void mfiClose(void);

/*
****************************************************************
**
**  mfiGetInfo
**
**  Input:
**      info:   Pointer to buffer to store chip information
**
**  Output:
**      info:   Filled with chip information (8 bytes)
**
**  Return:
**      int     Number of bytes read on success, -1 on failure
**
****************************************************************
*/
int mfiGetInfo(uint8_t *info);

/*
****************************************************************
**
**  mfiGetDeviceCertificateSerialNumber
**
**  Input:
**      serialNumber:   Pointer to buffer for serial number
**
**  Output:
**      serialNumber:   Filled with 32-byte device certificate serial number
**
**  Return:
**      int     32 on success, -1 on failure
**
****************************************************************
*/
int mfiGetDeviceCertificateSerialNumber(uint8_t *serialNumber);

/*
****************************************************************
**
**  mfiReadAuthControlStatus
**
**  Input:
**      status: Pointer to buffer for authentication status
**
**  Output:
**      status: Filled with current authentication control status
**
**  Return:
**      int     1 on success, -1 on failure
**
****************************************************************
*/
int mfiReadAuthControlStatus(uint8_t *status);

/*
****************************************************************
**
**  mfiWriteAuthControlStatus
**
**  Input:
**      status: Authentication control status to write
**
**  Output:
**      None
**
**  Return:
**      int     0 on success, -1 on failure
**
****************************************************************
*/
int mfiWriteAuthControlStatus(uint8_t status);

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
int mfiReadSelfTestStatus(uint8_t *status);

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
int mfiReadChallengeResponseData(uint8_t *responseData, uint16_t len);

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
**      uint16_t        Length of challenge response data, 0 on error
**
****************************************************************
*/
uint16_t mfiReadChallengeResponseDataLength(void);

/*
****************************************************************
**
**  mfiReadChallengeData
**
**  Input:
**      challengeData:  Pointer to buffer for challenge data
**
**  Output:
**      challengeData:  Filled with 32-byte challenge data
**
**  Return:
**      int             32 on success, -1 on failure
**
****************************************************************
*/
int mfiReadChallengeData(uint8_t *challengeData);

/*
****************************************************************
**
**  mfiWriteChallengeData
**
**  Input:
**      challengeData:  Pointer to challenge data to write
**      len:            Length of challenge data (should be 32)
**
**  Output:
**      None
**
**  Return:
**      int             0 on success, -1 on failure
**
****************************************************************
*/
int mfiWriteChallengeData(uint8_t *challengeData, uint16_t len);

/*
****************************************************************
**
**  mfiReadAccessoryCertificateData
**
**  Input:
**      certData:       Pointer to buffer for certificate data
**      pLen:           Pointer to store actual length read
**
**  Output:
**      certData:       Filled with X.509 certificate data
**      pLen:           Set to actual certificate length
**
**  Return:
**      int             Certificate length on success, -1 on failure
**
****************************************************************
*/
int mfiReadAccessoryCertificateData(uint8_t *certData, uint16_t *pLen);

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
int mfiAuthenticationCertificate(uint8_t *x509Certificate, uint16_t *pLen);

/*
****************************************************************
**
**  mfiAuthenticationResponse
**
**  Input:
**      challengeData:          Pointer to challenge data
**      challengeDataLen:       Length of challenge data (should be 32)
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
                              uint16_t *pLen);

#ifdef __cplusplus
}
#endif

#endif /* MFIDriver_mfiI2c_h */
