/*	
 * ImageSource.hpp
 *
 *  Created on: 20.08.2012
 *      Author: poschmann, Patrik Huber
 */

#ifndef IMAGESOURCE_HPP_
#define IMAGESOURCE_HPP_

#include "opencv2/core/core.hpp"
#ifdef WIN32
	#define BOOST_ALL_DYN_LINK	// Link against the dynamic boost lib. Seems to be necessary because we use /MD, i.e. link to the dynamic CRT.
	#define BOOST_ALL_NO_LIB	// Don't use the automatic library linking by boost with VS2010 (#pragma ...). Instead, we specify everything in cmake.
#endif
#include "boost/filesystem.hpp"
#include <vector>

using cv::Mat;
using boost::filesystem::path;
using std::vector;

namespace imageio {

/**
 * Source of subsequent images.
 */
class ImageSource {
public:

	virtual ~ImageSource() {}

	/**
	 * Moves the image source forward to the next image.
	 *
	 * @return True if the image source contains a next image, false otherwise.
	 */
	virtual const bool next() = 0;

	/**
	 * Retrieves the current image and moves the image source forward to
	 * the next image.
	 *
	 * @return The image (that may be empty if no data could be retrieved).
	 */
	virtual const Mat get() = 0;

	/**
	 * Retrieves the current image.
	 *
	 * @return The image (that may be empty if no data could be retrieved).
	 */
	virtual const Mat getImage() = 0;

	/**
	 * Retrieves the name of the current image. That could be the
	 * path to the image or the current frame number.
	 *
	 * @return The name of the current image (that may be empty if no data could be retrieved).
	 */
	virtual const path getName() = 0;

	/**
	 * Retrieves the list of image names currently in the image source.
	 *
	 * @return The image (that may be empty if no data could be retrieved)...
	 */
	virtual const vector<path> getNames() = 0;

};

} /* namespace imageio */
#endif /* IMAGESOURCE_HPP_ */
