/**
 * @file iap2_control.h
 * @brief iAP2 Control Session Message Encoder/Decoder
 *
 * This file provides APIs to encode and decode iAP2 control session messages
 * as defined in Apple's Accessory Interface Specification (e.g., R42.1).
 * Supports all parameter types: bool, int, enum, string, blob, array, group, etc.
 *
 * Endianness: All multi-byte fields are big-endian (network order).
 */

#ifndef __IAP2_CONTROL_H__
#define __IAP2_CONTROL_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/** iAP2 message start marker */
#define IAP2_MSG_START 0x4040U

/* === Macros for Big-Endian Reading === */
/**
 * @brief Read a big-endian uint16 from byte array
 * @param p Pointer to byte array (at least 2 bytes)
 * @return Host-order uint16
 */
#define READ_U16(p) ((uint16_t)(((p)[0] << 8) | (p)[1]))

/**
 * @brief Read a big-endian uint32 from byte array
 * @param p Pointer to byte array (at least 4 bytes)
 * @return Host-order uint32
 */
#define READ_U32(p) ((uint32_t)(((p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | (p)[3]))

/* === Parameter Types === */

/**
 * @enum iAP2ParamType
 * @brief Inferred or explicit iAP2 parameter data types
 */
typedef enum {
    CTRL_SESS_PARAM_TYPE_NONE,      /**< No data (length=4) */
    CTRL_SESS_PARAM_TYPE_BOOL,      /**< Boolean (1 byte: 0 or 1) */
    CTRL_SESS_PARAM_TYPE_ENUM,      /**< Enum (1 byte unsigned) */
    CTRL_SESS_PARAM_TYPE_INT8,
    CTRL_SESS_PARAM_TYPE_UINT8,
    CTRL_SESS_PARAM_TYPE_INT16,
    CTRL_SESS_PARAM_TYPE_UINT16,
    CTRL_SESS_PARAM_TYPE_INT32,
    CTRL_SESS_PARAM_TYPE_UINT32,
    CTRL_SESS_PARAM_TYPE_INT64,
    CTRL_SESS_PARAM_TYPE_UINT64,
    CTRL_SESS_PARAM_TYPE_SECS16,    /**< Duration in seconds (uint16) */
    CTRL_SESS_PARAM_TYPE_MSECS32,   /**< Duration in milliseconds (uint32) */
    CTRL_SESS_PARAM_TYPE_USECS64,   /**< Duration in microseconds (uint64) */
    CTRL_SESS_PARAM_TYPE_RAT16,     /**< Signed rational (int16 num/den) */
    CTRL_SESS_PARAM_TYPE_RAT32,     /**< Signed rational (int32 num/den) */
    CTRL_SESS_PARAM_TYPE_URAT16,    /**< Unsigned rational (uint16 num/den) */
    CTRL_SESS_PARAM_TYPE_URAT32,    /**< Unsigned rational (uint32 num/den) */
    CTRL_SESS_PARAM_TYPE_UTF8,      /**< Null-terminated UTF-8 string */
    CTRL_SESS_PARAM_TYPE_BLOB,      /**< Arbitrary binary data */
    CTRL_SESS_PARAM_TYPE_ARRAY,     /**< Homogeneous array (e.g., uint32[4]) */
    CTRL_SESS_PARAM_TYPE_GROUP      /**< Nested group of parameters */
} ctrlSessParamType;

/* === Structures === */

/**
 * @struct iAP2Parameter
 * @brief Represents a single decoded iAP2 parameter
 */
typedef struct {
    uint16_t id;          /**< Parameter ID */
    uint16_t length;      /**< Total length = 4 + data_len */
    const uint8_t *data;  /**< Points to data (NULL if none) */
    ctrlSessParamType inferred_type; /**< Optional type hint */
} ctrlSessParameter;

/**
 * @struct iAP2Message
 * @brief Represents a decoded iAP2 message header
 */
typedef struct {
    uint16_t msgId;             /**< Message ID */
    uint16_t totalLength;       /**< Total message length (including header) */
    const uint8_t *paramStart;  /**< Start of first parameter */
} ctrlSessMessage;

/**
 * @struct iAP2Builder
 * @brief Context for building an iAP2 message
 */
typedef struct {
    uint8_t *buffer;   /**< Output buffer */
    uint16_t offset;   /**< Current write offset */
    uint16_t maxLen;   /**< Max buffer size */
} ctrlSessBuilder;

/**
 * @struct iAP2GroupIterator
 * @brief Iterator for traversing parameters inside a GROUP
 */
typedef struct {
    const uint8_t *start; /**< Start of group data */
    size_t len;           /**< Total group data length */
    size_t offset;        /**< Current offset within group */
} ctrlSessGroupIterator;

/**
 * @struct iAP2GroupBuilder
 * @brief Helper to build nested GROUP parameters
 */
typedef struct {
    ctrlSessBuilder *builder;    /**< Parent builder */
    int headerPos;           /**< Offset of group header in buffer */
    uint16_t paramId;        /**< Group parameter ID */
} ctrlSessGroupBuilder;

/* === Rational Types === */

/** @brief Signed 16-bit rational (numerator/denominator) */
typedef struct {
    int16_t num, den;
} rat16_t;
/** @brief Signed 32-bit rational */
typedef struct {
    int32_t num, den;
} rat32_t;
/** @brief Unsigned 16-bit rational */
typedef struct {
    uint16_t num, den;
} urat16_t;
/** @brief Unsigned 32-bit rational */
typedef struct {
    uint32_t num, den;
} urat32_t;

/* === Function Declarations === */

/* --- Decoding --- */
int ctrlSess_DecodeMessageHeader(const uint8_t *buffer, size_t bufLen,
                                 ctrlSessMessage *msg);
int ctrlSess_GetNextParameter(const uint8_t *buffer, size_t remainingLen,
                              ctrlSessParameter *param);
void ctrlSess_InitGroupIterator(ctrlSessGroupIterator *it,
                                const ctrlSessParameter *group);
int ctrlSess_GroupNext(ctrlSessGroupIterator *it, ctrlSessParameter *sub);

/* --- Type-safe accessors --- */
int ctrlSess_ParamGetBool(const ctrlSessParameter *p, bool *out);
int ctrlSess_ParamGetEnum(const ctrlSessParameter *p, uint8_t *out);
int8_t  ctrlSess_ParamGetInt8(const ctrlSessParameter *p);
uint8_t ctrlSess_ParamGetUint8(const ctrlSessParameter *p);
int16_t ctrlSess_ParamGetInt16(const ctrlSessParameter *p);
uint16_t ctrlSess_ParamGetUint16(const ctrlSessParameter *p);
int32_t ctrlSess_ParamGetInt32(const ctrlSessParameter *p);
uint32_t ctrlSess_ParamGetUint32(const ctrlSessParameter *p);
uint16_t ctrlSess_ParamGetSecs16(const ctrlSessParameter *p);
uint32_t ctrlSess_ParamGetMsecs32(const ctrlSessParameter *p);
rat32_t ctrlSess_ParamGetRat32(const ctrlSessParameter *p);
const char *ctrlSess_ParamGetString(const ctrlSessParameter *p);
const uint8_t *ctrlSess_ParamGetBlob(const ctrlSessParameter *p, size_t *len);
bool ctrlSess_ParamIsNone(const ctrlSessParameter *p);

/* --- Encoding --- */
void ctrlSess_BuilderInit(ctrlSessBuilder *b, uint8_t *buf, uint16_t maxLen,
                          uint16_t msgId);
int ctrlSess_AddParameter(ctrlSessBuilder *b, uint16_t paramId,
                          const uint8_t *data, uint16_t dataLen);
uint16_t ctrlSess_BuilderFinish(ctrlSessBuilder *b);

/* --- Typed encoding helpers --- */
int ctrlSess_AddBool(ctrlSessBuilder *b, uint16_t paramId, bool value);
int ctrlSess_AddEnum(ctrlSessBuilder *b, uint16_t paramId, uint8_t value);
int ctrlSess_AddUint8(ctrlSessBuilder *b, uint16_t paramId, int8_t value);
int ctrlSess_AddUint16(ctrlSessBuilder *b, uint16_t paramId, uint16_t value);
int ctrlSess_AddUint32(ctrlSessBuilder *b, uint16_t paramId, uint32_t value);
int ctrlSess_AddString(ctrlSessBuilder *b, uint16_t paramId, const char *str);
int ctrlSess_AddBlob(ctrlSessBuilder *b, uint16_t paramId, const void *data,
                     size_t len);
int ctrlSess_AddNone(ctrlSessBuilder *b, uint16_t paramId);
int ctrlSess_AddRat32(ctrlSessBuilder *b, uint16_t paramId, int32_t num,
                      int32_t den);
int ctrlSess_AddUint16Array(ctrlSessBuilder *b, uint16_t paramId,
                            const uint16_t *arr, size_t count);
int ctrlSess_AddUint32Array(ctrlSessBuilder *b, uint16_t paramId,
                            const uint32_t *arr, size_t count);

/* --- Group encoding --- */
int ctrlSess_BeginGroup(ctrlSessGroupBuilder *gb, ctrlSessBuilder *b,
                        uint16_t paramId);
int ctrlSess_EndGroup(ctrlSessGroupBuilder *gb);

#endif /* __IAP2_CONTROL_H__ */