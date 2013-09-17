/*
 * LibSvmUtils.hpp
 *
 *  Created on: 12.09.2013
 *      Author: poschmann
 */

#ifndef LIBSVMUTILS_HPP_
#define LIBSVMUTILS_HPP_

#include "opencv2/core/core.hpp"
#include <unordered_map>
#include <memory>
#include <vector>

using cv::Mat;
using std::unordered_map;
using std::unique_ptr;
using std::vector;

struct svm_node;
struct svm_parameter;
struct svm_problem;
struct svm_model;

namespace classification {

/**
 * Deleter of libSVM nodes also removes the cv::Mat representation of that node within a map.
 */
class NodeDeleter {
public:
	/**
	 * Constructs a new node deleter.
	 *
	 * @param[in] map The map the nodes should be removed from on deletion.
	 */
	NodeDeleter(unordered_map<const struct svm_node*, Mat>& map);
	NodeDeleter(const NodeDeleter& other);
	NodeDeleter& operator=(const NodeDeleter& other);
	void operator()(struct svm_node *node) const;
private:
	unordered_map<const struct svm_node*, Mat>& map; ///< The map the nodes should be removed from on deletion.
};

/**
 * Deleter of the libSVM parameter.
 */
class ParameterDeleter {
public:
	void operator()(struct svm_parameter *param) const;
};

/**
 * Deleter of the libSVM problem.
 */
class ProblemDeleter {
public:
	void operator()(struct svm_problem *problem) const;
};

/**
 * Deleter of the libSVM model.
 */
class ModelDeleter {
public:
	void operator()(struct svm_model *model) const;
};

/**
 * Utility class for libSVM with functions for creating nodes and computing SVM outputs. Usable via
 * composition or inheritance.
 */
class LibSvmUtils {
public:

	LibSvmUtils();

	virtual ~LibSvmUtils();

	/**
	 * @return Deleter of libSVM nodes that removes the node from the map.
	 */
	NodeDeleter getNodeDeleter() const;

	/**
	 * Creates a new libSVM node from the given feature vector data. Will store the vector in a map for later retrieval.
	 * The vectors is removed from the map on node deletion.
	 *
	 * @param[in] vector The feature vector.
	 * @return The newly created libSVM node.
	 */
	unique_ptr<struct svm_node[], NodeDeleter> createNode(const Mat& vector) const;

	/**
	 * Retrieves the vector to the given libSVM node. Will have a look into the map of stored vectors first. If the vector
	 * is not contained within the map, it is created and stored within the map. Therefore this function should only be
	 * called with nodes that reside within a unique_ptr with a NodeDeleter to ensure that vectors are cleaned up.
	 *
	 * @param[in] node The libSVM node.
	 * @return The feature vector.
	 */
	Mat getVector(const struct svm_node *node) const;

	/**
	 * Computes the SVM output given a libSVM node.
	 *
	 * @param[in] model The libSVM model.
	 * @param[in] x The libSVM node.
	 * @return The SVM output value.
	 */
	double computeSvmOutput(struct svm_model *model, const struct svm_node *x) const;

	/**
	 * Extracts the support vectors from a libSVM model.
	 *
	 * @param[in] model The libSVM model.
	 * @return The support vectors.
	 */
	vector<Mat> extractSupportVectors(struct svm_model *model) const;

	/**
	 * Extracts the coefficients from a libSVM model.
	 *
	 * @param[in] model The libSVM model.
	 * @return The coefficients.
	 */
	vector<float> extractCoefficients(struct svm_model *model) const;

	/**
	 * Extracts the bias from a libSVM model.
	 *
	 * @param[in] model The libSVM model.
	 * @return The bias.
	 */
	double extractBias(struct svm_model *model) const;

private:

	/**
	 * Fills a libSVM node with the data of a feature vector.
	 *
	 * @param[in,out] node The libSVM node.
	 * @param[in] vector The feature vector.
	 * @param[in] size The size of the data.
	 */
	template<class T>
	void fillNode(struct svm_node *node, const Mat& vector, int size) const;

	/**
	 * Fills a vector with the data of a libSVM node.
	 *
	 * @param[in,out] vector The vector.
	 * @param[in] node The libSVM node.
	 * @param[in] size The size of the data.
	 */
	template<class T>
	void fillMat(Mat& vector, const struct svm_node *node, int size) const;

	mutable int matType; ///< The type of the support vector data.
	mutable int dimensions; ///< The amount of dimensions of the feature vectors.
	mutable unordered_map<const struct svm_node*, Mat> node2example; ///< Maps libSVM nodes to the training examples they were created with.
	NodeDeleter nodeDeleter; ///< Deleter of libSVM nodes that removes the node from the map.
};

} /* namespace classification */


#endif /* LIBSVMUTILS_HPP_ */
