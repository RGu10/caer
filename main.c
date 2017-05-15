/*
 * main.c
 *
 *  Created on: Oct 6, 2013
 *      Author: chtekk
 *
 *  Compile & run:
 *  $ cd caer/
 *  $ rm -rf CMakeFiles CMakeCache.txt
 *  $ CC=clang-3.7 cmake [-DJAER_COMPAT_FORMAT=1 -DENABLE_VISUALIZER=1 -DENABLE_NET_STREAM=1] -DDAVISFX2 .
 *  $ make
 *  $ ./caer-bin
 */

#include "main.h"
#include "base/config.h"
#include "base/config_server.h"
#include "base/log.h"
#include "base/mainloop.h"
#include "base/misc.h"

// Devices support.
#ifdef DVS128
#include "modules/ini/dvs128.h"
#endif
#ifdef DAVISFX2
#include "modules/ini/davis_fx2.h"
#endif
#ifdef DAVISFX3
#include "modules/ini/davis_fx3.h"
#endif

// Input/Output support.
#ifdef ENABLE_FILE_INPUT
#include "modules/misc/in/file.h"
#endif
#ifdef ENABLE_NETWORK_INPUT
#include "modules/misc/in/net_tcp.h"
#include "modules/misc/in/unix_socket.h"
#endif

#ifdef ENABLE_FILE_OUTPUT
#include "modules/misc/out/file.h"
#endif
#ifdef ENABLE_NETWORK_OUTPUT
#include "modules/misc/out/net_tcp_server.h"
#include "modules/misc/out/net_tcp.h"
#include "modules/misc/out/net_udp.h"
#include "modules/misc/out/unix_socket_server.h"
#include "modules/misc/out/unix_socket.h"
#endif

// Common filters support.
#ifdef ENABLE_BAFILTER
#include "modules/backgroundactivityfilter/backgroundactivityfilter.h"
#endif
#ifdef ENABLE_CAMERACALIBRATION
#include "modules/cameracalibration/cameracalibration.h"
#endif
#ifdef ENABLE_FRAMEENHANCER
#include "modules/frameenhancer/frameenhancer.h"
#endif
#ifdef ENABLE_STATISTICS
#include "modules/statistics/statistics.h"
#endif
#ifdef ENABLE_VISUALIZER
#include "modules/visualizer/visualizer.h"
#endif

#ifdef ENABLE_IMAGEGENERATOR
#include "modules/imagegenerator/imagegenerator.h"
#define CLASSIFYSIZE 64
#define DISPLAYIMGSIZE 256
#endif
#ifdef ENABLE_CAFFEINTERFACE
#include "modules/caffeinterface/wrapper.h"
#endif

static bool mainloop_1(void);

static bool mainloop_1(void) {
	// An eventPacketContainer bundles event packets of different types together,
	// to maintain time-coherence between the different events.
	caerEventPacketContainer container = NULL;
	caerSpecialEventPacket special = NULL;
	caerPolarityEventPacket polarity = NULL;

#ifdef ENABLE_VISUALIZER
	caerVisualizerEventHandler visualizerEventHandler = NULL;
#endif

	// Input modules grab data from outside sources (like devices, files, ...)
	// and put events into an event packet.
#ifdef DVS128
	container = caerInputDVS128(1);

	// Typed EventPackets contain events of a certain type.
	special = (caerSpecialEventPacket) caerEventPacketContainerGetEventPacket(container, SPECIAL_EVENT);
	polarity = (caerPolarityEventPacket) caerEventPacketContainerGetEventPacket(container, POLARITY_EVENT);
#endif

#ifdef DAVISFX2
	container = caerInputDAVISFX2(1);
#endif
#ifdef DAVISFX3
	container = caerInputDAVISFX3(1);
#endif
#if defined(DAVISFX2) || defined(DAVISFX3)
	// Typed EventPackets contain events of a certain type.
	special = (caerSpecialEventPacket) caerEventPacketContainerGetEventPacket(container, SPECIAL_EVENT);
	polarity = (caerPolarityEventPacket) caerEventPacketContainerGetEventPacket(container, POLARITY_EVENT);

	// Frame and IMU events exist only with DAVIS cameras.
	caerFrameEventPacket frame = NULL;
	caerIMU6EventPacket imu = NULL;

	frame = (caerFrameEventPacket) caerEventPacketContainerGetEventPacket(container, FRAME_EVENT);
	imu = (caerIMU6EventPacket) caerEventPacketContainerGetEventPacket(container, IMU6_EVENT);
#endif

#ifdef ENABLE_FILE_INPUT
	container = caerInputFile(10);
#endif
#ifdef ENABLE_NETWORK_INPUT
	container = caerInputNetTCP(11);
#endif
#if defined(ENABLE_FILE_INPUT) || defined(ENABLE_NETWORK_INPUT)
#ifdef ENABLE_VISUALIZER
	visualizerEventHandler = &caerInputVisualizerEventHandler;
#endif

	// Typed EventPackets contain events of a certain type.
	// We search for them by type here, because input modules may not have all or any of them.
	special = (caerSpecialEventPacket) caerEventPacketContainerFindEventPacketByType(container, SPECIAL_EVENT);
	polarity = (caerPolarityEventPacket) caerEventPacketContainerFindEventPacketByType(container, POLARITY_EVENT);

	caerFrameEventPacket frame = NULL;
	caerIMU6EventPacket imu = NULL;

	frame = (caerFrameEventPacket) caerEventPacketContainerFindEventPacketByType(container, FRAME_EVENT);
	imu = (caerIMU6EventPacket) caerEventPacketContainerFindEventPacketByType(container, IMU6_EVENT);
#endif

	// Filters process event packets: for example to suppress certain events,
	// like with the Background Activity Filter, which suppresses events that
	// look to be uncorrelated with real scene changes (noise reduction).
#ifdef ENABLE_BAFILTER
	caerBackgroundActivityFilter(2, polarity);
#endif

	// Filters can also extract information from event packets: for example
	// to show statistics about the current event-rate.
#ifdef ENABLE_STATISTICS
	caerStatistics(3, (caerEventPacketHeader) polarity, 1000);
#endif

	// Enable APS frame image enhancements.
#ifdef ENABLE_FRAMEENHANCER
	frame = caerFrameEnhancer(4, frame);
#endif

	// Enable image and event undistortion by using OpenCV camera calibration.
#ifdef ENABLE_CAMERACALIBRATION
	caerCameraCalibration(5, polarity, frame);
#endif

	//Enable camera pose estimation
#ifdef ENABLE_POSEESTIMATION
	caerPoseCalibration(6, polarity, frame);
#endif

	// A simple visualizer exists to show what the output looks like.
#ifdef ENABLE_VISUALIZER
	caerVisualizer(60, "Polarity", &caerVisualizerRendererPolarityEvents, visualizerEventHandler, (caerEventPacketHeader) polarity);
	caerVisualizer(61, "Frame", &caerVisualizerRendererFrameEvents, visualizerEventHandler, (caerEventPacketHeader) frame);
	caerVisualizer(62, "IMU6", &caerVisualizerRendererIMU6Events, visualizerEventHandler, (caerEventPacketHeader) imu);
	//caerVisualizerMulti(68, "PolarityAndFrame", &caerVisualizerMultiRendererPolarityAndFrameEvents, visualizerEventHandler, container);
#endif

#ifdef ENABLE_IMAGEGENERATOR
	// save images of accumulated spikes and frames
	char mainString[16] = "_main.c_";


	int * classifyhist = calloc((int)CLASSIFYSIZE * CLASSIFYSIZE, sizeof(int));
	if (classifyhist == NULL) {
			return (false); // Failure.
	}
	bool *haveimage;
	haveimage = (bool*)malloc(1);

#if defined(DAVISFX2) || defined(DAVISFX3) || defined(ENABLE_FILE_INPUT) || defined(ENABLE_NETWORK_INPUT) 
	unsigned char ** frame_img_ptr = calloc(sizeof(unsigned char *), 1);
	// generate images
	caerImageGenerator(20, polarity, CLASSIFYSIZE, classifyhist, haveimage);
#endif
#endif

#ifdef ENABLE_NULLHOPINTERFACE || ENABLE_CAFFEINTERFACE
	// this also requires image generator
#ifdef ENABLE_IMAGEGENERATOR
	// this wrapper let you interact with NullHop
	// for example, we now classify the latest image
	// only run CNN if we have a file to classify

	int *results;
	results  = (int*)calloc(1, sizeof(int));
	results[0] = NULL;
#endif
#endif

#ifdef ENABLE_CAFFEINTERFACE
	// this also requires image generator
#ifdef ENABLE_IMAGEGENERATOR
	caerCaffeWrapper(23, classifyhist, haveimage, results, CLASSIFYSIZE);
#endif
#endif

#ifdef ENABLE_NULLHOPINTERFACE
	// this also requires image generator
#ifdef ENABLE_IMAGEGENERATOR
	caerNullHopWrapper(22, classifyhist, haveimage, results);
#endif
#endif

#ifdef ENABLE_NULLHOPINTERFACE
#ifdef ENABLE_ARDUINOCNT
	caerArduinoCNT(24, results, haveimage);
#endif
#endif

#ifdef ENABLE_NETWORK_OUTPUT
	// Send polarity packets out via TCP. This is the server mode!
	// External clients connect to cAER, and we send them the data.
	// WARNING: slow clients can dramatically slow this and the whole
	// processing pipeline down!
	caerOutputNetTCPServer(8, 1, polarity);

	// And also send them via UDP. This is fast, as it doesn't care what is on the other side.
	//caerOutputNetUDP(9, 1, polarity); //, frame, imu, special);
#endif
#ifdef ENABLE_FILE_OUTPUT
	// Enable output to file (AEDAT 3.X format).
	caerOutputFile(7, 4, polarity, frame, imu, special);
#endif

#ifdef ENABLE_NULLHOPINTERFACE
	// this also requires image generator
#ifdef ENABLE_IMAGEGENERATOR
	free(results);
#endif
#endif

#ifdef ENABLE_IMAGEGENERATOR
	free(classifyhist);
	free(haveimage);
#endif

	return (true); // If false is returned, processing of this loop stops.
}

int main(int argc, char **argv) {
	// Set thread name.
	thrd_set_name("Main");

	// Initialize config storage from file, support command-line overrides.
	// If no init from file needed, pass NULL.
	caerConfigInit("caer-config.xml", argc, argv);

	// Initialize logging sub-system.
	caerLogInit();

	// Daemonize the application (run in background).
	//caerDaemonize();

	// Initialize visualizer framework (load fonts etc.).
#ifdef ENABLE_VISUALIZER
	caerVisualizerSystemInit();
#endif

	// Start the configuration server thread for run-time config changes.
	caerConfigServerStart();

	// Finally run the main event processing loops.
	struct caer_mainloop_definition mainLoops[1] = { { 1, &mainloop_1 } };
	caerMainloopRun(&mainLoops, 1); // Only start Mainloop 1.

	// After shutting down the mainloops, also shutdown the config server
	// thread if needed.
	caerConfigServerStop();

	return (EXIT_SUCCESS);
}
