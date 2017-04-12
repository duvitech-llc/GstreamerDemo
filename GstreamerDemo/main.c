#include <stdio.h>
#include <gst/gst.h>
#include "config.h"
#include "gsttest.h"

int
main(int   argc,
	char *argv[])
{
	GstElement *pipeline;
	GstBus *bus;
	GstMessage *msg;
	const gchar *nano_str;
	guint major, minor, micro, nano;

	gst_init(&argc, &argv);

	gst_version(&major, &minor, &micro, &nano);

	if (nano == 1)
		nano_str = "(CVS)";
	else if (nano == 2)
		nano_str = "(Prerelease)";
	else
		nano_str = "";

	printf("This program is linked against GStreamer %d.%d.%d %s\n",
		major, minor, micro, nano_str);
	
	/* register my plugin */
	if (!gst_plugin_register_static( 
		GST_VERSION_MAJOR,
		GST_VERSION_MINOR,
		"test",
		"Template test",
		test_init,
		VERSION,
		"LGPL",
		"GStreamer",
		"GStreamer-app",
		"http://gstreamer.net/")) {

			printf("Failed to register custom plugin\n");
	}

	/* Build the pipeline */
	pipeline = gst_parse_launch("videotestsrc ! test silent=false ! autovideosink ", NULL);

	/* Start playing */
	gst_element_set_state(pipeline, GST_STATE_PLAYING);

	/* Wait until error or EOS */
	bus = gst_element_get_bus(pipeline);
	msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

	/* Free resources */
	if (msg != NULL)
		gst_message_unref(msg);
	gst_object_unref(bus);
	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(pipeline);
	return 0;

	return 0;
}