/*
 * superviseddescent.hpp.hpp
 *
 *  Created on: 14.11.2014
 *      Author: Patrik Huber
 */

#pragma once

#ifndef SUPERVISEDDESCENT_HPP_
#define SUPERVISEDDESCENT_HPP_

#include "logging/LoggerFactory.hpp"

#include "opencv2/core/core.hpp"
#include "Eigen/Dense"

#include "boost/lexical_cast.hpp"

#include <memory>
#include <chrono>

using logging::Logger; // Todo: Move to cpp as soon as finished
using logging::LoggerFactory;
using cv::Mat;
using boost::lexical_cast;
using std::string;

namespace superviseddescent {
	namespace v2 {

/**
 * Stuff for supervised descent v2
 * Will be moved to separate files / cpp files once finished
 */

class Regressor
{
public:
	virtual ~Regressor() {};

	// or train()
	// Returns whether the learning was successful.
	// or maybe the prediction error? => no, for that case, we can use test() with the same data.
	virtual bool learn(cv::Mat data, cv::Mat labels) = 0;

	// maybe, for testing:
	// return a results struct or something, like in tiny-cnn
	// Or atm the normalised LS residual
	virtual double test(cv::Mat data, cv::Mat labels) = 0;

	virtual cv::Mat predict(cv::Mat values) = 0;

	// overload for float, let's see if we can use it in this way
	virtual float predict(float value) = 0;
};

class Regulariser
{
public:
	/**
	* Todo: Description.
	*/
	enum class RegularisationType
	{
		Manual, ///< use lambda
		MatrixNorm, ///< use norm... optional lambda used as factor. If used, a suitable default is 0.5f.As mentioned by the authors of [SDM].
		EigenvalueThreshold ///< see description libEigen
	};

	Regulariser(RegularisationType regularisationType = RegularisationType::Manual, float param = 0.0f, bool regulariseLastRow = true) : regularisationType(regularisationType), lambda(param), regulariseLastRow(regulariseLastRow)
	{
	};

	// Returns a diagonal regularisation matrix with the same dimensions as the given data matrix.
	// Might use the data to calculate an automatic value for lambda.
	cv::Mat getMatrix(cv::Mat data, int numTrainingElements)
	{
		auto logger = Loggers->getLogger("superviseddescent");
		// move regularisation to a separate function, so we can also test it separately. getRegulariser()?
		// Actually all that stuff can go in a class Regulariser
		switch (regularisationType)
		{
		case RegularisationType::Manual:
			// We just take lambda as it was given, no calculation necessary.
			break;
		case RegularisationType::MatrixNorm:
			// The given lambda is the factor we have to multiply the automatic value with
			lambda = lambda * static_cast<float>(cv::norm(data)) / static_cast<float>(numTrainingElements); // We divide by the number of images.
			// However, division by (AtA.rows * AtA.cols) might make more sense? Because this would be an approximation for the
			// RMS (eigenvalue? see sheet of paper, ev's of diag-matrix etc.), and thus our (conservative?) guess for a lambda that makes AtA invertible.
			break;
		case RegularisationType::EigenvalueThreshold:
			//lambda = calculateEigenvalueThreshold(AtA);
			lambda = 0.5f; // TODO COPY FROM OLD CODE
			break;
		default:
			break;
		}
		logger.debug("Lambda is: " + lexical_cast<string>(lambda));

		Mat regulariser = Mat::eye(data.rows, data.cols, CV_32FC1) * lambda; // always symmetric

		if (!regulariseLastRow) {
			regulariser.at<float>(regulariser.rows - 1, regulariser.cols - 1) = 0.0f; // no lambda for the bias
		}
		return regulariser;
	};
private:
	RegularisationType regularisationType;
	float lambda; // param for RegularisationType. Can be lambda directly or a factor with which the lambda from MatrixNorm will be multiplied with.
	bool regulariseLastRow; // if your last row of data matrix is a bias (offset), then you might want to choose whether it should be regularised as well. Otherwise, just leave it to default (true).
};

class LinearRegressor : public Regressor
{

public:
	// The default should be the simplest case: Do not regularisation at all.
	// regulariseBias assumes the last row (col?) that will be given as training is a row to learn a bias b (affine component)
	// ==> If anywhere, learnBias should go to SDO class. Nothing changes in the regressor.
	// If you want to learn a bias term, add it as a column with [1; 1; ...; 1] to the data
	// A few of those params could go to the learn function?
	LinearRegressor(Regulariser regulariser = Regulariser()) : x(), regulariser(regulariser)
	{
	};

	// Returns if the learning was successful or not. I.e. in this case, if the matrix was invertible.
	bool learn(cv::Mat data, cv::Mat labels)
	{
		Logger logger = Loggers->getLogger("superviseddescent");
		std::chrono::time_point<std::chrono::system_clock> start, end;
		int elapsed_mseconds;

		Mat AtA = data.t() * data;

		Mat regularisationMatrix = regulariser.getMatrix(AtA, data.rows);
		
		// solve for x!
		Mat AtAReg = AtA + regularisationMatrix;
		if (!AtAReg.isContinuous()) {
			std::string msg("Matrix is not continuous. This should not happen as we allocate it directly.");
			logger.error(msg);
			throw std::runtime_error(msg);
		}
		Eigen::Map<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> AtAReg_Eigen(AtAReg.ptr<float>(), AtAReg.rows, AtAReg.cols);
		std::chrono::time_point<std::chrono::system_clock> inverseTimeStart = std::chrono::system_clock::now();
		// Calculate the full-pivoting LU decomposition of the regularized AtA. Note: We could also try FullPivHouseholderQR if our system is non-minimal (i.e. there are more constraints than unknowns).
		Eigen::FullPivLU<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> luOfAtAReg(AtAReg_Eigen);
		// we could also print the smallest eigenvalue here, but that would take time to calculate (SelfAdjointEigenSolver, see above)
		float rankOfAtAReg = luOfAtAReg.rank(); // Todo: Use auto?
		logger.trace("Rank of the regularized AtA: " + lexical_cast<string>(rankOfAtAReg));
		if (luOfAtAReg.isInvertible()) {
			logger.debug("The regularized AtA is invertible.");
		}
		else {
			// Eigen will most likely return garbage here (according to their docu anyway). We have a few options:
			// - Increase lambda
			// - Calculate the pseudo-inverse. See: http://eigen.tuxfamily.org/index.php?title=FAQ#Is_there_a_method_to_compute_the_.28Moore-Penrose.29_pseudo_inverse_.3F
			string msg("The regularised AtA is not invertible. We continued learning, but Eigen calculates garbage in this case according to their documentation. (The rank is " + lexical_cast<string>(rankOfAtAReg)+", full rank would be " + lexical_cast<string>(AtAReg_Eigen.rows()) + "). Increase lambda (or use the pseudo-inverse, which is not implemented yet).");
			logger.error(msg);
		}
		Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> AtARegInv_EigenFullLU = luOfAtAReg.inverse();
		//Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> AtARegInv_Eigen = AtAReg_Eigen.inverse(); // This would be the cheap variant (PartialPivotLU), but we can't check if the matrix is invertible.
		Mat AtARegInvFullLU(AtARegInv_EigenFullLU.rows(), AtARegInv_EigenFullLU.cols(), CV_32FC1, AtARegInv_EigenFullLU.data()); // create an OpenCV Mat header for the Eigen data
		std::chrono::time_point<std::chrono::system_clock> inverseTimeEnd = std::chrono::system_clock::now();
		elapsed_mseconds = std::chrono::duration_cast<std::chrono::milliseconds>(inverseTimeEnd - inverseTimeStart).count();
		logger.debug("Inverting the regularized AtA took " + lexical_cast<string>(elapsed_mseconds) + "ms.");
		//Mat AtARegInvOCV = AtAReg.inv(); // slow OpenCV inv() for comparison

		// Todo(1): Moving AtA by lambda should move the eigenvalues by lambda, however, it does not. It did however on an early test (with rand maybe?).
		//Eigen::SelfAdjointEigenSolver<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> es(AtAReg_Eigen);
		//std::cout << es.eigenvalues() << std::endl;

		Mat AtARegInvAt = AtARegInvFullLU * data.t(); // Todo: We could use luOfAtAReg.solve(b, x) instead of .inverse() and these lines.
		Mat AtARegInvAtb = AtARegInvAt * labels; // = x
		
		x = AtARegInvAtb; // store in member variable.
		
		return luOfAtAReg.isInvertible();
	};

	// Return the normalised least square residuals?
	// $\frac{\|\mathbf{x}_k-\mathbf{x^*}\|}{\|\mathbf{x^*}\|}$
	// $k$ is the current regressor step, i.e. this regressor only returns the residual for this step
	double test(cv::Mat data, cv::Mat labels)
	{
		cv::Mat predictions;
		for (int i = 0; i < data.rows; ++i) {
			cv::Mat prediction = predict(data.row(i));
			predictions.push_back(prediction);
		}
		return cv::norm(predictions, labels, cv::NORM_L2) / cv::norm(labels, cv::NORM_L2);
	};

	// values has to be a row vector. One row for every data point.
	// The input to this function is only 1 data point, i.e. one row-vector.
	cv::Mat predict(cv::Mat values)
	{
		cv::Mat prediction = values * x;
		return prediction;
	};

	float predict(float value)
	{
		if (x.rows != 1 || x.cols != 1) {
			throw std::runtime_error("Trying to predict a float value, but the used regressor doesn't have exactly one row and one column.");
		}
		float prediction = value * x.at<float>(0);
		return prediction;
	};
	
	cv::Mat x; // move back to private once finished
	// could rename to 'R' (as used in the SDM)
private:
	Regulariser regulariser;

	// Add serialisation
};

// or... LossFunction, CostFunction, Evaluator, ...?
// No inheritance, so the user can plug in anything without inheriting.
class TrivialEvaluationFunction
{

};

inline void noEval(const cv::Mat& currentPredictions) // do nothing
{
};

// template blabla.. requires Evaluator with function eval(Mat, blabla)
// or... Learner? SDMLearner? Just SDM? SupervDescOpt? SDMTraining?
// This at the moment handles the case of _known_ y (target) values.
// Requirements on RegressorType: See Regressor abstract class
// Requirements on UpdateStrategy: A function Mat(...Mat.. Regressor...);
//  for one sample. Also possibly (if a performance problem) an overload
//  for several samples at once.
template<class RegressorType, class UpdateStrategy=void>
class SupervisedDescentOptimiser
{
public:
	SupervisedDescentOptimiser(std::vector<RegressorType> regressors) : regressors(regressors)
	{
	};


	// Hmm in case of LM / 3DMM, we might need to give more info than this? How to handle that?
	// Yea, some optimisers need access to the image data to optimise. Think about where this belongs.
	// The input to this will be something different (e.g. only images? a templated class?)
	// OnTrainingEpochCallback will get called
	// The callback functions signature should look like:
	//
	// $\mathbf{H}$ a function, should return a cv::Mat row-vector (1 row). Should process 1 training data. Optimally, whatever it is - float or a whole cv::Mat. But we can also simulate the 1D-case with a 1x1 cv::Mat for now.
	template<class H, class OnTrainingEpochCallback>
	void train(cv::Mat x, cv::Mat y, cv::Mat x0, H h, OnTrainingEpochCallback onTrainingEpochCallback)
	{
		// data = x = the parameters we want to learn, ground truth labels for them = y. x0 = c = initialisation
		// Simple experiments with sin etc.
		auto logger = Loggers->getLogger("superviseddescent");
		Mat currentX = x0;
		for (auto&& regressor : regressors) {
			// 1) Extract features where necessary
			Mat features;
			for (int i = 0; i < currentX.rows; ++i) {
				features.push_back(h(currentX.row(i)));
			}
			Mat insideRegressor = features - y; // Todo: Find better name ;-)
			// We got $\sum\|x_*^i - x_k^i + R_k(h(x_k^i)-y^i)\|_2^2 $
			// That is: $-b + Ax = 0$, $Ax = b$, $x = A^-1 * b$
			// Thus: $b = x_k^i - x_*^i$. 
			// This is correct! It's the other way round than in the CVPR 2013 paper though. However,
			// it cancels out, as we later subtract the learned direction, while in the 2013 paper they add it.
			Mat b = currentX - x; // correct
			
			// 2) Learn using that data
			regressor.learn(insideRegressor, b);
			// 3) If not finished, 
			//    apply learned regressor, set data = newData or rather calculate new delta
			//    use it to learn the next regressor in next loop iter
			
			// If it's a performance problem, we should do R * featureMatrix only once, for all examples at the same time.
			// Note/Todo: Why does the old code have 'shapeStep = featureMatrix * R' the other way round?
			Mat x_k; // x_k = currentX - R * (h(currentX) - y):
			for (int i = 0; i < currentX.rows; ++i) {
				// No need to re-extract the features, we already did so in step 1)
				x_k.push_back(Mat(currentX.row(i) - regressor.predict(insideRegressor.row(i)))); // minus here is correct. CVPR paper got a '+'. See above for reason why.
			}
			currentX = x_k;
			onTrainingEpochCallback(currentX);
		}
	};

	template<class H>
	void train(cv::Mat x, cv::Mat y, cv::Mat x0, H h)
	{
		return train(x, y, x0, h, noEval);
	};

	// Returns the final prediction
	template<class H, class OnRegressorIterationCallback>
	cv::Mat test(cv::Mat y, cv::Mat x0, H h, OnRegressorIterationCallback onRegressorIterationCallback)
	{
		Mat currentX = x0;
		for (auto&& regressor : regressors) {
			Mat features;
			for (int i = 0; i < currentX.rows; ++i) {
				features.push_back(h(currentX.row(i)));
			}
			Mat insideRegressor = features - y; // Todo: Find better name ;-)
			Mat x_k; // (currentX.rows, currentX.cols, currentX.type())
			// For every training example:
			// This calculates x_k = currentX - R * (h(currentX) - y):
			for (int i = 0; i < currentX.rows; ++i) {
				Mat myInside = h(currentX.row(i)) - y.row(i);
				x_k.push_back(Mat(currentX.row(i) - regressor.predict(myInside))); // we need Mat() because the subtraction yields a (non-persistent) MatExpr
			}
			currentX = x_k;
			onRegressorIterationCallback(currentX);
		}
		return currentX; // Return the final predictions
	};

	template<class H>
	cv::Mat test(cv::Mat y, cv::Mat x0, H h)
	{
		return test(y, x0, h, noEval);
	};

	// Predicts the result value for a single example, using all regressors.
	// For the case where we have a template y (known y).
	template<class H>
	cv::Mat predict(cv::Mat x0, cv::Mat template_y, H h)
	{
		Mat currentX = x0;
		for (auto&& regressor : regressors) {
			// calculate x_k = currentX - R * (h(currentX) - y):
			cv::Mat myInside = h(currentX) - y;
			Mat x_k = currentX - regressor.predict(myInside);
			currentX = x_k;
		}
		return currentX;
	};

private:
	// Note/Todo:
	// Maybe we want a private function:
	// getNextX(currentX, h, y, regressor); to eliminate duplicate code in predict() and test().
	// Then, we could make an overload without y.
	// However, it wouldn't solve the problem of with/without y at the learning stage.

	std::vector<RegressorType> regressors;

	//UpdateStrategy updateStrategy;
};


/**
 * This implements ... todo.
 *
 * Usage example:
 * @code
 * auto testexp = [](float value) { return std::exp(value); };
 * auto testinv = [](float value) { return 1.0f / value; };
 * auto testpow = [](float value) { return std::pow(value, 2); };
 * float r_exp = 0.115f;
 * float r_inv = -7.0f;
 * float r_pow = 0.221;
 *
 * int dims = 31; Mat x_0_tr(dims, 1, CV_32FC1); // exp: [-2:0.2:4]
 * //int dims = 26; Mat x_0_tr(dims, 1, CV_32FC1); // inv: [1:0.2:6]
 * //int dims = 31; Mat x_0_tr(dims, 1, CV_32FC1); // pow: [0:0.2:6]
 * {
 *	vector<float> values(dims);
 * 	strided_iota(std::begin(values), std::next(std::begin(values), dims), -2.0f, 0.2f); // exp
 * 	//strided_iota(std::begin(values), std::next(std::begin(values), dims), 1.0f, 0.2f); // inv
 * 	//strided_iota(std::begin(values), std::next(std::begin(values), dims), 0.0f, 0.2f); // pow
 * 	x_0_tr = Mat(values, true);
 * }
 * Mat y_tr(31, 1, CV_32FC1); // exp: all = exp(1) = 2.7...;
 * //Mat y_tr(dims, 1, CV_32FC1); // inv: all = 0.286;
 * //Mat y_tr(31, 1, CV_32FC1); // pow: all = 9;
 * {
 * 	vector<float> values(dims);
 * 	std::fill(begin(values), end(values), std::exp(1.0f));
 * 	y_tr = Mat(values, true);
 * }
 *
 * v2::GenericDM1D g;
 * g.train(x_0_tr, y_tr, r_exp, testexp);
 * @endcode
 */
class GenericDM1D
{
public:
	template<typename H>
	void train(cv::Mat data, cv::Mat labels, float genericDescentMap, H h)
	{
		auto logger = Loggers->getLogger("superviseddescent");

		logger.info("r is: " + std::to_string(genericDescentMap));
		
		// For all training example, do the update step for 25 iterations.
		cv::Mat x_0 = data;
		cv::Mat x_prev = x_0;
		std::stringstream out;
		out << x_0 << std::endl;
		logger.info(out.str());
		for (int i = 1; i <= 25; ++i) {
			// Update x_0, the starting positions, using the learned regressor:
			//float x_next_0 = x_prev_0 - r.x.at<float>(0, 0) * (h(x_prev_0) - (labels.at<float>(0)));
			cv::Mat x_next(x_prev.rows, 1, CV_32FC1);
			for (int i = 0; i < x_prev.rows; ++i) {
				auto thisDelta = h(x_prev.at<float>(i)) - labels.at<float>(i);
				auto thisDeltaRegressed = genericDescentMap * thisDelta;
				x_next.at<float>(i) = x_prev.at<float>(i) -thisDeltaRegressed;
			}
			std::stringstream outIter;
			outIter << x_next << std::endl;
			logger.info(outIter.str());
			x_prev = x_next;
		}
	};
};

	} /* namespace v2 */
} /* namespace superviseddescent */
#endif /* SUPERVISEDDESCENT_HPP_ */