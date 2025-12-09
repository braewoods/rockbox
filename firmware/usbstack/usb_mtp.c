/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2026 by James Buren
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include "usb_core.h"
#include "usb_drv.h"
#include "usb_class_driver.h"
#include "usb_mtp.h"
#include "core_alloc.h"
#include "panic.h"
#include "version.h"

// MTP Phases
#define MTP_PHASE_REQUEST  1
#define MTP_PHASE_DATA     2
#define MTP_PHASE_STREAM   3
#define MTP_PHASE_RESPONSE 4

// MTP Container Types
#define MTP_TYPE_UNDEFINED 0x0000
#define MTP_TYPE_REQUEST   0x0001
#define MTP_TYPE_DATA      0x0002
#define MTP_TYPE_RESPONSE  0x0003
#define MTP_TYPE_EVENT     0x0004

// MTP Request Codes
#define MTP_REQUEST_GET_DEVICE_INFO               0x1001
#define MTP_REQUEST_OPEN_SESSION                  0x1002
#define MTP_REQUEST_CLOSE_SESSION                 0x1003
#define MTP_REQUEST_GET_STORAGE_IDS               0x1004
#define MTP_REQUEST_GET_STORAGE_INFO              0x1005
#define MTP_REQUEST_GET_NUM_OBJECTS               0x1006
#define MTP_REQUEST_GET_OBJECT_HANDLES            0x1007
#define MTP_REQUEST_GET_OBJECT_INFO               0x1008
#define MTP_REQUEST_GET_OBJECT                    0x1009
#define MTP_REQUEST_GET_THUMB                     0x100A
#define MTP_REQUEST_DELETE_OBJECT                 0x100B
#define MTP_REQUEST_SEND_OBJECT_INFO              0x100C
#define MTP_REQUEST_SEND_OBJECT                   0x100D
#define MTP_REQUEST_INITIATE_CAPTURE              0x100E
#define MTP_REQUEST_FORMAT_STORE                  0x100F
#define MTP_REQUEST_RESET_DEVICE                  0x1010
#define MTP_REQUEST_SELF_TEST                     0x1011
#define MTP_REQUEST_SET_OBJECT_PROTECTION         0x1012
#define MTP_REQUEST_POWER_DOWN                    0x1013
#define MTP_REQUEST_GET_DEVICE_PROP_DESC          0x1014
#define MTP_REQUEST_GET_DEVICE_PROP_VALUE         0x1015
#define MTP_REQUEST_SET_DEVICE_PROP_VALUE         0x1016
#define MTP_REQUEST_RESET_DEVICE_PROP_VALUE       0x1017
#define MTP_REQUEST_TERMINATE_OPEN_CAPTURE        0x1018
#define MTP_REQUEST_MOVE_OBJECT                   0x1019
#define MTP_REQUEST_COPY_OBJECT                   0x101A
#define MTP_REQUEST_GET_PARTIAL_OBJECT            0x101B
#define MTP_REQUEST_INITIATE_OPEN_CAPTURE         0x101C
#define MTP_REQUEST_GET_OBJECT_PROPS_SUPPORTED    0x9801
#define MTP_REQUEST_GET_OBJECT_PROP_DESC          0x9802
#define MTP_REQUEST_GET_OBJECT_PROP_VALUE         0x9803
#define MTP_REQUEST_SET_OBJECT_PROP_VALUE         0x9804
#define MTP_REQUEST_GET_OBJECT_PROP_LIST          0x9805
#define MTP_REQUEST_SET_OBJECT_PROP_LIST          0x9806
#define MTP_REQUEST_GET_INTER_DEPENDENT_PROP_DESC 0x9807
#define MTP_REQUEST_SEND_OBJECT_PROP_LIST         0x9808
#define MTP_REQUEST_GET_OBJECT_REFERENCES         0x9810
#define MTP_REQUEST_SET_OBJECT_REFERENCES         0x9811
#define MTP_REQUEST_SKIP                          0x9820

// MTP Response Codes
#define MTP_RESPONSE_UNDEFINED                                0x2000
#define MTP_RESPONSE_OK                                       0x2001
#define MTP_RESPONSE_GENERAL_ERROR                            0x2002
#define MTP_RESPONSE_SESSION_NOT_OPEN                         0x2003
#define MTP_RESPONSE_INVALID_TRANSACTION_ID                   0x2004
#define MTP_RESPONSE_REQUEST_NOT_SUPPORTED                    0x2005
#define MTP_RESPONSE_PARAMETER_NOT_SUPPORTED                  0x2006
#define MTP_RESPONSE_INCOMPLETE_TRANSFER                      0x2007
#define MTP_RESPONSE_INVALID_STORAGE_ID                       0x2008
#define MTP_RESPONSE_INVALID_OBJECT_HANDLE                    0x2009
#define MTP_RESPONSE_DEVICEPROP_NOT_SUPPORTED                 0x200A
#define MTP_RESPONSE_INVALID_OBJECT_FORMAT_CODE               0x200B
#define MTP_RESPONSE_STORE_FULL                               0x200C
#define MTP_RESPONSE_OBJECT_WRITE_PROTECTED                   0x200D
#define MTP_RESPONSE_STORE_READ_ONLY                          0x200E
#define MTP_RESPONSE_ACCESS_DENIED                            0x200F
#define MTP_RESPONSE_NO_THUMBNAIL_PRESENT                     0x2010
#define MTP_RESPONSE_SELFTEST_FAILED                          0x2011
#define MTP_RESPONSE_PARTIAL_DELETION                         0x2012
#define MTP_RESPONSE_STORE_NOT_AVAILABLE                      0x2013
#define MTP_RESPONSE_SPECIFICATION_BY_FORMAT_UNSUPPORTED      0x2014
#define MTP_RESPONSE_NO_VALID_OBJECT_INFO                     0x2015
#define MTP_RESPONSE_INVALID_CODE_FORMAT                      0x2016
#define MTP_RESPONSE_UNKNOWN_VENDOR_CODE                      0x2017
#define MTP_RESPONSE_CAPTURE_ALREADY_TERMINATED               0x2018
#define MTP_RESPONSE_DEVICE_BUSY                              0x2019
#define MTP_RESPONSE_INVALID_PARENT_OBJECT                    0x201A
#define MTP_RESPONSE_INVALID_DEVICE_PROP_FORMAT               0x201B
#define MTP_RESPONSE_INVALID_DEVICE_PROP_VALUE                0x201C
#define MTP_RESPONSE_INVALID_PARAMETER                        0x201D
#define MTP_RESPONSE_SESSION_ALREADY_OPEN                     0x201E
#define MTP_RESPONSE_TRANSACTION_CANCELLED                    0x201F
#define MTP_RESPONSE_SPECIFICATION_OF_DESTINATION_UNSUPPORTED 0x2020
#define MTP_RESPONSE_INVALID_OBJECT_PROP_CODE                 0xA801
#define MTP_RESPONSE_INVALID_OBJECT_PROP_FORMAT               0xA802
#define MTP_RESPONSE_INVALID_OBJECT_PROP_VALUE                0xA803
#define MTP_RESPONSE_INVALID_OBJECT_REFERENCE                 0xA804
#define MTP_RESPONSE_GROUP_NOT_SUPPORTED                      0xA805
#define MTP_RESPONSE_INVALID_DATA_SET                         0xA806
#define MTP_RESPONSE_SPECIFICATION_BY_GROUP_UNSUPPORTED       0xA807
#define MTP_RESPONSE_SPECIFICATION_BY_DEPTH_UNSUPPORTED       0xA808
#define MTP_RESPONSE_OBJECT_TOO_LARGE                         0xA809
#define MTP_RESPONSE_OBJECT_PROP_NOT_SUPPORTED                0xA80A

// MTP Event Codes
#define MTP_EVENT_UNDEFINED                 0x4000
#define MTP_EVENT_CANCEL_TRANSACTION        0x4001
#define MTP_EVENT_OBJECT_ADDED              0x4002
#define MTP_EVENT_OBJECT_REMOVED            0x4003
#define MTP_EVENT_STORE_ADDED               0x4004
#define MTP_EVENT_STORE_REMOVED             0x4005
#define MTP_EVENT_DEVICE_PROP_CHANGED       0x4006
#define MTP_EVENT_OBJECT_INFO_CHANGED       0x4007
#define MTP_EVENT_DEVICE_INFO_CHANGED       0x4008
#define MTP_EVENT_REQUEST_OBJECT_TRANSFER   0x4009
#define MTP_EVENT_STORE_FULL                0x400A
#define MTP_EVENT_DEVICE_RESET              0x400B
#define MTP_EVENT_STORAGE_INFO_CHANGED      0x400C
#define MTP_EVENT_CAPTURE_COMPLETE          0x400D
#define MTP_EVENT_UNREPORTED_STATUS         0x400E
#define MTP_EVENT_OBJECT_PROP_CHANGED       0xC801
#define MTP_EVENT_OBJECT_PROP_DESC_CHANGED  0xC802
#define MTP_EVENT_OBJECT_REFERENCES_CHANGED 0xC803

// Buffer Sizes
#define MTP_BUFFER_SIZE       8192
#define MTP_IN_BUFFER_SIZE    (sizeof(struct mtp_packet) + MTP_BUFFER_SIZE)
#define MTP_OUT_BUFFER_SIZE   (sizeof(struct mtp_packet) + MTP_BUFFER_SIZE)
#define MTP_EVENT_BUFFER_SIZE sizeof(struct mtp_packet)

// MTP String Macros
#define MTP_STRINGIFY(S)   #S
#define PACK_ARRAY(D, A)   pack_array(D, ARRAYLEN(A), A, sizeof(A))

typedef void (*mtp_xfer_func) (int, int);

struct mtp_packet {
    uint32_t len;
    uint16_t type;
    uint16_t code;
    uint32_t tsid;
    uint32_t args[5];
} __attribute__((packed));

struct mtp_buffer_io {
    mtp_xfer_func xfer;
    struct mtp_packet req;
    struct {
        struct mtp_packet hdr;
        unsigned char buf[4096];
    } dat;
    struct mtp_packet res;
};

struct mtp_buffer_event {
    mtp_xfer_func xfer;
    struct mtp_packet evt;
};

struct mtp_state {
    struct mtp_buffer_io io;
    struct mtp_buffer_event event;
    uint32_t seid;
};

static int ep_in;
static int ep_out;
static int ep_event;
static int usb_interface;
static int mtp_handle;
static struct mtp_state *mtp;

static struct usb_interface_descriptor __attribute__((aligned(2)))
                                       interface_descriptor =
{
    .bLength            = sizeof(struct usb_interface_descriptor),
    .bDescriptorType    = USB_DT_INTERFACE,
    .bInterfaceNumber   = 0,
    .bAlternateSetting  = 0,
    .bNumEndpoints      = 3,
    .bInterfaceClass    = USB_CLASS_STILL_IMAGE,
    .bInterfaceSubClass = 1,
    .bInterfaceProtocol = 1,
    .iInterface         = USB_STRING_INDEX_MTP,
};

static struct usb_endpoint_descriptor __attribute__((aligned(2)))
                                      endpoint_descriptor =
{
    .bLength          = sizeof(struct usb_endpoint_descriptor),
    .bDescriptorType  = USB_DT_ENDPOINT,
    .bEndpointAddress = 0,
    .bmAttributes     = 0,
    .wMaxPacketSize   = 0,
    .bInterval        = 0
};

static int mtp_verify_request(const struct mtp_packet *req, uint32_t length);
static void mtp_format_response(struct mtp_packet *res, uint16_t code, uint32_t tsid, const uint32_t *args, int len);
static void mtp_xfer_response(int status, int length);
static void mtp_try_send_response(void);
static void mtp_send_response(uint16_t code, uint32_t tsid, const uint32_t *args, int len);
static void mtp_xfer_request(int status, int length);
static void mtp_try_recv_request(void);
static void mtp_recv_request(void);
static void pack_u8(unsigned char **dest, uint8_t val);
static void pack_u16(unsigned char **dest, uint16_t val);
static void pack_u32(unsigned char **dest, uint32_t val);
static void pack_array(unsigned char **dest, size_t len, const void *arr, size_t size);
static void pack_ascii(unsigned char **dest, const char *str);
static void pack_device_info(struct mtp_packet *hdr, unsigned char *buf);
static void mtp_get_device_info(const struct mtp_packet *req, int args_len);
static void mtp_open_session(const struct mtp_packet *req, int args_len);
static void mtp_close_session(const struct mtp_packet *req, int args_len);

static int mtp_verify_request(const struct mtp_packet *req, uint32_t length)
{
    const uint32_t min_size = offsetof(struct mtp_packet, args);
    const uint32_t max_size = sizeof(struct mtp_packet);

    // Verify minimum request size
    if (length < min_size)
        return -1;

    // Verify whole request was received
    if (length != req->len)
        return -1;

    // Verify maximum request size
    if (length > max_size)
        return -1;

    // Verify the request type
    if (req->type != MTP_TYPE_REQUEST)
        return -1;

    const uint32_t args_size = max_size - min_size;
    const uint32_t args_quot = args_size % sizeof(uint32_t);
    const uint32_t args_rem = args_size / sizeof(uint32_t);

    // Verify the arguments array size is a valid multiple
    if (args_rem != 0)
        return -1;

    return args_quot;
}

static void mtp_format_response(struct mtp_packet *res, uint16_t code, uint32_t tsid, const uint32_t *args, int len)
{
    res->len = offsetof(struct mtp_packet, args);
    res->type = MTP_TYPE_RESPONSE;
    res->code = code;
    res->tsid = tsid;

    for (int i = 0; i < len; i++) {
        res->args[i] = args[i];
        res->len += sizeof(uint32_t);
    }
}

static void mtp_xfer_response(int status, int length)
{
    (void) length;

    // Retry if the transfer failed; length has no meaning
    if (status != 0) {
        mtp_try_send_response();
        return;
    }

    // Return to request phase
    mtp_recv_request();
}

static void mtp_try_send_response(void)
{
    struct mtp_buffer_io *io = &mtp->io;
    struct mtp_packet *res = &io->res;

    usb_drv_send_nonblocking(ep_in, res, res->len);
}

static void mtp_send_response(uint16_t code, uint32_t tsid, const uint32_t *args, int len)
{
    struct mtp_buffer_io *io = &mtp->io;
    struct mtp_packet *res = &io->res;

    mtp_format_response(res, code, tsid, args, len);

    io->xfer = mtp_xfer_response;

    mtp_try_send_response();
}

static void mtp_xfer_request(int status, int length)
{
    // Retry if the transfer failed; length has no meaning
    if (status != 0) {
        mtp_try_recv_request();
        return;
    }

    struct mtp_buffer_io *io = &mtp->io;
    const struct mtp_packet *req = &io->req;
    int args_len = mtp_verify_request(req, length);

    // Retry if the packet is malformed
    if (args_len < 0) {
        mtp_try_recv_request();
        return;
    }

    switch (req->code) {
        case MTP_REQUEST_GET_DEVICE_INFO:
            mtp_get_device_info(req, args_len);
            break;

        case MTP_REQUEST_OPEN_SESSION:
            mtp_open_session(req, args_len);
            break;

        case MTP_REQUEST_CLOSE_SESSION:
            mtp_close_session(req, args_len);
            break;

        default:
            mtp_send_response(MTP_RESPONSE_REQUEST_NOT_SUPPORTED, req->tsid, NULL, 0);
            break;
    }
}

static void mtp_try_recv_request(void)
{
    struct mtp_buffer_io *io = &mtp->io;
    struct mtp_packet *req = &io->req;

    usb_drv_recv_nonblocking(ep_out, req, sizeof(*req));
}

static void mtp_recv_request(void)
{
    struct mtp_buffer_io *io = &mtp->io;

    io->xfer = mtp_xfer_request;

    mtp_try_recv_request();
}

static void pack_u8(unsigned char **dest, uint8_t val)
{
    pack_data(dest, &val, sizeof(val));
}

static void pack_u16(unsigned char **dest, uint16_t val)
{
    pack_data(dest, &val, sizeof(val));
}

static void pack_u32(unsigned char **dest, uint32_t val)
{
    pack_data(dest, &val, sizeof(val));
}

static void pack_array(unsigned char **dest, size_t len, const void *arr, size_t size)
{
    pack_u32(dest, len);
    pack_data(dest, arr, size);
}

static void pack_ascii(unsigned char **dest, const char *str)
{
    unsigned char *len = (*dest)++;

    if (*str == '\0') {
        *len = 0;
        return;
    }

    const char *tmp = str;

    while (*str != '\0')
        pack_u16(dest, *str++);

    pack_u16(dest, '\0');

    *len = (str - tmp);
}

static void pack_device_info(struct mtp_packet *hdr, unsigned char *buf)
{
    static const uint16_t requests[] = {
        MTP_REQUEST_GET_DEVICE_INFO,
    };
    static const uint16_t events[] = {};
    static const uint16_t device_props[] = {};
    static const uint16_t capture_formats[] = {};
    static const uint16_t playback_formats[] = {};
    unsigned char *dst = buf;

    // Standard Version
    pack_u16(&dst, 100);

    // MTP Vendor Extension Id
    pack_u32(&dst, 0xffffffff);

    // MTP Version
    pack_u16(&dst, 100);

    // MTP Extensions
    pack_ascii(&dst, "");

    // Functional Mode
    pack_u16(&dst, 0x0000);

    // Requests
    PACK_ARRAY(&dst, requests);

    // Events
    PACK_ARRAY(&dst, events);

    // Device Props
    PACK_ARRAY(&dst, device_props);

    // Capture Formats
    PACK_ARRAY(&dst, capture_formats);

    // Playback Formats
    PACK_ARRAY(&dst, playback_formats);

    // Manufacturer
    pack_ascii(&dst, "");

    // Model
    pack_ascii(&dst, MODEL_NAME);

    // Device Version
    pack_ascii(&dst, rbversion);

    // Serial Number
    pack_ascii(&dst, "");

    // Update packet length
    hdr->len += (dst - buf);
}

static void mtp_get_device_info(const struct mtp_packet *req, int args_len)
{

}

static void mtp_open_session(const struct mtp_packet *req, int args_len)
{
    uint32_t seid;

    if (args_len > 1) {
        mtp_send_response(MTP_RESPONSE_PARAMETER_NOT_SUPPORTED, req->tsid, NULL, 0);
        return;
    }

    if (args_len < 1 || (seid = req->args[0]) == 0) {
        mtp_send_response(MTP_RESPONSE_INVALID_PARAMETER, req->tsid, NULL, 0);
        return;
    }

    if (mtp->seid != 0) {
        mtp_send_response(MTP_RESPONSE_SESSION_ALREADY_OPEN, req->tsid, &mtp->seid, 1);
        return;
    }

    mtp->seid = seid;
    mtp_send_response(MTP_RESPONSE_OK, req->tsid, NULL, 0);
}

static void mtp_close_session(const struct mtp_packet *req, int args_len)
{
    if (args_len > 0) {
        mtp_send_response(MTP_RESPONSE_PARAMETER_NOT_SUPPORTED, req->tsid, NULL, 0);
        return;
    }

    if (mtp->seid == 0) {
        mtp_send_response(MTP_RESPONSE_SESSION_NOT_OPEN, req->tsid, NULL, 0);
        return;
    }

    mtp->seid = 0;
    mtp_send_response(MTP_RESPONSE_OK, req->tsid, NULL, 0);
}

int usb_mtp_request_endpoints(struct usb_class_driver *drv)
{
    ep_in = usb_core_request_endpoint(USB_ENDPOINT_XFER_BULK, USB_DIR_IN, drv);
    if (ep_in < 0)
        goto ep_in_fail;

    ep_out = usb_core_request_endpoint(USB_ENDPOINT_XFER_BULK, USB_DIR_OUT, drv);
    if (ep_out < 0)
        goto ep_out_fail;

    ep_event = usb_core_request_endpoint(USB_ENDPOINT_XFER_INT, USB_DIR_IN, drv);
    if (ep_event < 0)
        goto ep_event_fail;

    return 0;

ep_event_fail:
    usb_core_release_endpoint(ep_out);
ep_out_fail:
    usb_core_release_endpoint(ep_in);
ep_in_fail:
    return -1;
}

int usb_mtp_set_first_interface(int interface)
{
    usb_interface = interface;

    return interface + 1;
}

int usb_mtp_get_config_descriptor(unsigned char *dest, int max_packet_size)
{
    unsigned char *orig_dest = dest;

    interface_descriptor.bInterfaceNumber = usb_interface;
    PACK_DATA(&dest, interface_descriptor);

    endpoint_descriptor.bmAttributes = USB_ENDPOINT_XFER_BULK;
    endpoint_descriptor.wMaxPacketSize = max_packet_size;
    endpoint_descriptor.bInterval = 0;

    endpoint_descriptor.bEndpointAddress = ep_in;
    PACK_DATA(&dest, endpoint_descriptor);

    endpoint_descriptor.bEndpointAddress = ep_out;
    PACK_DATA(&dest, endpoint_descriptor);

    endpoint_descriptor.bmAttributes = USB_ENDPOINT_XFER_INT;
    endpoint_descriptor.wMaxPacketSize = 28;
    endpoint_descriptor.bInterval = 6;

    endpoint_descriptor.bEndpointAddress = ep_event;
    PACK_DATA(&dest, endpoint_descriptor);

    return (dest - orig_dest);
}

void usb_mtp_init_connection(void)
{
    mtp_handle = core_alloc_ex(sizeof(*mtp), &buflib_ops_locked);
    if (mtp_handle < 0)
        panicf("%s(): OOM", __func__);

    mtp = core_get_data(mtp_handle);
    mtp->event.xfer = NULL;
    mtp->seid = 0;

    // Start in request phase
    mtp_recv_request();
}

void usb_mtp_init(void)
{
    return;
}

void usb_mtp_disconnect(void)
{
    mtp_handle = core_free(mtp_handle);
}

void usb_mtp_transfer_complete(int ep, int dir, int status, int length)
{
    (void) dir;

    mtp_xfer_func xfer;

    if (ep == ep_in || ep == ep_out)
        xfer = mtp->io.xfer;
    else if (ep == ep_event)
        xfer = mtp->event.xfer;
    else
        return;

    if (xfer == NULL)
        panicf("%s: no xfer function!", __func__);

    xfer(status, length);
}

bool usb_mtp_control_request(struct usb_ctrlrequest *req, void *reqdata, unsigned char *dest)
{
    (void) req;
    (void) reqdata;
    (void) dest;
    return false;
}

#ifdef HAVE_HOTSWAP
void usb_mtp_notify_hotswap(int volume, bool inserted)
{
    (void) volume;
    (void) inserted;
}
#endif
