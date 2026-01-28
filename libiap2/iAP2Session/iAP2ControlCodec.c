/**
 * @file iap2_control.c
 * @brief Implementation of iAP2 control session message encoder/decoder
 */

#include "iAP2ControlCodec.h"
#include <string.h>
#include <stdlib.h>

/**
 * @brief Write a uint16 in big-endian to byte array
 * @param p Destination (2 bytes)
 * @param val Value to write
 */
static void write_u16(uint8_t *p, uint16_t val)
{
    p[0] = (val >> 8) & 0xFF;
    p[1] = val & 0xFF;
}

/**
 * @brief Write a uint32 in big-endian to byte array
 * @param p Destination (4 bytes)
 * @param val Value to write
 */
static void write_u32(uint8_t *p, uint32_t val)
{
    p[0] = (val >> 24) & 0xFF;
    p[1] = (val >> 16) & 0xFF;
    p[2] = (val >> 8) & 0xFF;
    p[3] = val & 0xFF;
}

/**
 * @brief Decode iAP2 message header
 * @param buffer Input buffer
 * @param bufLen Buffer length
 * @param msg Output message structure
 * @return 0 on success, negative error code otherwise
 */
int ctrlSess_DecodeMessageHeader(const uint8_t *buffer, size_t bufLen, ctrlSessMessage *msg)
{
    if (bufLen < 6)
        return -1; /* too short */

    if (READ_U16(buffer) != IAP2_MSG_START)
        return -2; /* invalid start */

    msg->totalLength = READ_U16(buffer + 2);
    msg->msgId = READ_U16(buffer + 4);
    msg->paramStart = buffer + 6;

    if (msg->totalLength > bufLen || msg->totalLength < 6)
        return -3; /* incomplete or malformed */

    return 0;
}

/**
 * @brief Parse next parameter from buffer
 * @param buffer Current position in parameter list
 * @param remainingLen Bytes left in buffer
 * @param param Output parameter
 * @return Number of bytes consumed (>0), 0 if done, <0 on error
 */
int ctrlSess_GetNextParameter(const uint8_t *buffer, size_t remainingLen, ctrlSessParameter *param)
{
    if (remainingLen < 4)
        return 0; /* no more parameters */

    param->length = READ_U16(buffer);
    param->id = READ_U16(buffer + 2);

    if (param->length < 4 || param->length > remainingLen)
        return -1; /* invalid length */

    param->data = (param->length > 4) ? (buffer + 4) : NULL;
    param->inferred_type = CTRL_SESS_PARAM_TYPE_NONE; /* caller may refine */

    return (int)param->length;
}

/**
 * @brief Initialize iterator for GROUP parameter
 * @param it Iterator to initialize
 * @param group The GROUP parameter
 */
void ctrlSess_InitGroupIterator(ctrlSessGroupIterator *it, const ctrlSessParameter *group)
{
    it->start = group->data;
    it->len = (group->length >= 4) ? (group->length - 4) : 0;
    it->offset = 0;
}

/**
 * @brief Get next sub-parameter from GROUP
 * @param it Group iterator
 * @param sub Output sub-parameter
 * @return Bytes consumed (>0), 0 if done, <0 on error
 */
int ctrlSess_GroupNext(ctrlSessGroupIterator *it, ctrlSessParameter *sub)
{
    if (it->offset >= it->len)
        return 0;

    int res = ctrlSess_GetNextParameter(it->start + it->offset,
                    it->len - it->offset, sub);
    if (res <= 0)
        return res;

    it->offset += (size_t)res;
    return res;
}

/* === Type-Safe Accessors === */

int ctrlSess_ParamGetBool(const ctrlSessParameter *p, bool *out)
{
    if (!p || p->length != 5 || !p->data)
        return -1;
    uint8_t v = p->data[0];
    if (v > 1)
        return -2;
    *out = (v == 1);
    return 0;
}

int ctrlSess_ParamGetEnum(const ctrlSessParameter *p, uint8_t *out)
{
    if (!p || p->length != 5 || !p->data)
        return -1;
    *out = p->data[0];
    return 0;
}

int8_t ctrlSess_ParamGetInt8(const ctrlSessParameter *p)
{
    return p && p->data ? (int8_t)p->data[0] : 0;
}

uint8_t ctrlSess_ParamGetUint8(const ctrlSessParameter *p)
{
    return p && p->data ? p->data[0] : 0;
}

int16_t ctrlSess_ParamGetInt16(const ctrlSessParameter *p)
{
    return p && p->data && p->length >= 6 ? (int16_t)READ_U16(p->data) : 0;
}

uint16_t ctrlSess_ParamGetUint16(const ctrlSessParameter *p)
{
    return p && p->data && p->length >= 6 ? READ_U16(p->data) : 0;
}

int32_t ctrlSess_ParamGetInt32(const ctrlSessParameter *p)
{
    return p && p->data && p->length >= 8 ? (int32_t)READ_U32(p->data) : 0;
}

uint32_t ctrlSess_ParamGetUint32(const ctrlSessParameter *p)
{
    return p && p->data && p->length >= 8 ? READ_U32(p->data) : 0;
}

uint16_t ctrlSess_ParamGetSecs16(const ctrlSessParameter *p)
{
    return ctrlSess_ParamGetUint16(p);
}

uint32_t ctrlSess_ParamGetMsecs32(const ctrlSessParameter *p)
{
    return ctrlSess_ParamGetUint32(p);
}

rat32_t ctrlSess_ParamGetRat32(const ctrlSessParameter *p)
{
    rat32_t r = {0, 0};
    if (p && p->data && p->length >= 12) {
        r.num = (int32_t)READ_U32(p->data);
        r.den = (int32_t)READ_U32(p->data + 4);
    }
    return r;
}

const char *ctrlSess_ParamGetString(const ctrlSessParameter *p)
{
    if (!p || p->length < 5)
        return NULL;
    return (const char *)p->data;
}

const uint8_t *ctrlSess_ParamGetBlob(const ctrlSessParameter *p, size_t *len)
{
    if (!p || p->length < 4) {
        if (len)
            *len = 0;
        return NULL;
    }
    if (len)
        *len = (size_t)(p->length - 4);
    return p->data;
}

bool ctrlSess_ParamIsNone(const ctrlSessParameter *p)
{
    return p && p->length == 4 && p->data == NULL;
}

/* === Encoding === */

void ctrlSess_BuilderInit(ctrlSessBuilder *b, uint8_t *buf, uint16_t maxLen, uint16_t msgId)
{
    b->buffer = buf;
    b->maxLen = maxLen;
    write_u16(buf, IAP2_MSG_START);
    write_u16(buf + 4, msgId);
    b->offset = 6;
}

int ctrlSess_AddParameter(ctrlSessBuilder *b, uint16_t paramId, const uint8_t *data, uint16_t dataLen)
{
    uint16_t paramTotalLen = 4 + dataLen;
    if (b->offset + paramTotalLen > b->maxLen)
        return -1;

    write_u16(b->buffer + b->offset, paramTotalLen);
    write_u16(b->buffer + b->offset + 2, paramId);
    if (data && dataLen > 0)
        memcpy(b->buffer + b->offset + 4, data, dataLen);
    b->offset += paramTotalLen;
    return 0;
}

uint16_t ctrlSess_BuilderFinish(ctrlSessBuilder *b)
{
    write_u16(b->buffer + 2, b->offset);
    return b->offset;
}

/* === Typed Adders === */

int ctrlSess_AddBool(ctrlSessBuilder *b, uint16_t paramId, bool value)
{
    uint8_t v = value ? 1 : 0;
    return ctrlSess_AddParameter(b, paramId, &v, 1);
}

int ctrlSess_AddEnum(ctrlSessBuilder *b, uint16_t paramId, uint8_t value)
{
    return ctrlSess_AddParameter(b, paramId, &value, 1);
}

int ctrlSess_AddUint8(ctrlSessBuilder *b, uint16_t paramId, int8_t value)
{
    return ctrlSess_AddParameter(b, paramId, (uint8_t *)&value, 1);
}

int ctrlSess_AddUint16(ctrlSessBuilder *b, uint16_t paramId, uint16_t value)
{
    uint8_t buf[2];
    write_u16(buf, value);
    return ctrlSess_AddParameter(b, paramId, buf, 2);
}

int ctrlSess_AddUint32(ctrlSessBuilder *b, uint16_t paramId, uint32_t value)
{
    uint8_t buf[4];
    write_u32(buf, value);
    return ctrlSess_AddParameter(b, paramId, buf, 4);
}

int ctrlSess_AddString(ctrlSessBuilder *b, uint16_t paramId, const char *str)
{
    if (!str)
        str = "";
    return ctrlSess_AddParameter(b, paramId, (uint8_t *)str, (uint16_t)(strlen(str) + 1));
}

int ctrlSess_AddBlob(ctrlSessBuilder *b, uint16_t paramId, const void *data, size_t len)
{
    if (len > 0xFFFF)
        return -1;
    return ctrlSess_AddParameter(b, paramId, (const uint8_t *)data, (uint16_t)len);
}

int ctrlSess_AddNone(ctrlSessBuilder *b, uint16_t paramId)
{
    return ctrlSess_AddParameter(b, paramId, NULL, 0);
}

int ctrlSess_AddRat32(ctrlSessBuilder *b, uint16_t paramId, int32_t num, int32_t den)
{
    uint8_t buf[8];
    write_u32(buf, (uint32_t)num);
    write_u32(buf + 4, (uint32_t)den);
    return ctrlSess_AddParameter(b, paramId, buf, 8);
}

int ctrlSess_AddUint16Array(ctrlSessBuilder *b, uint16_t paramId, const uint16_t *arr, size_t count)
{
    if (count == 0)
        return ctrlSess_AddParameter(b, paramId, NULL, 0);
    if (count > (0xFFFF / 2))
        return -1;
    size_t len = count * sizeof(uint16_t);
    uint8_t *buf = malloc(len);
    if (!buf)
        return -1;
    for (size_t i = 0; i < count; i++) {
        write_u16(buf + i * 2, arr[i]);
    }
    int res = ctrlSess_AddParameter(b, paramId, buf, (uint16_t)len);
    free(buf);
    return res;
}

int ctrlSess_AddUint32Array(ctrlSessBuilder *b, uint16_t paramId, const uint32_t *arr, size_t count)
{
    if (count == 0)
        return ctrlSess_AddParameter(b, paramId, NULL, 0);
    if (count > (0xFFFF / 4))
        return -1;
    size_t len = count * sizeof(uint32_t);
    uint8_t *buf = malloc(len);
    if (!buf)
        return -1;
    for (size_t i = 0; i < count; i++)
        write_u32(buf + i * 4, arr[i]);
    int res = ctrlSess_AddParameter(b, paramId, buf, (uint16_t)len);
    free(buf);
    return res;
}

/* === Group Building === */

int ctrlSess_BeginGroup(ctrlSessGroupBuilder *gb, ctrlSessBuilder *b, uint16_t paramId)
{
    if (b->offset + 4 > b->maxLen)
        return -1;
    gb->builder = b;
    gb->paramId = paramId;
    gb->headerPos = b->offset;
    b->offset += 4;
    return 0;
}

int ctrlSess_EndGroup(ctrlSessGroupBuilder *gb)
{
    ctrlSessBuilder *b = gb->builder;
    if (gb->headerPos < 0 || (size_t)(gb->headerPos + 4) > b->offset)
        return -1;
    uint16_t dataLen = b->offset - (gb->headerPos + 4);
    write_u16(b->buffer + gb->headerPos, 4 + dataLen);
    write_u16(b->buffer + gb->headerPos + 2, gb->paramId);
    return 0;
}