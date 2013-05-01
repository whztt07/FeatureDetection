/*
 * KinectImageSource.cpp
 *
 *  Created on: 07.10.2012
 *      Author: Patrik Huber
 */

#include "imageio/KinectImageSource.hpp"
#include <iostream>

namespace imageio {

KinectImageSource::KinectImageSource(int device) {

#ifdef WIN32
	m_pColorStreamHandle = INVALID_HANDLE_VALUE;
	/// Create the first connected Kinect found
	INuiSensor * pNuiSensor;
	HRESULT hr;

	int iSensorCount = 0;
	hr = NuiGetSensorCount(&iSensorCount);
	if (FAILED(hr))
	{
		std::cout << "Error getting sensor count. No Kinect plugged in?" << hr << std::endl;
	}

	// Look at each Kinect sensor
	for (int i = 0; i < iSensorCount; ++i)
	{
		// Create the sensor so we can check status, if we can't create it, move on to the next
		hr = NuiCreateSensorByIndex(i, &pNuiSensor);
		if (FAILED(hr))
		{
			continue;
		}

		// Get the status of the sensor, and if connected, then we can initialize it
		hr = pNuiSensor->NuiStatus();
		if (S_OK == hr)
		{
			m_pNuiSensor = pNuiSensor;
			break;
		}

		// This sensor wasn't OK, so release it since we're not using it
		pNuiSensor->Release();
	}

	if (NULL != m_pNuiSensor)
	{
		// Initialize the Kinect and specify that we'll be using color
		hr = m_pNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_COLOR); 
		if (SUCCEEDED(hr))
		{
			// Create an event that will be signaled when color data is available
			//m_hNextColorFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

			// Open a color image stream to receive color frames
			hr = m_pNuiSensor->NuiImageStreamOpen(
				NUI_IMAGE_TYPE_COLOR,
				NUI_IMAGE_RESOLUTION_640x480,
				0,
				2,
				NULL,
				&m_pColorStreamHandle);
		}
	}

	if (NULL == m_pNuiSensor || FAILED(hr))
	{
		std::cout << "No ready Kinect found!" << std::endl;
	}

	//std::cout << "hr: " << hr << std::endl;

#else
	std::cerr << "Error! This is the Microsoft Kinect SDK interface and not available under Linux." << std::endl;
#endif

}


KinectImageSource::~KinectImageSource() {
#ifdef WIN32
	if (m_pNuiSensor)
	{
		m_pNuiSensor->NuiShutdown();
	}
	if ( m_pNuiSensor != NULL )
	{
		m_pNuiSensor->Release();
		m_pNuiSensor = NULL;
	}
#endif
}

const cv::Mat KinectImageSource::get() {

#ifdef WIN32
	// Attempt to get the color frame
	HRESULT hr;
	hr = m_pNuiSensor->NuiImageStreamGetNextFrame(m_pColorStreamHandle, 0, &imageFrame);

	if (FAILED(hr))
	{
		//return frame;	// Hmm, really? We've failed. return frame then?
		return Mat();
	}

	INuiFrameTexture * pTexture = imageFrame.pFrameTexture;
	NUI_LOCKED_RECT LockedRect;

	// Lock the frame data so the Kinect knows not to modify it while we're reading it
	pTexture->LockRect(0, &LockedRect, NULL, 0);

	// Make sure we've received valid data
	if (LockedRect.Pitch != 0)
	{
		// Draw the data with Direct2D
		//m_pDrawColor->Draw(static_cast<BYTE *>(LockedRect.pBits), LockedRect.size);
		frame = Mat(480, 640, CV_8UC4, static_cast<BYTE *>(LockedRect.pBits));

		// Write out the bitmap to disk
		//static_cast<BYTE *>(LockedRect.pBits), cColorWidth, cColorHeight, 32
		//BYTE* pBitmapBits,                     LONG lWidth, LONG lHeight, WORD wBitsPerPixel
		/// <param name="pBitmapBits">image data to save</param>
		/// <param name="lWidth">width (in pixels) of input image data</param>
		/// <param name="lHeight">height (in pixels) of input image data</param>
		/// <param name="wBitsPerPixel">bits per pixel of image data</param>

		/// Standard RGB, no compression
	}

	// We're done with the texture so unlock it
	pTexture->UnlockRect(0);

	// Release the frame
	m_pNuiSensor->NuiImageStreamReleaseFrame(m_pColorStreamHandle, &imageFrame);

	return frame;
#else
	std::cerr << "Error! This is the Microsoft Kinect SDK interface and not available under Linux." << std::endl;
	return frame;
#endif
}

const bool KinectImageSource::next()
{
#ifdef WIN32
	return true;	// There should always be a next frame in the Kinect. If not, get() will fail.
#else
	std::cerr << "Error! This is the Microsoft Kinect SDK interface and not available under Linux." << std::endl;
	return false;
#endif
}

const Mat KinectImageSource::getImage()
{
#ifdef WIN32
	return frame;	// TODO: What about the very first frame? We should initialize frame in the constructor.
					// TODO: This is currently flawed. A call to next() with a subsequent call to getImage()
					//       should return the next frame, not always the same!
#else
	std::cerr << "Error! This is the Microsoft Kinect SDK interface and not available under Linux." << std::endl;
	return Mat();
#endif
}

const path KinectImageSource::getName()
{
#ifdef WIN32
	return path();
#else
	std::cerr << "Error! This is the Microsoft Kinect SDK interface and not available under Linux." << std::endl;
	return path();
#endif
}

const vector<path> KinectImageSource::getNames()
{
#ifdef WIN32
	return vector<path>();
#else
	std::cerr << "Error! This is the Microsoft Kinect SDK interface and not available under Linux." << std::endl;
	return vector<path>();
#endif
}

} /* namespace imageio */
