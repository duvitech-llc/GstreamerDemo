/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2016 George Vigelette <<user@hostname.org>>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-test
 *
 * FIXME:Describe test here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! test ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */



#include <gst/gst.h>
#include <stdint.h>
#include <string.h>

#  include "config.h"
#include "gsttest.h"

#define NETWORK_ERROR   (-1)
#define NETWORK_INVALID_HEADER  (-2)
#define MAX_BUF_SIZE    (32)

GST_DEBUG_CATEGORY_STATIC (gst_test_debug);
#define GST_CAT_DEFAULT gst_test_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT
};

typedef struct {

    unsigned int payloadType;

    guint8 *frameData;
    guint8 *pFrameBuffer;

    guint8 *dataHeaderBuf;
    guint8 *pBuffer;

    NetworkRx_CmdHeader cmdHeader;
    unsigned int  headerBufferCount;

    unsigned long long  frameDataCount;
    unsigned long long totalDataSize;
    
    gboolean bReceiveHeader;
    gboolean bReceiveFrame;
    gboolean bFault;
} NetworkRx_Obj;

NetworkRx_Obj gNetworkRx_obj;

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

#define gst_test_parent_class parent_class
G_DEFINE_TYPE (Gsttest, gst_test, GST_TYPE_ELEMENT);

static void gst_test_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_test_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_test_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_test_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);

static GstClockTime timestamp = 0;

/* GObject vmethod implementations */

/* initialize the test's class */
static void
gst_test_class_init (GsttestClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_test_set_property;
  gobject_class->get_property = gst_test_get_property;


  memset(&gNetworkRx_obj, 0, sizeof(gNetworkRx_obj));
  gNetworkRx_obj.dataHeaderBuf = g_malloc(MAX_BUF_SIZE);
  gNetworkRx_obj.frameData = g_malloc(480*640*2);
  if(gNetworkRx_obj.dataHeaderBuf==NULL)
  {
      g_print("# ERROR: Unable to allocate memory for buffer !!! \n");
    gNetworkRx_obj.bFault = TRUE;
  }else{
    // set pointer
    gNetworkRx_obj.payloadType = 0; // RAW only type currently supported
    gNetworkRx_obj.pBuffer = gNetworkRx_obj.dataHeaderBuf;
    gNetworkRx_obj.pFrameBuffer = gNetworkRx_obj.frameData;
    gNetworkRx_obj.frameDataCount = 0;
    gNetworkRx_obj.totalDataSize = 0;
    gNetworkRx_obj.headerBufferCount = 0;
    gNetworkRx_obj.bReceiveHeader = TRUE;
    gNetworkRx_obj.bReceiveFrame = FALSE;
    gNetworkRx_obj.bFault = FALSE;
  }

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  gst_element_class_set_details_simple(gstelement_class,
    "test",
    "FIXME:Generic",
    "FIXME:Generic Template Element",
    "George Vigelette <<gvigelet@duvitech.com>>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_test_init (Gsttest * filter)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_test_sink_event));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_test_chain));
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->silent = FALSE;
}

static void
gst_test_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gsttest *filter = GST_TEST (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_test_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gsttest *filter = GST_TEST (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
gst_test_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  Gsttest *filter;
  gboolean ret;

  filter = GST_TEST (parent);

  GST_LOG_OBJECT (filter, "Received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps * caps;

      gst_event_parse_caps (event, &caps);
      /* do something with the caps */

      /* and forward */
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

static uint32_t rec_header_data(NetworkRx_Obj *pReceiveObj, uint8_t* pData, uint32_t size){
  uint32_t headerDataRemaining = 0x20 - pReceiveObj->headerBufferCount;

  if(size >= headerDataRemaining){
    // possible header copy data to header struct
    memcpy(pReceiveObj->pBuffer , pData, headerDataRemaining);
    memcpy((uint8_t*)&pReceiveObj->cmdHeader, pReceiveObj->dataHeaderBuf, 0x20);

    // g_print("Header MagicNUM: 0x%4x\n",pReceiveObj->cmdHeader.header);
    if(pReceiveObj->cmdHeader.header != NETWORK_TX_HEADER){
      g_print("INVALID TX HEADER !!!\n");     
    }else{    
      pReceiveObj->headerBufferCount += headerDataRemaining;
      pReceiveObj->bReceiveHeader = FALSE;
      pReceiveObj->pFrameBuffer = pReceiveObj->frameData;
      pReceiveObj->frameDataCount = 0;

      return headerDataRemaining;
    }
  }else{
    memcpy(pReceiveObj->pBuffer , pData, size);
    pReceiveObj->pBuffer += size;
    pReceiveObj->headerBufferCount += size;

    return size;
  }	

  return 0;
}

static uint32_t rec_frame_data(NetworkRx_Obj *pNetworkRecObj, uint8_t* pData, GstPad *srcpad, uint32_t size){
      uint32_t remainingFrameBytes = 0;
      GstBuffer * buffer;
      remainingFrameBytes = pNetworkRecObj->cmdHeader.dataSize - pNetworkRecObj->frameDataCount;

      if(size <= remainingFrameBytes){
        
        memcpy(pNetworkRecObj->pFrameBuffer , pData, size);
        pNetworkRecObj->frameDataCount += size;
        pNetworkRecObj->pFrameBuffer += size;
        return size;
      }else{
        memcpy(pNetworkRecObj->pFrameBuffer , pData, remainingFrameBytes);
        pNetworkRecObj->frameDataCount += remainingFrameBytes;
        pNetworkRecObj->pFrameBuffer += remainingFrameBytes;
        buffer = gst_buffer_new_wrapped_full( 0, (gpointer)pNetworkRecObj->frameData, pNetworkRecObj->frameDataCount, 0, pNetworkRecObj->frameDataCount, NULL, NULL );
        
        GST_BUFFER_PTS (buffer) = timestamp;
        GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, 2);
        timestamp += GST_BUFFER_DURATION (buffer);

        gst_pad_push (srcpad, buffer);
        
        // g_print ("Frame Finished Received: %llu of FrameSize: %u.\n", pNetworkRecObj->frameDataCount, pNetworkRecObj->cmdHeader.dataSize);    
        pNetworkRecObj->headerBufferCount = 0;
        pNetworkRecObj->pBuffer = pNetworkRecObj->dataHeaderBuf;
        pNetworkRecObj->pFrameBuffer = pNetworkRecObj->frameData;
        pNetworkRecObj->frameDataCount = 0;
        pNetworkRecObj->totalDataSize = 0;
        pNetworkRecObj->headerBufferCount = 0;
        pNetworkRecObj->bReceiveHeader = TRUE;
        pNetworkRecObj->bReceiveFrame = FALSE;
    
        return remainingFrameBytes;
      }

}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_test_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  Gsttest *filter;
  GstMapInfo map;
  uint32_t bytesProcessed;
  uint32_t ret;
  uint32_t dataSize;
  GstFlowReturn retVal;

  bytesProcessed = 0;
  filter = GST_TEST (parent);

  GST_DEBUG ("Parsing TDA Data");

  
  if (!gst_buffer_map (buf, &map, GST_MAP_READ)){
    GST_WARNING_OBJECT (parent, "Could not map buffer, skipping");
    return GST_FLOW_OK;
  }

  if (filter->silent == FALSE)
    g_print ("I'm plugged, therefore I'm in.\n");


  // check for object init fault and just pass through  
  if(gNetworkRx_obj.bFault){
    g_print("RECEIVE OBJECT FAULT !!! PASS-THROUGH !!!\n");
    return gst_pad_push (filter->srcpad, buf);
  } 

  uint8_t * pDataBuffer = map.data;
  // g_print ("Data Size %lu\n", map.size);
  dataSize = map.size;

  do{
    if(gNetworkRx_obj.bReceiveHeader){
      // g_print("Processing Header Data !!!\n");
      ret = rec_header_data(&gNetworkRx_obj, pDataBuffer,dataSize);
      if(ret == 0){
        g_print("RECEIVE HEADER DATA ERROR !!!\n");
        retVal = GST_FLOW_ERROR;
        break;
      }
    }else{
      // g_print("DEBUG:\t\tframeDataCount: %llu \n\t\tTotalSize: %u\n\t\tdataSize: %u\n", gNetworkRx_obj.frameDataCount, gNetworkRx_obj.cmdHeader.dataSize, dataSize);
      ret = rec_frame_data(&gNetworkRx_obj, pDataBuffer, filter->srcpad, dataSize);
    }
    
    pDataBuffer += ret;
    bytesProcessed += ret;
    dataSize -= ret;
    // g_print("Bytes Processed: %u of Data Size: %lu \n", bytesProcessed, map.size);
    retVal = GST_FLOW_OK;
  }while(bytesProcessed<map.size);

  /* just push out the incoming buffer without touching it */
  return retVal;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
gboolean
test_init (GstPlugin * test)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template test' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_test_debug, "test",
      0, "Template test");

  return gst_element_register (test, "test", GST_RANK_NONE,
      GST_TYPE_TEST);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirsttest"
#endif

/* gstreamer looks for this structure to register tests
 *
 * exchange the string 'Template test' with your test description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    test,
    "Template test",
    test_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
